/************************************BEGIN*****************************************
*                       COPYRIGHT %G% %U% BY GHWORKS, LLC 
*                             ALL RIGHTS RESERVED
*                         SPDX-License-Identifier: MIT
* ********************************************************************************
* Name:        semaphore_hw_stm32f4.c
* Type:        C Source
* Description: STM32F4 Hardware Semaphore Implementation
*
* This file provides hardware-specific semaphore implementation for STM32F4 
* microcontrollers. It uses interrupt-driven counting and task notifications
* for optimal real-time performance.
*
**************************************END***************************************/

#include "../../gxkernel.h"
#include "semaphore.h"

/* STM32F4 hardware semaphore context structure */
typedef struct sm_stm32f4_context {
    volatile LONG   count;          /* Current semaphore count */
    ULONG           max_count;      /* Maximum count */
    volatile ULONG  waiting_tasks;  /* Number of waiting tasks */
    ULONG           signal_count;   /* Number of signal operations */
    ULONG           wait_count;     /* Number of wait operations */
} SM_STM32F4_CONTEXT;

/* STM32F4 Hardware Semaphore State */
static struct {
    ULONG           initialized;
    ULONG           total_created;
    ULONG           total_signals;
    ULONG           total_waits;
} sm_stm32f4_state = {0};

/******************************************************************************
* STM32F4 Hardware Semaphore Functions
******************************************************************************/

static ULONG sm_hw_stm32f4_init(void)
{
    if (sm_stm32f4_state.initialized) {
        return 0;
    }
    
    sm_stm32f4_state.total_created = 0;
    sm_stm32f4_state.total_signals = 0;
    sm_stm32f4_state.total_waits = 0;
    sm_stm32f4_state.initialized = 1;
    
    return 0;
}

static ULONG sm_hw_stm32f4_create_semaphore(void *scb, ULONG initial_count, ULONG max_count)
{
    SM_SCB *sem_cb = (SM_SCB *)scb;
    SM_STM32F4_CONTEXT *ctx;
    
    if (sem_cb == NULL) {
        return ERR_BADPARAM;
    }
    
    /* Allocate context structure */
    ctx = malloc(sizeof(SM_STM32F4_CONTEXT));
    if (ctx == NULL) {
        return ERR_NOMEM;
    }
    
    /* Initialize STM32F4 context */
    ctx->count = initial_count;
    ctx->max_count = max_count;
    ctx->waiting_tasks = 0;
    ctx->signal_count = 0;
    ctx->wait_count = 0;
    
    /* Store context in SCB */
    sem_cb->hw_context = ctx;
    sem_cb->context_size = sizeof(SM_STM32F4_CONTEXT);
    
    sm_stm32f4_state.total_created++;
    
    return 0;
}

static ULONG sm_hw_stm32f4_delete_semaphore(void *scb)
{
    SM_SCB *sem_cb = (SM_SCB *)scb;
    SM_STM32F4_CONTEXT *ctx;
    
    if (sem_cb == NULL || sem_cb->hw_context == NULL) {
        return ERR_BADPARAM;
    }
    
    ctx = (SM_STM32F4_CONTEXT *)sem_cb->hw_context;
    
    /* Clear semaphore state */
    ctx->count = 0;
    ctx->waiting_tasks = 0;
    
    /* Free context */
    free(ctx);
    sem_cb->hw_context = NULL;
    sem_cb->context_size = 0;
    
    return 0;
}

static ULONG sm_hw_stm32f4_wait_semaphore(void *scb, ULONG timeout)
{
    SM_SCB *sem_cb = (SM_SCB *)scb;
    SM_STM32F4_CONTEXT *ctx;
    ULONG result = 0;
    
    if (sem_cb == NULL || sem_cb->hw_context == NULL) {
        return ERR_BADPARAM;
    }
    
    ctx = (SM_STM32F4_CONTEXT *)sem_cb->hw_context;
    
    /* Disable interrupts for atomic operation */
    __asm volatile ("cpsid i" ::: "memory");
    
    ctx->wait_count++;
    sm_stm32f4_state.total_waits++;
    
    if (ctx->count > 0) {
        /* Semaphore available - decrement count */
        ctx->count--;
        result = 0;
    } else {
        /* Semaphore not available */
        ctx->waiting_tasks++;
        result = ERR_NOSEM;  /* Would normally suspend task here */
    }
    
    /* Re-enable interrupts */
    __asm volatile ("cpsie i" ::: "memory");
    
    /* For STM32F4, actual task suspension would be handled by task scheduler */
    /* The semaphore logic in semaphore.c handles the wait queue management */
    
    return result;
}

static ULONG sm_hw_stm32f4_signal_semaphore(void *scb)
{
    SM_SCB *sem_cb = (SM_SCB *)scb;
    SM_STM32F4_CONTEXT *ctx;
    ULONG result = 0;
    
    if (sem_cb == NULL || sem_cb->hw_context == NULL) {
        return ERR_BADPARAM;
    }
    
    ctx = (SM_STM32F4_CONTEXT *)sem_cb->hw_context;
    
    /* Disable interrupts for atomic operation */
    __asm volatile ("cpsid i" ::: "memory");
    
    ctx->signal_count++;
    sm_stm32f4_state.total_signals++;
    
    if (ctx->waiting_tasks > 0) {
        /* Wake up waiting task */
        ctx->waiting_tasks--;
        /* Task scheduler would handle actual task resumption */
        result = 0;
    } else {
        /* No waiting tasks - increment count if not at maximum */
        if (ctx->count < (LONG)ctx->max_count) {
            ctx->count++;
            result = 0;
        } else {
            result = ERR_SEMFULL;
        }
    }
    
    /* Re-enable interrupts */
    __asm volatile ("cpsie i" ::: "memory");
    
    return result;
}

static ULONG sm_hw_stm32f4_get_count(void *scb)
{
    SM_SCB *sem_cb = (SM_SCB *)scb;
    SM_STM32F4_CONTEXT *ctx;
    ULONG count;
    
    if (sem_cb == NULL || sem_cb->hw_context == NULL) {
        return 0;
    }
    
    ctx = (SM_STM32F4_CONTEXT *)sem_cb->hw_context;
    
    /* Read count atomically */
    __asm volatile ("cpsid i" ::: "memory");
    count = (ULONG)ctx->count;
    __asm volatile ("cpsie i" ::: "memory");
    
    return count;
}

/* Hardware operations structure for STM32F4 */
SM_HW_OPS sm_hw_stm32f4_ops = {
    .init = sm_hw_stm32f4_init,
    .create_semaphore = sm_hw_stm32f4_create_semaphore,
    .delete_semaphore = sm_hw_stm32f4_delete_semaphore,
    .wait_semaphore = sm_hw_stm32f4_wait_semaphore,
    .signal_semaphore = sm_hw_stm32f4_signal_semaphore,
    .get_count = sm_hw_stm32f4_get_count
};

/******************************************************************************
* Utility Functions for Debugging/Configuration
******************************************************************************/

/* Get STM32F4 semaphore hardware state */
void sm_hw_stm32f4_get_state(ULONG *initialized, ULONG *total_created, 
                             ULONG *total_signals, ULONG *total_waits)
{
    if (initialized) *initialized = sm_stm32f4_state.initialized;
    if (total_created) *total_created = sm_stm32f4_state.total_created;
    if (total_signals) *total_signals = sm_stm32f4_state.total_signals;
    if (total_waits) *total_waits = sm_stm32f4_state.total_waits;
}

/* Get semaphore context statistics */
void sm_hw_stm32f4_get_context_stats(void *scb, ULONG *current_count, ULONG *max_count,
                                     ULONG *waiting_tasks, ULONG *signal_count)
{
    SM_SCB *sem_cb = (SM_SCB *)scb;
    SM_STM32F4_CONTEXT *ctx;
    
    if (sem_cb == NULL || sem_cb->hw_context == NULL) {
        return;
    }
    
    ctx = (SM_STM32F4_CONTEXT *)sem_cb->hw_context;
    
    if (current_count) *current_count = (ULONG)ctx->count;
    if (max_count) *max_count = ctx->max_count;
    if (waiting_tasks) *waiting_tasks = ctx->waiting_tasks;
    if (signal_count) *signal_count = ctx->signal_count;
}

/* Force signal a semaphore (for testing) */
void sm_hw_stm32f4_force_signal(void *scb)
{
    sm_hw_stm32f4_signal_semaphore(scb);
}

/* Check current semaphore count */
ULONG sm_hw_stm32f4_check_count(void *scb)
{
    return sm_hw_stm32f4_get_count(scb);
}