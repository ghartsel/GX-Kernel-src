/************************************BEGIN*****************************************
*                       COPYRIGHT %G% %U% BY GHWORKS, LLC 
*                             ALL RIGHTS RESERVED
*                         SPDX-License-Identifier: MIT
* ********************************************************************************
* Name:        timer.c
* Type:        C Source
* Description: Timer Services Implementation
*
* Interface (public) Routines:
*   tm_cancel    - Cancel a timer
*   tm_evafter   - Set event after N ticks
*   tm_evevery   - Set recurring event every N ticks
*   tm_evwhen    - Set event at specific time
*   tm_get       - Get current system time
*   tm_set       - Set system time
*   tm_tick      - Process timer tick (called from interrupt)
*   tm_wkafter   - Wake task after N ticks
*   tm_wkwhen    - Wake task at specific time
*
* Private Functions:
*   Timer pool management
*   Timer list management
*   Time conversion utilities
*   Timer processing and scheduling
*
**************************************END***************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../gxkernel.h"
#include "../../gxkCfg.h"
#include "timer.h"

/* Global timer state */
TM_STATE tm_global_state = {0};

/* Forward declarations for internal functions */
static ULONG tm_init_once(void);
static ULONG tm_validate_inputs(ULONG ticks, ULONG events);
static ULONG tm_create_timer(ULONG type, ULONG action, ULONG ticks, 
                           ULONG events, ULONG date, ULONG time, ULONG *tmid);

/******************************************************************************
* Timer Pool Management Functions
******************************************************************************/

ULONG tm_pool_init(void)
{
    TM_POOL *pool = TM_GET_POOL();
    int i;
    
    memset(pool, 0, sizeof(TM_POOL));
    
    pool->magic = TM_POOL_MAGIC;
    pool->max_timers = TM_MAX_TIMERS;
    pool->free_count = TM_MAX_TIMERS;
    pool->next_id = 1;
    
    /* Initialize free list */
    for (i = 0; i < TM_MAX_TIMERS - 1; i++) {
        pool->timers[i].next = &pool->timers[i + 1];
        pool->timers[i].timer_id = 0;
        pool->timers[i].state = TM_STATE_FREE;
    }
    pool->timers[TM_MAX_TIMERS - 1].next = NULL;
    pool->free_list = &pool->timers[0];
    pool->active_list = NULL;
    
    return 0;
}

TM_TCB *tm_pool_alloc(void)
{
    TM_POOL *pool = TM_GET_POOL();
    TM_TCB *tcb;
    
    if (pool->free_count == 0 || pool->free_list == NULL) {
        return NULL;
    }
    
    tcb = pool->free_list;
    pool->free_list = tcb->next;
    pool->free_count--;
    
    memset(tcb, 0, sizeof(TM_TCB));
    tcb->magic = TM_POOL_MAGIC;
    tcb->timer_id = tm_generate_id();
    tcb->state = TM_STATE_ACTIVE;
    
    return tcb;
}

ULONG tm_pool_free(TM_TCB *tcb)
{
    TM_POOL *pool = TM_GET_POOL();
    
    if (!TM_IS_VALID_TCB(tcb)) {
        return ERR_BADTCB;
    }
    
    /* Remove from active list if present */
    tm_list_remove(tcb);
    
    /* Clear the TCB */
    memset(tcb, 0, sizeof(TM_TCB));
    tcb->state = TM_STATE_FREE;
    
    /* Add to free list */
    tcb->next = pool->free_list;
    pool->free_list = tcb;
    pool->free_count++;
    
    return 0;
}

TM_TCB *tm_pool_find(ULONG timer_id)
{
    TM_POOL *pool = TM_GET_POOL();
    TM_TCB *tcb;
    
    if (timer_id == TM_INVALID_ID) {
        return NULL;
    }
    
    /* Search active list */
    tcb = pool->active_list;
    while (tcb != NULL) {
        if (tcb->timer_id == timer_id && TM_IS_VALID_TCB(tcb)) {
            return tcb;
        }
        tcb = tcb->next;
    }
    
    return NULL;
}

/******************************************************************************
* Timer List Management Functions
******************************************************************************/

void tm_list_insert(TM_TCB *tcb)
{
    TM_POOL *pool = TM_GET_POOL();
    TM_TCB *current, *prev;
    
    if (!TM_IS_VALID_TCB(tcb)) {
        return;
    }
    
    /* Insert in sorted order by expiration time */
    prev = NULL;
    current = pool->active_list;
    
    while (current != NULL && current->expire_ticks <= tcb->expire_ticks) {
        prev = current;
        current = current->next;
    }
    
    tcb->next = current;
    tcb->prev = prev;
    
    if (prev != NULL) {
        prev->next = tcb;
    } else {
        pool->active_list = tcb;
    }
    
    if (current != NULL) {
        current->prev = tcb;
    }
}

void tm_list_remove(TM_TCB *tcb)
{
    TM_POOL *pool = TM_GET_POOL();
    
    if (!TM_IS_VALID_TCB(tcb)) {
        return;
    }
    
    if (tcb->prev != NULL) {
        tcb->prev->next = tcb->next;
    } else {
        pool->active_list = tcb->next;
    }
    
    if (tcb->next != NULL) {
        tcb->next->prev = tcb->prev;
    }
    
    tcb->next = NULL;
    tcb->prev = NULL;
}

TM_TCB *tm_list_get_expired(void)
{
    TM_POOL *pool = TM_GET_POOL();
    TM_TCB *tcb = pool->active_list;
    ULONG current_ticks = TM_GET_CURRENT_TICKS();
    
    if (tcb != NULL && tcb->expire_ticks <= current_ticks) {
        return tcb;
    }
    
    return NULL;
}

/******************************************************************************
* Time Management Functions
******************************************************************************/

ULONG tm_time_init(void)
{
    TM_SYSTIME *systime = TM_GET_SYSTIME();
    
    systime->date = 0x07e90101;     /* January 1, 2025 */
    systime->time = 0x00000000;     /* 00:00:00 */
    systime->ticks = 0;
    systime->tick_count = 0;
    systime->ticks_per_sec = TM_TICKS_PER_SEC;
    
    return 0;
}

ULONG tm_time_update(void)
{
    TM_SYSTIME *systime = TM_GET_SYSTIME();
    
    systime->tick_count++;
    systime->ticks++;
    
    if (systime->ticks >= systime->ticks_per_sec) {
        systime->ticks = 0;
        /* Increment time by 1 second */
        systime->time++;
        
        /* Handle time overflow (24-hour rollover) */
        if (systime->time >= 0x00181818) {  /* 24:00:00 */
            systime->time = 0;
            systime->date++;
        }
    }
    
    return 0;
}

ULONG tm_time_to_ticks(ULONG date, ULONG time, ULONG ticks)
{
    /* Simplified conversion - in real implementation would handle 
       full date/time arithmetic */
    TM_SYSTIME *systime = TM_GET_SYSTIME();
    ULONG current_ticks = systime->tick_count;
    ULONG target_ticks;
    
    /* For now, assume target is in the future and convert relative */
    if (date > systime->date || 
        (date == systime->date && time > systime->time) ||
        (date == systime->date && time == systime->time && ticks > systime->ticks)) {
        /* Future time - calculate difference */
        ULONG time_diff = (time - systime->time) * systime->ticks_per_sec;
        target_ticks = current_ticks + time_diff + (ticks - systime->ticks);
    } else {
        /* Past or current time */
        target_ticks = current_ticks;
    }
    
    return target_ticks;
}

void tm_ticks_to_time(ULONG tick_count, ULONG *date, ULONG *time, ULONG *ticks)
{
    TM_SYSTIME *systime = TM_GET_SYSTIME();
    ULONG tick_diff = tick_count - systime->tick_count;
    ULONG seconds = tick_diff / systime->ticks_per_sec;
    
    *date = systime->date;
    *time = systime->time + seconds;
    *ticks = systime->ticks + (tick_diff % systime->ticks_per_sec);
    
    if (*ticks >= systime->ticks_per_sec) {
        *ticks -= systime->ticks_per_sec;
        (*time)++;
    }
    
    /* Handle time overflow */
    if (*time >= 0x00181818) {
        *time -= 0x00181818;
        (*date)++;
    }
}

/******************************************************************************
* Timer Processing Functions
******************************************************************************/

ULONG tm_process_expired(void)
{
    TM_TCB *tcb;
    ULONG processed = 0;
    
    while ((tcb = tm_list_get_expired()) != NULL) {
        tm_fire_timer(tcb);
        processed++;
        
        /* Handle periodic timers */
        if (tcb->type == TM_TYPE_PERIODIC) {
            tcb->expire_ticks += tcb->period_ticks;
            /* Reinsert in sorted order */
            tm_list_remove(tcb);
            tm_list_insert(tcb);
        } else {
            /* One-shot timer - remove and free */
            tm_pool_free(tcb);
        }
    }
    
    return processed;
}

ULONG tm_schedule_next(void)
{
    TM_STATE *state = TM_GET_STATE();
    TM_POOL *pool = TM_GET_POOL();
    
    if (state->hw_ops && state->hw_ops->set_alarm && pool->active_list) {
        return state->hw_ops->set_alarm(pool->active_list->expire_ticks);
    }
    
    return 0;
}

ULONG tm_fire_timer(TM_TCB *tcb)
{
    if (!TM_IS_VALID_TCB(tcb)) {
        return ERR_BADTCB;
    }
    
    tcb->state = TM_STATE_EXPIRED;
    
    switch (tcb->action) {
        case TM_ACTION_EVENT:
            /* Send events to task */
            if (tcb->events != 0) {
                ev_send(tcb->task_id, tcb->events);
            }
            break;
            
        case TM_ACTION_WAKEUP:
            /* Wake up sleeping task */
            t_resume(tcb->task_id);
            break;
            
        default:
            TM_DEBUG_PRINT("Unknown timer action: %lu", tcb->action);
            break;
    }
    
    return 0;
}

/******************************************************************************
* Utility Functions
******************************************************************************/

ULONG tm_validate_tcb(TM_TCB *tcb)
{
    return TM_IS_VALID_TCB(tcb) ? 0 : ERR_BADTCB;
}

ULONG tm_generate_id(void)
{
    TM_POOL *pool = TM_GET_POOL();
    ULONG id = pool->next_id++;
    
    if (pool->next_id == 0) {
        pool->next_id = 1;  /* Wrap around, skip 0 */
    }
    
    return id;
}

static ULONG tm_validate_inputs(ULONG ticks, ULONG events)
{
    if (ticks == 0) {
        return ERR_ILLTICKS;
    }
    
    return 0;
}

static ULONG tm_init_once(void)
{
    TM_STATE *state = TM_GET_STATE();
    
    if (state->initialized) {
        return 0;
    }
    
    /* Initialize timer pool */
    tm_pool_init();
    
    /* Initialize system time */
    tm_time_init();
    
    /* Initialize hardware abstraction */
    tm_hw_init();
    
    state->initialized = 1;
    
    return 0;
}

static ULONG tm_create_timer(ULONG type, ULONG action, ULONG ticks, 
                           ULONG events, ULONG date, ULONG time, ULONG *tmid)
{
    TM_TCB *tcb;
    ULONG error;
    
    error = tm_init_once();
    if (error != 0) {
        return error;
    }
    
    if (tmid == NULL) {
        return ERR_BADPARAM;
    }
    
    tcb = tm_pool_alloc();
    if (tcb == NULL) {
        return ERR_NOTIMERS;
    }
    
    tcb->type = type;
    tcb->action = action;
    tcb->events = events;
    tcb->task_id = t_ident();  /* Current task ID */
    tcb->start_ticks = TM_GET_CURRENT_TICKS();
    
    switch (type) {
        case TM_TYPE_ONESHOT:
        case TM_TYPE_PERIODIC:
            tcb->delay_ticks = ticks;
            tcb->expire_ticks = tcb->start_ticks + ticks;
            if (type == TM_TYPE_PERIODIC) {
                tcb->period_ticks = ticks;
            }
            break;
            
        case TM_TYPE_ABSOLUTE:
            tcb->target_date = date;
            tcb->target_time = time;
            tcb->target_tick = ticks;
            tcb->expire_ticks = tm_time_to_ticks(date, time, ticks);
            break;
            
        default:
            tm_pool_free(tcb);
            return ERR_BADPARAM;
    }
    
    /* Insert into active list */
    tm_list_insert(tcb);
    
    /* Schedule next alarm */
    tm_schedule_next();
    
    *tmid = tcb->timer_id;
    
    return 0;
}

/******************************************************************************
* Public API Functions
******************************************************************************/

ULONG tm_cancel(ULONG tmid)
{
    TM_TCB *tcb;
    
    tm_init_once();
    
    tcb = tm_pool_find(tmid);
    if (tcb == NULL) {
        return ERR_BADTMID;
    }
    
    tcb->state = TM_STATE_CANCELLED;
    tm_pool_free(tcb);
    
    /* Reschedule next alarm */
    tm_schedule_next();
    
    return 0;
}

ULONG tm_evafter(ULONG ticks, ULONG events, ULONG *tmid)
{
    ULONG error = tm_validate_inputs(ticks, events);
    if (error != 0) {
        return error;
    }
    
    return tm_create_timer(TM_TYPE_ONESHOT, TM_ACTION_EVENT, ticks, 
                          events, 0, 0, tmid);
}

ULONG tm_evevery(ULONG ticks, ULONG events, ULONG *tmid)
{
    ULONG error = tm_validate_inputs(ticks, events);
    if (error != 0) {
        return error;
    }
    
    return tm_create_timer(TM_TYPE_PERIODIC, TM_ACTION_EVENT, ticks, 
                          events, 0, 0, tmid);
}

ULONG tm_evwhen(ULONG date, ULONG time, ULONG ticks, ULONG events, ULONG *tmid)
{
    if (events == 0) {
        return ERR_BADPARAM;
    }
    
    return tm_create_timer(TM_TYPE_ABSOLUTE, TM_ACTION_EVENT, ticks, 
                          events, date, time, tmid);
}

ULONG tm_get(ULONG *date, ULONG *time, ULONG *ticks)
{
    TM_SYSTIME *systime;
    
    tm_init_once();
    
    if (date == NULL || time == NULL || ticks == NULL) {
        return ERR_BADPARAM;
    }
    
    systime = TM_GET_SYSTIME();
    
    *date = systime->date;
    *time = systime->time;
    *ticks = systime->ticks;
    
    return 0;
}

ULONG tm_set(ULONG date, ULONG time, ULONG ticks)
{
    TM_SYSTIME *systime;
    
    tm_init_once();
    
    systime = TM_GET_SYSTIME();
    
    systime->date = date;
    systime->time = time;
    systime->ticks = ticks;
    
    return 0;
}

ULONG tm_tick(void)
{
    TM_STATE *state = TM_GET_STATE();
    
    if (!state->initialized) {
        return 0;
    }
    
    state->interrupt_count++;
    
    /* Update system time */
    tm_time_update();
    
    /* Process expired timers */
    tm_process_expired();
    
    /* Schedule next hardware alarm */
    tm_schedule_next();
    
    return 0;
}

ULONG tm_wkafter(ULONG ticks)
{
    ULONG tmid;
    ULONG error;
    
    if (ticks == 0) {
        return ERR_ILLTICKS;
    }
    
    error = tm_create_timer(TM_TYPE_ONESHOT, TM_ACTION_WAKEUP, ticks, 
                           0, 0, 0, &tmid);
    if (error != 0) {
        return error;
    }
    
    /* Suspend current task - it will be resumed when timer expires */
    return t_suspend(t_ident());
}

ULONG tm_wkwhen(ULONG date, ULONG time, ULONG ticks)
{
    ULONG tmid;
    ULONG error;
    
    error = tm_create_timer(TM_TYPE_ABSOLUTE, TM_ACTION_WAKEUP, ticks, 
                           0, date, time, &tmid);
    if (error != 0) {
        return error;
    }
    
    /* Suspend current task - it will be resumed when timer expires */
    return t_suspend(t_ident());
}