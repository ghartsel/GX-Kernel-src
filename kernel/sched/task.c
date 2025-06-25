/************************************BEGIN*****************************************
*                       COPYRIGHT %G% %U% BY GHWORKS, LLC 
*                             ALL RIGHTS RESERVED
*                         SPDX-License-Identifier: MIT
* ********************************************************************************
* Name:        task.c
* Type:        C Source
* Description: Task Management Implementation
*
* Interface (public) Routines:
*   t_create     - Create a new task
*   t_delete     - Delete a task
*   t_getreg     - Get task register value
*   t_ident      - Get task ID by name or current task
*   t_mode       - Set task mode
*   t_restart    - Restart a task
*   t_resume     - Resume a suspended task
*   t_setpri     - Set task priority
*   t_setreg     - Set task register value
*   t_start      - Start a created task
*   t_suspend    - Suspend a task
*
* Private Functions:
*   Task pool management
*   Scheduler implementation
*   Context management
*   Stack management
*   Priority-based ready queues
*
**************************************END***************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../gxkernel.h"
#include "../../gxkCfg.h"
#include "task.h"

/* Global task state */
T_STATE t_global_state = {0};

/* Forward declarations for internal functions */
static ULONG t_init_once(void);
static ULONG t_validate_inputs(char name[4], ULONG priority, ULONG sstack, ULONG ustack, ULONG flags);
static void t_idle_task(ULONG args[]);

/******************************************************************************
* Task Pool Management Functions
******************************************************************************/

ULONG t_pool_init(void)
{
    T_POOL *pool = T_GET_POOL();
    int i;
    
    memset(pool, 0, sizeof(T_POOL));
    
    pool->magic = T_POOL_MAGIC;
    pool->max_tasks = T_MAX_TASKS;
    pool->active_count = 0;
    pool->next_id = 1;
    
    /* Initialize free list */
    for (i = 0; i < T_MAX_TASKS - 1; i++) {
        pool->tasks[i].next = &pool->tasks[i + 1];
        pool->tasks[i].task_id = 0;
        pool->tasks[i].state = T_STATE_FREE;
        pool->tasks[i].magic = 0;
    }
    pool->tasks[T_MAX_TASKS - 1].next = NULL;
    pool->free_list = &pool->tasks[0];
    
    return 0;
}

T_TCB *t_pool_alloc(void)
{
    T_POOL *pool = T_GET_POOL();
    T_TCB *tcb;
    
    if (pool->free_list == NULL) {
        return NULL;
    }
    
    tcb = pool->free_list;
    pool->free_list = tcb->next;
    pool->active_count++;
    
    /* Initialize TCB */
    memset(tcb, 0, sizeof(T_TCB));
    tcb->magic = T_POOL_MAGIC;
    tcb->task_id = t_generate_id();
    tcb->state = T_STATE_CREATED;
    tcb->priority = T_DEFAULT_PRIORITY;
    
    return tcb;
}

ULONG t_pool_free(T_TCB *tcb)
{
    T_POOL *pool = T_GET_POOL();
    
    if (!T_IS_VALID_TCB(tcb)) {
        return ERR_BADTCB;
    }
    
    /* Remove from any scheduler lists */
    t_scheduler_remove_ready(tcb);
    
    /* Free stack memory */
    t_stack_free(tcb);
    
    /* Delete hardware context */
    t_context_delete(tcb);
    
    /* Clear the TCB */
    memset(tcb, 0, sizeof(T_TCB));
    tcb->state = T_STATE_FREE;
    
    /* Add to free list */
    tcb->next = pool->free_list;
    pool->free_list = tcb;
    pool->active_count--;
    
    return 0;
}

T_TCB *t_pool_find(ULONG task_id)
{
    T_POOL *pool = T_GET_POOL();
    T_TCB *tcb;
    int i;
    
    if (task_id == T_INVALID_ID) {
        return NULL;
    }
    
    /* Search all tasks */
    for (i = 0; i < T_MAX_TASKS; i++) {
        tcb = &pool->tasks[i];
        if (tcb->task_id == task_id && T_IS_VALID_TCB(tcb)) {
            return tcb;
        }
    }
    
    return NULL;
}

T_TCB *t_pool_find_by_name(char name[4])
{
    T_POOL *pool = T_GET_POOL();
    T_TCB *tcb;
    int i;
    
    if (name == NULL) {
        return T_GET_CURRENT_TASK();
    }
    
    /* Search all tasks */
    for (i = 0; i < T_MAX_TASKS; i++) {
        tcb = &pool->tasks[i];
        if (T_IS_VALID_TCB(tcb) && t_name_compare(tcb->name, name)) {
            return tcb;
        }
    }
    
    return NULL;
}

/******************************************************************************
* Scheduler Functions
******************************************************************************/

ULONG t_scheduler_init(void)
{
    T_SCHEDULER *sched = T_GET_SCHEDULER();
    int i;
    
    memset(sched, 0, sizeof(T_SCHEDULER));
    
    sched->magic = T_POOL_MAGIC;
    sched->current_task = NULL;
    sched->ready_mask = 0;
    sched->preemption_enabled = 1;
    sched->context_switches = 0;
    
    /* Initialize ready lists */
    for (i = 0; i <= T_MAX_PRIORITY; i++) {
        sched->ready_lists[i] = NULL;
    }
    
    return 0;
}

void t_scheduler_add_ready(T_TCB *tcb)
{
    T_SCHEDULER *sched = T_GET_SCHEDULER();
    ULONG priority;
    T_TCB *head;
    
    if (!T_IS_VALID_TCB(tcb) || tcb->state != T_STATE_READY) {
        return;
    }
    
    priority = T_PRIORITY_TO_INDEX(tcb->priority);
    head = sched->ready_lists[priority];
    
    /* Add to head of priority list */
    tcb->next = head;
    tcb->prev = NULL;
    
    if (head != NULL) {
        head->prev = tcb;
    }
    
    sched->ready_lists[priority] = tcb;
    T_SET_READY_BIT(priority);
}

void t_scheduler_remove_ready(T_TCB *tcb)
{
    T_SCHEDULER *sched = T_GET_SCHEDULER();
    ULONG priority;
    
    if (!T_IS_VALID_TCB(tcb)) {
        return;
    }
    
    priority = T_PRIORITY_TO_INDEX(tcb->priority);
    
    /* Remove from linked list */
    if (tcb->prev != NULL) {
        tcb->prev->next = tcb->next;
    } else {
        sched->ready_lists[priority] = tcb->next;
    }
    
    if (tcb->next != NULL) {
        tcb->next->prev = tcb->prev;
    }
    
    tcb->next = NULL;
    tcb->prev = NULL;
    
    /* Clear ready bit if list is empty */
    if (sched->ready_lists[priority] == NULL) {
        T_CLEAR_READY_BIT(priority);
    }
}

T_TCB *t_scheduler_get_highest_ready(void)
{
    T_SCHEDULER *sched = T_GET_SCHEDULER();
    ULONG priority;
    
    if (sched->ready_mask == 0) {
        return NULL;
    }
    
    /* Find highest priority (lowest number) with ready tasks */
    priority = T_FIND_HIGHEST_PRIORITY();
    
    return sched->ready_lists[priority];
}

void t_scheduler_reschedule(void)
{
    T_SCHEDULER *sched = T_GET_SCHEDULER();
    T_TCB *current = sched->current_task;
    T_TCB *next;
    ULONG int_level;
    
    int_level = T_ENTER_CRITICAL();
    
    next = t_scheduler_get_highest_ready();
    
    if (next == NULL) {
        /* No ready tasks - should not happen if idle task exists */
        T_EXIT_CRITICAL(int_level);
        return;
    }
    
    if (current != next) {
        /* Context switch needed */
        sched->context_switches++;
        sched->last_switch_time = 0; /* Would get from timer */
        
        if (current != NULL && current->state == T_STATE_RUNNING) {
            current->state = T_STATE_READY;
            t_scheduler_add_ready(current);
        }
        
        /* Remove next task from ready list */
        t_scheduler_remove_ready(next);
        next->state = T_STATE_RUNNING;
        sched->current_task = next;
        
        /* Perform hardware context switch */
        t_context_switch(current, next);
    }
    
    T_EXIT_CRITICAL(int_level);
}

void t_scheduler_preempt(void)
{
    T_SCHEDULER *sched = T_GET_SCHEDULER();
    
    if (sched->preemption_enabled) {
        t_scheduler_reschedule();
    }
}

void t_scheduler_yield(void)
{
    t_scheduler_reschedule();
}

/******************************************************************************
* Context Management Functions
******************************************************************************/

ULONG t_context_create(T_TCB *tcb, void (*entry)(ULONG args[]), ULONG args[])
{
    T_STATE *state = T_GET_STATE();
    ULONG error;
    int i;
    
    if (!T_IS_VALID_TCB(tcb) || entry == NULL) {
        return ERR_BADPARAM;
    }
    
    tcb->entry_point = entry;
    
    /* Copy arguments */
    for (i = 0; i < 4; i++) {
        tcb->args[i] = args ? args[i] : 0;
    }
    
    /* Create hardware-specific context */
    if (state->hw_ops && state->hw_ops->create_context) {
        error = state->hw_ops->create_context(tcb, (void*)entry, 
                                            tcb->stack_base, tcb->stack_size);
        if (error != 0) {
            return error;
        }
    }
    
    return 0;
}

void t_context_switch(T_TCB *old_task, T_TCB *new_task)
{
    T_STATE *state = T_GET_STATE();
    
    if (state->hw_ops && state->hw_ops->switch_context) {
        state->hw_ops->switch_context(old_task, new_task);
    }
}

void t_context_delete(T_TCB *tcb)
{
    T_STATE *state = T_GET_STATE();
    
    if (state->hw_ops && state->hw_ops->delete_context) {
        state->hw_ops->delete_context(tcb);
    }
}

/******************************************************************************
* Stack Management Functions
******************************************************************************/

ULONG t_stack_alloc(T_TCB *tcb, ULONG size)
{
    T_STATE *state = T_GET_STATE();
    
    if (!T_IS_VALID_TCB(tcb) || size < T_MIN_STACK_SIZE) {
        return ERR_BADPARAM;
    }
    
    if (size > T_MAX_STACK_SIZE) {
        return ERR_TINYSTK;
    }
    
    /* Check total stack usage */
    if (state->total_stack_used + size > MAX_SSTACK) {
        return ERR_NOSTK;
    }
    
    /* Allocate stack memory */
    tcb->stack_base = malloc(size);
    if (tcb->stack_base == NULL) {
        return ERR_NOSTK;
    }
    
    tcb->stack_size = size;
    tcb->stack_used = 0;
    state->total_stack_used += size;
    
    /* Initialize stack with pattern for overflow detection */
    memset(tcb->stack_base, 0xAA, size);
    
    return 0;
}

void t_stack_free(T_TCB *tcb)
{
    T_STATE *state = T_GET_STATE();
    
    if (T_IS_VALID_TCB(tcb) && tcb->stack_base != NULL) {
        free(tcb->stack_base);
        state->total_stack_used -= tcb->stack_size;
        tcb->stack_base = NULL;
        tcb->stack_size = 0;
        tcb->stack_used = 0;
    }
}

ULONG t_stack_check(T_TCB *tcb)
{
    /* Stack overflow/underflow checking would be implemented here */
    /* This is platform-specific and would check stack patterns */
    return 0;
}

ULONG t_stack_usage(T_TCB *tcb)
{
    if (!T_IS_VALID_TCB(tcb) || tcb->stack_base == NULL) {
        return 0;
    }
    
    return tcb->stack_used;
}

/******************************************************************************
* Task State Management Functions
******************************************************************************/

void t_set_state(T_TCB *tcb, ULONG new_state)
{
    ULONG old_state;
    
    if (!T_IS_VALID_TCB(tcb)) {
        return;
    }
    
    old_state = tcb->state;
    tcb->state = new_state;
    
    /* Handle state transitions */
    switch (new_state) {
        case T_STATE_READY:
            if (old_state != T_STATE_READY) {
                t_scheduler_add_ready(tcb);
            }
            break;
            
        case T_STATE_RUNNING:
        case T_STATE_SUSPENDED:
        case T_STATE_BLOCKED:
        case T_STATE_DELETED:
            if (old_state == T_STATE_READY) {
                t_scheduler_remove_ready(tcb);
            }
            break;
    }
}

ULONG t_validate_priority(ULONG priority)
{
    return (priority >= T_MIN_PRIORITY && priority <= T_MAX_PRIORITY) ? 0 : ERR_PRIOR;
}

ULONG t_validate_flags(ULONG flags)
{
    /* Validate task creation flags */
    ULONG valid_flags = T_PREEMPT | T_NOPREEMPT | T_TSLICE | T_NOTSLICE |
                       T_ASR | T_NOASR | T_FPU | T_NOFPU | T_ISR | T_NOISR;
    
    return ((flags & ~valid_flags) == 0) ? 0 : ERR_BADPARAM;
}

/******************************************************************************
* Interrupt Management Functions
******************************************************************************/

ULONG t_disable_interrupts(void)
{
    T_STATE *state = T_GET_STATE();
    ULONG old_level = state->interrupts_disabled;
    
    if (state->hw_ops && state->hw_ops->disable_interrupts) {
        state->hw_ops->disable_interrupts();
    }
    
    state->interrupts_disabled++;
    return old_level;
}

void t_enable_interrupts(ULONG level)
{
    T_STATE *state = T_GET_STATE();
    
    if (state->interrupts_disabled > 0) {
        state->interrupts_disabled--;
        
        if (state->interrupts_disabled == 0 && 
            state->hw_ops && state->hw_ops->enable_interrupts) {
            state->hw_ops->enable_interrupts();
        }
    }
}

/******************************************************************************
* Utility Functions
******************************************************************************/

ULONG t_validate_tcb(T_TCB *tcb)
{
    return T_IS_VALID_TCB(tcb) ? 0 : ERR_BADTCB;
}

ULONG t_generate_id(void)
{
    T_POOL *pool = T_GET_POOL();
    ULONG id = pool->next_id++;
    
    if (pool->next_id == 0) {
        pool->next_id = 1;  /* Wrap around, skip 0 */
    }
    
    return id;
}

void t_name_copy(char dest[4], char src[4])
{
    int i;
    for (i = 0; i < 4; i++) {
        dest[i] = src ? src[i] : '\0';
    }
}

ULONG t_name_compare(char name1[4], char name2[4])
{
    int i;
    for (i = 0; i < 4; i++) {
        if (name1[i] != name2[i]) {
            return 0;
        }
    }
    return 1;
}

static ULONG t_validate_inputs(char name[4], ULONG priority, ULONG sstack, ULONG ustack, ULONG flags)
{
    ULONG total_stack = sstack + ustack;
    
    if (total_stack < T_MIN_STACK_SIZE) {
        return ERR_TINYSTK;
    }
    
    if (t_validate_priority(priority) != 0) {
        return ERR_PRIOR;
    }
    
    if (t_validate_flags(flags) != 0) {
        return ERR_BADPARAM;
    }
    
    return 0;
}

static ULONG t_init_once(void)
{
    T_STATE *state = T_GET_STATE();
    
    if (state->initialized) {
        return 0;
    }
    
    /* Initialize task pool */
    t_pool_init();
    
    /* Initialize scheduler */
    t_scheduler_init();
    
    /* Initialize hardware abstraction */
    t_hw_init();
    
    /* Create idle task */
    /* This would be implemented to create a low-priority idle task */
    
    state->initialized = 1;
    
    return 0;
}

static void t_idle_task(ULONG args[])
{
    /* Idle task implementation - runs when no other tasks are ready */
    while (1) {
        /* Could implement power management here */
        t_scheduler_yield();
    }
}

/******************************************************************************
* Public API Functions
******************************************************************************/

ULONG t_create(char name[4], ULONG prio, ULONG sstack, ULONG ustack, ULONG flags, ULONG *tid)
{
    T_TCB *tcb;
    ULONG error;
    ULONG total_stack = sstack + ustack;
    
    error = t_init_once();
    if (error != 0) {
        return error;
    }
    
    if (tid == NULL) {
        return ERR_BADPARAM;
    }
    
    error = t_validate_inputs(name, prio, sstack, ustack, flags);
    if (error != 0) {
        return error;
    }
    
    tcb = t_pool_alloc();
    if (tcb == NULL) {
        return ERR_NOTCB;
    }
    
    /* Initialize TCB fields */
    t_name_copy(tcb->name, name);
    tcb->priority = prio;
    tcb->flags = flags;
    tcb->mode = 0;
    tcb->state = T_STATE_CREATED;
    
    /* Allocate stack */
    error = t_stack_alloc(tcb, total_stack);
    if (error != 0) {
        t_pool_free(tcb);
        return error;
    }
    
    *tid = tcb->task_id;
    
    return 0;
}

ULONG t_delete(ULONG tid)
{
    T_TCB *tcb;
    ULONG int_level;
    
    t_init_once();
    
    tcb = t_pool_find(tid);
    if (tcb == NULL) {
        return ERR_OBJID;
    }
    
    if (tcb->state == T_STATE_FREE) {
        return ERR_OBJDEL;
    }
    
    int_level = T_ENTER_CRITICAL();
    
    /* Mark for deletion */
    t_set_state(tcb, T_STATE_DELETED);
    
    /* If it's the current task, schedule another */
    if (tcb == T_GET_CURRENT_TASK()) {
        t_scheduler_reschedule();
    }
    
    /* Free the task */
    t_pool_free(tcb);
    
    T_EXIT_CRITICAL(int_level);
    
    return 0;
}

ULONG t_getreg(ULONG tid, ULONG regnum, ULONG *reg_value)
{
    T_TCB *tcb;
    
    t_init_once();
    
    if (reg_value == NULL || regnum >= T_REG_COUNT) {
        return ERR_BADPARAM;
    }
    
    if (tid == 0) {
        tcb = T_GET_CURRENT_TASK();
    } else {
        tcb = t_pool_find(tid);
    }
    
    if (tcb == NULL) {
        return ERR_OBJID;
    }
    
    if (tcb->state == T_STATE_FREE) {
        return ERR_OBJDEL;
    }
    
    *reg_value = tcb->reg[regnum];
    
    return 0;
}

ULONG t_ident(char name[4], ULONG node, ULONG *tid)
{
    T_TCB *tcb;
    
    t_init_once();
    
    if (tid == NULL) {
        return ERR_BADPARAM;
    }
    
    /* Node parameter ignored for now (single-node implementation) */
    
    tcb = t_pool_find_by_name(name);
    if (tcb == NULL) {
        return ERR_OBJNF;
    }
    
    *tid = tcb->task_id;
    
    return 0;
}

ULONG t_mode(ULONG mask, ULONG new_mode, ULONG *old_mode)
{
    T_TCB *current;
    
    t_init_once();
    
    current = T_GET_CURRENT_TASK();
    if (current == NULL) {
        return ERR_NOTACTIVE;
    }
    
    if (old_mode != NULL) {
        *old_mode = current->mode;
    }
    
    /* Update mode based on mask */
    if (mask & T_PREEMPT) {
        if (new_mode & T_PREEMPT) {
            T_MODE_CLEAR(current, T_NOPREEMPT);
        } else {
            T_MODE_SET(current, T_NOPREEMPT);
        }
    }
    
    if (mask & T_TSLICE) {
        if (new_mode & T_TSLICE) {
            T_MODE_SET(current, T_TSLICE);
        } else {
            T_MODE_CLEAR(current, T_TSLICE);
        }
    }
    
    if (mask & T_ASR) {
        if (new_mode & T_ASR) {
            T_MODE_SET(current, T_ASR);
        } else {
            T_MODE_CLEAR(current, T_ASR);
        }
    }
    
    if (mask & T_ISR) {
        if (new_mode & T_ISR) {
            T_MODE_SET(current, T_ISR);
        } else {
            T_MODE_CLEAR(current, T_ISR);
        }
    }
    
    return 0;
}

ULONG t_restart(ULONG tid, ULONG targs[])
{
    T_TCB *tcb;
    void (*entry_point)(ULONG args[]);
    ULONG mode;
    ULONG error;
    
    t_init_once();
    
    tcb = t_pool_find(tid);
    if (tcb == NULL) {
        return ERR_OBJID;
    }
    
    if (tcb->state == T_STATE_FREE) {
        return ERR_OBJDEL;
    }
    
    if (tcb->state == T_STATE_CREATED) {
        return ERR_NACTIVE;
    }
    
    /* Save restart parameters */
    entry_point = tcb->entry_point;
    mode = tcb->mode;
    
    /* Delete and recreate context */
    t_context_delete(tcb);
    
    error = t_context_create(tcb, entry_point, targs);
    if (error != 0) {
        return error;
    }
    
    /* Reset to ready state */
    t_set_state(tcb, T_STATE_READY);
    
    return 0;
}

ULONG t_resume(ULONG tid)
{
    T_TCB *tcb;
    ULONG int_level;
    
    t_init_once();
    
    tcb = t_pool_find(tid);
    if (tcb == NULL) {
        return ERR_OBJID;
    }
    
    if (tcb->state == T_STATE_FREE) {
        return ERR_OBJDEL;
    }
    
    if (tcb->state != T_STATE_SUSPENDED) {
        return ERR_NOTSUSP;
    }
    
    int_level = T_ENTER_CRITICAL();
    
    t_set_state(tcb, T_STATE_READY);
    t_scheduler_preempt();
    
    T_EXIT_CRITICAL(int_level);
    
    return 0;
}

ULONG t_setpri(ULONG tid, ULONG newprio, ULONG *oldprio)
{
    T_TCB *tcb;
    ULONG int_level;
    ULONG error;
    
    t_init_once();
    
    tcb = t_pool_find(tid);
    if (tcb == NULL) {
        return ERR_OBJID;
    }
    
    if (tcb->state == T_STATE_FREE) {
        return ERR_OBJDEL;
    }
    
    error = t_validate_priority(newprio);
    if (error != 0) {
        return ERR_SETPRI;
    }
    
    if (oldprio != NULL) {
        *oldprio = tcb->priority;
    }
    
    int_level = T_ENTER_CRITICAL();
    
    /* If task is ready, need to move it in ready queues */
    if (tcb->state == T_STATE_READY) {
        t_scheduler_remove_ready(tcb);
        tcb->priority = newprio;
        t_scheduler_add_ready(tcb);
    } else {
        tcb->priority = newprio;
    }
    
    /* Check if reschedule needed */
    t_scheduler_preempt();
    
    T_EXIT_CRITICAL(int_level);
    
    return 0;
}

ULONG t_setreg(ULONG tid, ULONG regnum, ULONG reg_value)
{
    T_TCB *tcb;
    
    t_init_once();
    
    if (regnum >= T_REG_COUNT) {
        return ERR_REGNUM;
    }
    
    if (tid == 0) {
        tcb = T_GET_CURRENT_TASK();
    } else {
        tcb = t_pool_find(tid);
    }
    
    if (tcb == NULL) {
        return ERR_OBJID;
    }
    
    if (tcb->state == T_STATE_FREE) {
        return ERR_OBJDEL;
    }
    
    tcb->reg[regnum] = reg_value;
    
    return 0;
}

ULONG t_start(ULONG tid, ULONG mode, void (*start_addr)(ULONG args[]), ULONG targs[])
{
    T_TCB *tcb;
    ULONG error;
    ULONG int_level;
    
    t_init_once();
    
    if (start_addr == NULL) {
        return ERR_BADPARAM;
    }
    
    tcb = t_pool_find(tid);
    if (tcb == NULL) {
        return ERR_OBJID;
    }
    
    if (tcb->state == T_STATE_FREE) {
        return ERR_OBJDEL;
    }
    
    if (tcb->state != T_STATE_CREATED) {
        return ERR_ACTIVE;
    }
    
    /* Create task context */
    error = t_context_create(tcb, start_addr, targs);
    if (error != 0) {
        return error;
    }
    
    tcb->mode = mode;
    
    int_level = T_ENTER_CRITICAL();
    
    t_set_state(tcb, T_STATE_READY);
    t_scheduler_preempt();
    
    T_EXIT_CRITICAL(int_level);
    
    return 0;
}

ULONG t_suspend(ULONG tid)
{
    T_TCB *tcb;
    ULONG int_level;
    
    t_init_once();
    
    tcb = t_pool_find(tid);
    if (tcb == NULL) {
        return ERR_OBJID;
    }
    
    if (tcb->state == T_STATE_FREE) {
        return ERR_OBJDEL;
    }
    
    if (tcb->state == T_STATE_SUSPENDED) {
        return ERR_SUSP;
    }
    
    int_level = T_ENTER_CRITICAL();
    
    t_set_state(tcb, T_STATE_SUSPENDED);
    
    /* If suspending current task, schedule another */
    if (tcb == T_GET_CURRENT_TASK()) {
        t_scheduler_reschedule();
    }
    
    T_EXIT_CRITICAL(int_level);
    
    return 0;
}