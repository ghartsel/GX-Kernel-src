/************************************BEGIN*****************************************
*                       COPYRIGHT %G% %U% BY GHWORKS, LLC 
*                             ALL RIGHTS RESERVED
*                         SPDX-License-Identifier: MIT
* ********************************************************************************
* Name:        semaphore.c
* Type:        C Source
* Description: Semaphore Services Implementation
*
* Interface (public) Routines:
*   sm_create    - Create a new semaphore
*   sm_delete    - Delete a semaphore
*   sm_ident     - Identify semaphore by name
*   sm_p         - Wait for semaphore (P operation)
*   sm_v         - Signal semaphore (V operation)
*
* Private Functions:
*   Semaphore pool management
*   Wait queue management
*   Count management
*   Hardware abstraction
*   Statistics tracking
*
**************************************END***************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../gxkernel.h"
#include "../../gxkCfg.h"
#include "semaphore.h"

/* Global semaphore state */
SM_STATE sm_global_state = {0};

/* Forward declarations for internal functions */
static ULONG sm_init_once(void);

/******************************************************************************
* Semaphore Pool Management Functions
******************************************************************************/

ULONG sm_pool_init(void)
{
    SM_POOL *pool = SM_GET_POOL();
    int i;
    
    memset(pool, 0, sizeof(SM_POOL));
    
    pool->magic = SM_POOL_MAGIC;
    pool->max_semaphores = SM_MAX_SEMAPHORES;
    pool->active_count = 0;
    pool->next_id = 1;
    
    /* Initialize free list */
    for (i = 0; i < SM_MAX_SEMAPHORES - 1; i++) {
        pool->semaphores[i].next = &pool->semaphores[i + 1];
        pool->semaphores[i].semaphore_id = 0;
        pool->semaphores[i].state = SM_STATE_FREE;
        pool->semaphores[i].magic = 0;
    }
    pool->semaphores[SM_MAX_SEMAPHORES - 1].next = NULL;
    pool->free_list = &pool->semaphores[0];
    pool->active_list = NULL;
    
    return 0;
}

SM_SCB *sm_pool_alloc(void)
{
    SM_POOL *pool = SM_GET_POOL();
    SM_SCB *scb;
    
    if (pool->free_list == NULL) {
        return NULL;
    }
    
    scb = pool->free_list;
    pool->free_list = scb->next;
    pool->active_count++;
    
    /* Initialize SCB */
    memset(scb, 0, sizeof(SM_SCB));
    scb->magic = SM_POOL_MAGIC;
    scb->semaphore_id = sm_generate_id();
    scb->state = SM_STATE_ACTIVE;
    
    /* Add to active list */
    scb->next = pool->active_list;
    if (pool->active_list != NULL) {
        pool->active_list->prev = scb;
    }
    pool->active_list = scb;
    
    return scb;
}

ULONG sm_pool_free(SM_SCB *scb)
{
    SM_POOL *pool = SM_GET_POOL();
    
    if (!SM_IS_VALID_SCB(scb)) {
        return ERR_BADPARAM;
    }
    
    /* Remove from active list */
    if (scb->prev != NULL) {
        scb->prev->next = scb->next;
    } else {
        pool->active_list = scb->next;
    }
    
    if (scb->next != NULL) {
        scb->next->prev = scb->prev;
    }
    
    /* Clear wait queue */
    sm_wait_queue_clear(scb);
    
    /* Delete hardware context */
    SM_STATE *state = SM_GET_STATE();
    if (state->hw_ops && state->hw_ops->delete_semaphore) {
        state->hw_ops->delete_semaphore(scb);
    }
    
    /* Clear the SCB */
    memset(scb, 0, sizeof(SM_SCB));
    scb->state = SM_STATE_FREE;
    
    /* Add to free list */
    scb->next = pool->free_list;
    pool->free_list = scb;
    pool->active_count--;
    
    return 0;
}

SM_SCB *sm_pool_find(ULONG semaphore_id)
{
    SM_POOL *pool = SM_GET_POOL();
    SM_SCB *scb;
    
    if (!SM_IS_VALID_ID(semaphore_id)) {
        return NULL;
    }
    
    /* Search active list */
    scb = pool->active_list;
    while (scb != NULL) {
        if (scb->semaphore_id == semaphore_id && SM_IS_VALID_SCB(scb)) {
            return scb;
        }
        scb = scb->next;
    }
    
    return NULL;
}

SM_SCB *sm_pool_find_by_name(char name[4])
{
    SM_POOL *pool = SM_GET_POOL();
    SM_SCB *scb;
    
    /* Search active list */
    scb = pool->active_list;
    while (scb != NULL) {
        if (SM_IS_VALID_SCB(scb) && sm_name_compare(scb->name, name)) {
            return scb;
        }
        scb = scb->next;
    }
    
    return NULL;
}

/******************************************************************************
* Wait Queue Management Functions
******************************************************************************/

ULONG sm_wait_queue_add(SM_SCB *scb, ULONG task_id, ULONG priority)
{
    SM_WAIT_NODE *node, *current, *prev;
    
    if (!SM_IS_VALID_SCB(scb)) {
        return ERR_BADPARAM;
    }
    
    /* Allocate wait node */
    node = malloc(sizeof(SM_WAIT_NODE));
    if (node == NULL) {
        return ERR_NOMEM;
    }
    
    node->task_id = task_id;
    node->priority = priority;
    node->wait_start = 0; /* Would get current time */
    node->next = NULL;
    node->prev = NULL;
    
    if (scb->wait_mode == SM_PRIOR) {
        /* Insert by priority (higher priority = lower number = earlier in queue) */
        prev = NULL;
        current = scb->wait_queue_head;
        
        while (current != NULL && current->priority <= priority) {
            prev = current;
            current = current->next;
        }
        
        node->next = current;
        node->prev = prev;
        
        if (prev != NULL) {
            prev->next = node;
        } else {
            scb->wait_queue_head = node;
        }
        
        if (current != NULL) {
            current->prev = node;
        } else {
            scb->wait_queue_tail = node;
        }
    } else {
        /* Insert at tail (FIFO) */
        if (scb->wait_queue_tail != NULL) {
            scb->wait_queue_tail->next = node;
            node->prev = scb->wait_queue_tail;
        } else {
            scb->wait_queue_head = node;
        }
        scb->wait_queue_tail = node;
    }
    
    scb->wait_count++;
    
    return 0;
}

ULONG sm_wait_queue_remove(SM_SCB *scb, ULONG task_id)
{
    SM_WAIT_NODE *current;
    
    if (!SM_IS_VALID_SCB(scb)) {
        return ERR_BADPARAM;
    }
    
    /* Find and remove the task */
    current = scb->wait_queue_head;
    while (current != NULL) {
        if (current->task_id == task_id) {
            /* Remove from queue */
            if (current->prev != NULL) {
                current->prev->next = current->next;
            } else {
                scb->wait_queue_head = current->next;
            }
            
            if (current->next != NULL) {
                current->next->prev = current->prev;
            } else {
                scb->wait_queue_tail = current->prev;
            }
            
            free(current);
            scb->wait_count--;
            return 0;
        }
        current = current->next;
    }
    
    return ERR_OBJNF;
}

ULONG sm_wait_queue_get_next(SM_SCB *scb)
{
    SM_WAIT_NODE *node;
    ULONG task_id;
    
    if (!SM_IS_VALID_SCB(scb) || scb->wait_queue_head == NULL) {
        return 0;
    }
    
    node = scb->wait_queue_head;
    task_id = node->task_id;
    
    /* Remove from head */
    scb->wait_queue_head = node->next;
    if (scb->wait_queue_head != NULL) {
        scb->wait_queue_head->prev = NULL;
    } else {
        scb->wait_queue_tail = NULL;
    }
    
    free(node);
    scb->wait_count--;
    
    return task_id;
}

ULONG sm_wait_queue_clear(SM_SCB *scb)
{
    SM_WAIT_NODE *current, *next;
    
    if (!SM_IS_VALID_SCB(scb)) {
        return ERR_BADPARAM;
    }
    
    current = scb->wait_queue_head;
    while (current != NULL) {
        next = current->next;
        free(current);
        current = next;
    }
    
    scb->wait_queue_head = NULL;
    scb->wait_queue_tail = NULL;
    scb->wait_count = 0;
    
    return 0;
}

/******************************************************************************
* Semaphore Operations
******************************************************************************/

ULONG sm_increment_count(SM_SCB *scb)
{
    if (!SM_IS_VALID_SCB(scb)) {
        return ERR_BADPARAM;
    }
    
    if (scb->current_count >= (LONG)scb->maximum_count) {
        return ERR_SEMFULL;
    }
    
    scb->current_count++;
    return 0;
}

ULONG sm_decrement_count(SM_SCB *scb)
{
    if (!SM_IS_VALID_SCB(scb)) {
        return ERR_BADPARAM;
    }
    
    if (scb->current_count <= 0) {
        return ERR_NOSEM;
    }
    
    scb->current_count--;
    return 0;
}

ULONG sm_check_available(SM_SCB *scb)
{
    if (!SM_IS_VALID_SCB(scb)) {
        return 0;
    }
    
    return (scb->current_count > 0) ? 1 : 0;
}

/******************************************************************************
* Utility Functions
******************************************************************************/

ULONG sm_validate_scb(SM_SCB *scb)
{
    return SM_IS_VALID_SCB(scb) ? 0 : ERR_BADPARAM;
}

ULONG sm_validate_count(LONG count)
{
    return SM_IS_VALID_COUNT(count) ? 0 : ERR_BADPARAM;
}

ULONG sm_validate_flags(ULONG flags)
{
    return SM_IS_VALID_FLAGS(flags) ? 0 : ERR_BADPARAM;
}

ULONG sm_generate_id(void)
{
    SM_POOL *pool = SM_GET_POOL();
    ULONG id = pool->next_id++;
    
    if (pool->next_id == 0) {
        pool->next_id = 1;  /* Wrap around, skip 0 */
    }
    
    return id;
}

void sm_name_copy(char dest[4], char src[4])
{
    int i;
    for (i = 0; i < 4; i++) {
        dest[i] = src ? src[i] : '\0';
    }
}

ULONG sm_name_compare(char name1[4], char name2[4])
{
    int i;
    for (i = 0; i < 4; i++) {
        if (name1[i] != name2[i]) {
            return 0;
        }
    }
    return 1;
}

ULONG sm_get_current_task_id(void)
{
    /* This would integrate with the task system to get current task ID */
    return 0;
}

ULONG sm_get_current_priority(void)
{
    /* This would integrate with the task system to get current priority */
    return 128;
}

void sm_update_statistics(SM_SCB *scb, ULONG operation, ULONG wait_time)
{
    if (!SM_IS_VALID_SCB(scb)) {
        return;
    }
    
    switch (operation) {
        case SM_STAT_WAIT:
            scb->total_waits++;
            if (wait_time > scb->max_wait_time) {
                scb->max_wait_time = wait_time;
            }
            break;
            
        case SM_STAT_SIGNAL:
            scb->total_signals++;
            break;
            
        case SM_STAT_TIMEOUT:
            scb->total_timeouts++;
            break;
            
        default:
            break;
    }
}

ULONG sm_get_statistics(ULONG semaphore_id, ULONG *waits, ULONG *signals, 
                       ULONG *timeouts, ULONG *max_wait)
{
    SM_SCB *scb = sm_pool_find(semaphore_id);
    
    if (scb == NULL) {
        return ERR_OBJID;
    }
    
    if (waits) *waits = scb->total_waits;
    if (signals) *signals = scb->total_signals;
    if (timeouts) *timeouts = scb->total_timeouts;
    if (max_wait) *max_wait = scb->max_wait_time;
    
    return 0;
}

static ULONG sm_init_once(void)
{
    SM_STATE *state = SM_GET_STATE();
    
    if (state->initialized) {
        return 0;
    }
    
    /* Initialize semaphore pool */
    sm_pool_init();
    
    /* Initialize hardware abstraction */
    sm_hw_init();
    
    state->total_created = 0;
    state->total_deleted = 0;
    state->initialized = 1;
    
    return 0;
}

/******************************************************************************
* Public API Functions
******************************************************************************/

ULONG sm_create(char name[4], ULONG count, ULONG flags, ULONG *smid)
{
    SM_STATE *state = SM_GET_STATE();
    SM_SCB *scb;
    ULONG error;
    
    error = sm_init_once();
    if (error != 0) {
        return error;
    }
    
    if (smid == NULL) {
        return ERR_BADPARAM;
    }
    
    error = sm_validate_count(count);
    if (error != 0) {
        return error;
    }
    
    error = sm_validate_flags(flags);
    if (error != 0) {
        return error;
    }
    
    scb = sm_pool_alloc();
    if (scb == NULL) {
        return ERR_NOSCB;
    }
    
    /* Initialize SCB fields */
    sm_name_copy(scb->name, name);
    scb->current_count = count;
    scb->initial_count = count;
    scb->maximum_count = (flags & SM_PRIOR) ? SM_MAX_COUNT : 8; /* Default max */
    scb->flags = flags;
    scb->wait_mode = (flags & SM_PRIOR) ? SM_PRIOR : SM_FIFO;
    
    /* Create hardware semaphore */
    if (state->hw_ops && state->hw_ops->create_semaphore) {
        error = state->hw_ops->create_semaphore(scb, count, scb->maximum_count);
        if (error != 0) {
            sm_pool_free(scb);
            return error;
        }
    }
    
    *smid = scb->semaphore_id;
    state->total_created++;
    
    return 0;
}

ULONG sm_delete(ULONG smid)
{
    SM_STATE *state = SM_GET_STATE();
    SM_SCB *scb;
    
    sm_init_once();
    
    scb = sm_pool_find(smid);
    if (scb == NULL) {
        return ERR_OBJID;
    }
    
    if (scb->state == SM_STATE_FREE) {
        return ERR_OBJDEL;
    }
    
    SM_ENTER_CRITICAL();
    
    /* Wake up all waiting tasks with error */
    while (scb->wait_queue_head != NULL) {
        ULONG task_id = sm_wait_queue_get_next(scb);
        /* Would resume task with error: t_resume(task_id); */
    }
    
    /* Free the semaphore */
    sm_pool_free(scb);
    state->total_deleted++;
    
    SM_EXIT_CRITICAL();
    
    return 0;
}

ULONG sm_ident(char name[4], ULONG node, ULONG *smid)
{
    SM_SCB *scb;
    
    sm_init_once();
    
    if (smid == NULL) {
        return ERR_BADPARAM;
    }
    
    /* Node parameter ignored for now (single-node implementation) */
    
    scb = sm_pool_find_by_name(name);
    if (scb == NULL) {
        return ERR_OBJNF;
    }
    
    *smid = scb->semaphore_id;
    
    return 0;
}

ULONG sm_p(ULONG smid, ULONG flags, ULONG timeout)
{
    SM_STATE *state = SM_GET_STATE();
    SM_SCB *scb;
    ULONG task_id, priority;
    ULONG error;
    
    sm_init_once();
    
    scb = sm_pool_find(smid);
    if (scb == NULL) {
        return ERR_OBJID;
    }
    
    if (scb->state != SM_STATE_ACTIVE) {
        return ERR_OBJDEL;
    }
    
    SM_ENTER_CRITICAL();
    
    /* Check if semaphore is available */
    if (sm_check_available(scb)) {
        error = sm_decrement_count(scb);
        sm_update_statistics(scb, SM_STAT_WAIT, 0);
        SM_EXIT_CRITICAL();
        return error;
    }
    
    /* Semaphore not available - check for no-wait flag */
    if (flags & SM_NOWAIT) {
        SM_EXIT_CRITICAL();
        return ERR_NOSEM;
    }
    
    /* Add to wait queue */
    task_id = sm_get_current_task_id();
    priority = sm_get_current_priority();
    
    error = sm_wait_queue_add(scb, task_id, priority);
    if (error != 0) {
        SM_EXIT_CRITICAL();
        return error;
    }
    
    SM_EXIT_CRITICAL();
    
    /* Wait on hardware semaphore */
    if (state->hw_ops && state->hw_ops->wait_semaphore) {
        error = state->hw_ops->wait_semaphore(scb, timeout);
        
        if (error == ERR_TIMEOUT) {
            SM_ENTER_CRITICAL();
            sm_wait_queue_remove(scb, task_id);
            sm_update_statistics(scb, SM_STAT_TIMEOUT, 0);
            SM_EXIT_CRITICAL();
            return ERR_TIMEOUT;
        }
    }
    
    sm_update_statistics(scb, SM_STAT_WAIT, 0);
    return 0;
}

ULONG sm_v(ULONG smid)
{
    SM_STATE *state = SM_GET_STATE();
    SM_SCB *scb;
    ULONG task_id;
    ULONG error;
    
    sm_init_once();
    
    scb = sm_pool_find(smid);
    if (scb == NULL) {
        return ERR_OBJID;
    }
    
    if (scb->state != SM_STATE_ACTIVE) {
        return ERR_OBJDEL;
    }
    
    SM_ENTER_CRITICAL();
    
    /* Check if there are waiting tasks */
    if (scb->wait_count > 0) {
        /* Wake up next waiting task */
        task_id = sm_wait_queue_get_next(scb);
        
        /* Signal hardware semaphore */
        if (state->hw_ops && state->hw_ops->signal_semaphore) {
            state->hw_ops->signal_semaphore(scb);
        }
        
        /* Would resume task: t_resume(task_id); */
    } else {
        /* No waiting tasks - increment count */
        error = sm_increment_count(scb);
        if (error != 0) {
            SM_EXIT_CRITICAL();
            return error;
        }
    }
    
    sm_update_statistics(scb, SM_STAT_SIGNAL, 0);
    
    SM_EXIT_CRITICAL();
    
    return 0;
}

/******************************************************************************
* Hardware Abstraction
******************************************************************************/

ULONG sm_hw_init(void)
{
    SM_STATE *state = SM_GET_STATE();
    
    /* Set up hardware operations - would be selected based on platform */
    #ifdef STM32F4
    state->hw_ops = &sm_hw_stm32f4_ops;
    #else
    state->hw_ops = &sm_hw_generic_ops;
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

/* Initialize semaphore system (called during kernel initialization) */
ULONG gxk_sem_init(void)
{
    return sm_init_once();
}