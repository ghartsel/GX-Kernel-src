/************************************BEGIN*****************************************
*                       COPYRIGHT %G% %U% BY GHWORKS, LLC 
*                             ALL RIGHTS RESERVED
*                         SPDX-License-Identifier: MIT
* ********************************************************************************
* Name:        event.c
* Type:        C Source
* Description: Event Services Implementation
*
* Interface (public) Routines:
*   ev_receive   - Wait for and receive events
*   ev_send      - Send events to a task
*
* Private Functions:
*   Event pool management
*   Event condition checking
*   Wait management
*   Hardware abstraction
*   Statistics tracking
*
**************************************END***************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../gxkernel.h"
#include "../../gxkCfg.h"
#include "event.h"

/* Global event state */
EV_STATE ev_global_state = {0};

/* Forward declarations for internal functions */
static ULONG ev_init_once(void);
static ULONG ev_get_current_time(void);

/******************************************************************************
* Event Pool Management Functions
******************************************************************************/

ULONG ev_pool_init(void)
{
    EV_POOL *pool = EV_GET_POOL();
    int i;
    
    memset(pool, 0, sizeof(EV_POOL));
    
    pool->magic = EV_POOL_MAGIC;
    pool->max_tasks = MAX_TASK;
    pool->active_count = 0;
    
    /* Initialize event control blocks (one per task) */
    for (i = 0; i < MAX_TASK; i++) {
        EV_ECB *ecb = &pool->event_blocks[i];
        memset(ecb, 0, sizeof(EV_ECB));
        ecb->magic = EV_POOL_MAGIC;
        ecb->task_id = i;
        ecb->state = EV_STATE_FREE;
    }
    
    return 0;
}

EV_ECB *ev_pool_get_task_ecb(ULONG task_id)
{
    EV_POOL *pool = EV_GET_POOL();
    
    if (!EV_IS_VALID_TASK_ID(task_id)) {
        return NULL;
    }
    
    return &pool->event_blocks[task_id];
}

ULONG ev_pool_validate_task_id(ULONG task_id)
{
    return EV_IS_VALID_TASK_ID(task_id) ? 0 : ERR_OBJID;
}

/******************************************************************************
* Event Processing Functions
******************************************************************************/

ULONG ev_check_conditions(EV_ECB *ecb)
{
    ULONG pending = ecb->pending_events;
    ULONG waiting = ecb->waiting_events;
    
    if (!EV_IS_VALID_ECB(ecb) || ecb->state != EV_STATE_WAITING) {
        return 0;
    }
    
    if (ecb->wait_condition == EV_ALL) {
        /* Wait for ALL specified events */
        return EV_CHECK_ALL_CONDITION(pending, waiting);
    } else {
        /* Wait for ANY specified events */
        return EV_CHECK_ANY_CONDITION(pending, waiting);
    }
}

ULONG ev_update_pending(EV_ECB *ecb, ULONG new_events)
{
    if (!EV_IS_VALID_ECB(ecb)) {
        return ERR_BADPARAM;
    }
    
    EV_SET_EVENTS(ecb->pending_events, new_events);
    ev_update_statistics(ecb, EV_STAT_SEND);
    
    return 0;
}

ULONG ev_clear_received(EV_ECB *ecb, ULONG events_to_clear)
{
    if (!EV_IS_VALID_ECB(ecb)) {
        return ERR_BADPARAM;
    }
    
    EV_CLEAR_EVENTS(ecb->pending_events, events_to_clear);
    ecb->received_events = events_to_clear;
    
    return 0;
}

ULONG ev_timeout_expired(EV_ECB *ecb)
{
    ULONG current_time;
    ULONG elapsed_time;
    
    if (!EV_IS_VALID_ECB(ecb) || ecb->timeout_ticks == EV_INFINITE_TIMEOUT) {
        return 0;
    }
    
    current_time = ev_get_current_time();
    elapsed_time = current_time - ecb->wait_start_time;
    
    return (elapsed_time >= ecb->timeout_ticks) ? 1 : 0;
}

/******************************************************************************
* Wait Management Functions
******************************************************************************/

ULONG ev_start_wait(EV_ECB *ecb, ULONG events, ULONG flags, ULONG timeout)
{
    ULONG error;
    
    if (!EV_IS_VALID_ECB(ecb)) {
        return ERR_BADPARAM;
    }
    
    /* Set wait parameters */
    ecb->waiting_events = events;
    ecb->wait_condition = (flags & EV_ANY) ? EV_ANY : EV_ALL;
    ecb->wait_flags = flags;
    ecb->timeout_ticks = timeout;
    ecb->wait_start_time = ev_get_current_time();
    ecb->state = EV_STATE_WAITING;
    
    /* Create hardware event object if needed */
    EV_STATE *state = EV_GET_STATE();
    if (state->hw_ops && state->hw_ops->create_event) {
        error = state->hw_ops->create_event(ecb);
        if (error != 0) {
            return error;
        }
    }
    
    ev_update_statistics(ecb, EV_STAT_WAIT_START);
    
    return 0;
}

ULONG ev_complete_wait(EV_ECB *ecb, ULONG *events_received)
{
    if (!EV_IS_VALID_ECB(ecb) || events_received == NULL) {
        return ERR_BADPARAM;
    }
    
    *events_received = ecb->received_events;
    
    /* Clear wait state */
    ecb->waiting_events = 0;
    ecb->wait_condition = 0;
    ecb->wait_flags = 0;
    ecb->timeout_ticks = 0;
    ecb->state = EV_STATE_FREE;
    
    ev_update_statistics(ecb, EV_STAT_WAIT_COMPLETE);
    
    return 0;
}

ULONG ev_cancel_wait(EV_ECB *ecb)
{
    EV_STATE *state = EV_GET_STATE();
    
    if (!EV_IS_VALID_ECB(ecb)) {
        return ERR_BADPARAM;
    }
    
    /* Clear hardware event object */
    if (state->hw_ops && state->hw_ops->clear_event) {
        state->hw_ops->clear_event(ecb);
    }
    
    /* Reset wait state */
    ecb->waiting_events = 0;
    ecb->wait_condition = 0;
    ecb->wait_flags = 0;
    ecb->timeout_ticks = 0;
    ecb->state = EV_STATE_FREE;
    
    return 0;
}

/******************************************************************************
* Utility Functions
******************************************************************************/

ULONG ev_validate_ecb(EV_ECB *ecb)
{
    return EV_IS_VALID_ECB(ecb) ? 0 : ERR_BADPARAM;
}

ULONG ev_validate_events(ULONG events)
{
    return EV_IS_VALID_EVENT_MASK(events) ? 0 : ERR_BADPARAM;
}

ULONG ev_validate_flags(ULONG flags)
{
    return EV_IS_VALID_FLAGS(flags) ? 0 : ERR_BADPARAM;
}

ULONG ev_get_current_task_id(void)
{
    /* This would integrate with the task system to get current task ID */
    /* For now, return a placeholder */
    return 0;
}

static ULONG ev_get_current_time(void)
{
    /* This would integrate with the timer system to get current tick count */
    /* For now, return a placeholder */
    return 0;
}

void ev_update_statistics(EV_ECB *ecb, ULONG operation)
{
    EV_STATE *state = EV_GET_STATE();
    
    if (!EV_IS_VALID_ECB(ecb)) {
        return;
    }
    
    switch (operation) {
        case EV_STAT_SEND:
            ecb->events_sent++;
            state->total_events_sent++;
            break;
            
        case EV_STAT_RECEIVE:
            ecb->events_received++;
            state->total_events_received++;
            break;
            
        case EV_STAT_WAIT_START:
            ecb->wait_count++;
            break;
            
        case EV_STAT_TIMEOUT:
            ecb->timeout_count++;
            break;
            
        default:
            break;
    }
}

ULONG ev_get_statistics(ULONG task_id, ULONG *sent, ULONG *received, 
                       ULONG *waits, ULONG *timeouts)
{
    EV_ECB *ecb = ev_pool_get_task_ecb(task_id);
    
    if (ecb == NULL) {
        return ERR_OBJID;
    }
    
    if (sent) *sent = ecb->events_sent;
    if (received) *received = ecb->events_received;
    if (waits) *waits = ecb->wait_count;
    if (timeouts) *timeouts = ecb->timeout_count;
    
    return 0;
}

static ULONG ev_init_once(void)
{
    EV_STATE *state = EV_GET_STATE();
    
    if (state->initialized) {
        return 0;
    }
    
    /* Initialize event pool */
    ev_pool_init();
    
    /* Initialize hardware abstraction */
    ev_hw_init();
    
    state->total_events_sent = 0;
    state->total_events_received = 0;
    state->initialized = 1;
    
    return 0;
}

/******************************************************************************
* Public API Functions
******************************************************************************/

ULONG ev_receive(ULONG events, ULONG flags, ULONG timeout, ULONG *events_r)
{
    EV_STATE *state = EV_GET_STATE();
    EV_ECB *ecb;
    ULONG task_id;
    ULONG error;
    ULONG events_satisfied;
    
    error = ev_init_once();
    if (error != 0) {
        return error;
    }
    
    if (events_r == NULL) {
        return ERR_BADPARAM;
    }
    
    error = ev_validate_events(events);
    if (error != 0) {
        return error;
    }
    
    error = ev_validate_flags(flags);
    if (error != 0) {
        return error;
    }
    
    /* Get current task ID */
    task_id = ev_get_current_task_id();
    ecb = ev_pool_get_task_ecb(task_id);
    if (ecb == NULL) {
        return ERR_OBJID;
    }
    
    EV_ENTER_CRITICAL();
    
    /* Check if requested events are already pending */
    ecb->waiting_events = events;
    ecb->wait_condition = (flags & EV_ANY) ? EV_ANY : EV_ALL;
    
    events_satisfied = ev_check_conditions(ecb);
    
    if (events_satisfied) {
        /* Events are already available */
        if (ecb->wait_condition == EV_ALL) {
            *events_r = ecb->pending_events & events;
            ev_clear_received(ecb, events);
        } else {
            *events_r = ecb->pending_events & events;
            ev_clear_received(ecb, *events_r);
        }
        
        ev_update_statistics(ecb, EV_STAT_RECEIVE);
        EV_EXIT_CRITICAL();
        return 0;
    }
    
    /* Events not available - check for no-wait flag */
    if (flags & EV_NOWAIT) {
        *events_r = 0;
        EV_EXIT_CRITICAL();
        return ERR_NOEVS;
    }
    
    /* Start waiting for events */
    error = ev_start_wait(ecb, events, flags, timeout);
    if (error != 0) {
        EV_EXIT_CRITICAL();
        return error;
    }
    
    EV_EXIT_CRITICAL();
    
    /* Wait on hardware event object */
    if (state->hw_ops && state->hw_ops->wait_event) {
        error = state->hw_ops->wait_event(ecb, timeout);
        
        if (error == ERR_TIMEOUT) {
            ev_update_statistics(ecb, EV_STAT_TIMEOUT);
            ev_cancel_wait(ecb);
            return ERR_TIMEOUT;
        }
    }
    
    /* Complete the wait and return received events */
    return ev_complete_wait(ecb, events_r);
}

ULONG ev_send(ULONG tid, ULONG events)
{
    EV_STATE *state = EV_GET_STATE();
    EV_ECB *ecb;
    ULONG error;
    ULONG events_satisfied;
    
    error = ev_init_once();
    if (error != 0) {
        return error;
    }
    
    error = ev_pool_validate_task_id(tid);
    if (error != 0) {
        return error;
    }
    
    error = ev_validate_events(events);
    if (error != 0) {
        return error;
    }
    
    ecb = ev_pool_get_task_ecb(tid);
    if (ecb == NULL) {
        return ERR_OBJID;
    }
    
    EV_ENTER_CRITICAL();
    
    /* Add new events to pending events */
    ev_update_pending(ecb, events);
    
    /* Check if waiting task's conditions are now satisfied */
    if (ecb->state == EV_STATE_WAITING) {
        events_satisfied = ev_check_conditions(ecb);
        
        if (events_satisfied) {
            /* Signal the waiting task */
            if (state->hw_ops && state->hw_ops->signal_event) {
                state->hw_ops->signal_event(ecb);
            }
            
            /* Prepare received events based on wait condition */
            if (ecb->wait_condition == EV_ALL) {
                ecb->received_events = ecb->pending_events & ecb->waiting_events;
                ev_clear_received(ecb, ecb->waiting_events);
            } else {
                ecb->received_events = ecb->pending_events & ecb->waiting_events;
                ev_clear_received(ecb, ecb->received_events);
            }
            
            ecb->state = EV_STATE_SIGNALED;
        }
    }
    
    EV_EXIT_CRITICAL();
    
    return 0;
}

/******************************************************************************
* Hardware Abstraction
******************************************************************************/

ULONG ev_hw_init(void)
{
    EV_STATE *state = EV_GET_STATE();
    
    /* Set up hardware operations - would be selected based on platform */
    #ifdef STM32F4
    state->hw_ops = &ev_hw_stm32f4_ops;
    #else
    state->hw_ops = &ev_hw_generic_ops;
    #endif
    
    /* Initialize hardware */
    if (state->hw_ops && state->hw_ops->init) {
        return state->hw_ops->init();
    }
    
    return 0;
}

/******************************************************************************
* Legacy Compatibility and Additional Functions
******************************************************************************/

/* Initialize event system (called during kernel initialization) */
ULONG gxk_ev_init(void)
{
    return ev_init_once();
}