/************************************BEGIN*****************************************
*                       COPYRIGHT %G% %U% BY GHWORKS, LLC 
*                             ALL RIGHTS RESERVED
*                         SPDX-License-Identifier: MIT
* ********************************************************************************
* Name:        task_hw_generic.c
* Type:        C Source
* Description: Generic Hardware Task Abstraction Layer
*
* This file provides a portable hardware abstraction for task functionality
* that can work across different platforms without OS-specific dependencies.
* It uses POSIX threads and signals for context switching simulation.
*
* For embedded systems, this would be replaced with platform-specific
* implementations (task_hw_stm32f4.c, task_hw_riscv.c, etc.)
*
* Functions:
*   t_hw_generic_init           - Initialize generic task hardware
*   t_hw_generic_create_context - Create task context
*   t_hw_generic_switch_context - Switch between task contexts
*   t_hw_generic_delete_context - Delete task context
*   t_hw_generic_enable_int     - Enable interrupts
*   t_hw_generic_disable_int    - Disable interrupts
*   t_hw_init                   - Initialize hardware layer
*
**************************************END***************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <signal.h>
#include <setjmp.h>
#include <unistd.h>
#include <errno.h>
#include "../../gxkernel.h"
#include "task.h"

/* Generic hardware context structure */
typedef struct t_generic_context {
    pthread_t       thread_id;      /* POSIX thread ID */
    pthread_mutex_t mutex;          /* Thread synchronization */
    pthread_cond_t  cond;           /* Condition variable */
    jmp_buf         registers;      /* Register context (setjmp/longjmp) */
    volatile int    running;        /* Thread running flag */
    volatile int    suspended;      /* Thread suspended flag */
    void            (*entry)(ULONG args[]); /* Entry point */
    ULONG           args[4];        /* Task arguments */
    void            *stack_base;    /* Stack base pointer */
    ULONG           stack_size;     /* Stack size */
} T_GENERIC_CONTEXT;

/* Generic hardware state */
static struct {
    ULONG               initialized;
    ULONG               interrupts_enabled;
    pthread_mutex_t     global_mutex;
    T_GENERIC_CONTEXT   *current_context;
    ULONG               context_switches;
} t_hw_generic_state = {0};

/* Forward declarations */
static void *t_hw_generic_thread_wrapper(void *arg);
static void t_hw_generic_yield_handler(int sig);
static ULONG t_hw_generic_setup_signals(void);

/******************************************************************************
* Generic Hardware Task Functions
******************************************************************************/

static ULONG t_hw_generic_init(void)
{
    ULONG error;
    
    if (t_hw_generic_state.initialized) {
        return 0;
    }
    
    /* Initialize global mutex */
    if (pthread_mutex_init(&t_hw_generic_state.global_mutex, NULL) != 0) {
        return ERR_NOTSUPPORTED;
    }
    
    /* Set up signal handling for cooperative context switching */
    error = t_hw_generic_setup_signals();
    if (error != 0) {
        pthread_mutex_destroy(&t_hw_generic_state.global_mutex);
        return error;
    }
    
    t_hw_generic_state.interrupts_enabled = 1;
    t_hw_generic_state.current_context = NULL;
    t_hw_generic_state.context_switches = 0;
    t_hw_generic_state.initialized = 1;
    
    return 0;
}

static ULONG t_hw_generic_create_context(void *tcb, void *entry, void *stack, ULONG size)
{
    T_TCB *task = (T_TCB *)tcb;
    T_GENERIC_CONTEXT *ctx;
    pthread_attr_t attr;
    int result;
    
    if (task == NULL || entry == NULL) {
        return ERR_BADPARAM;
    }
    
    /* Allocate context structure */
    ctx = malloc(sizeof(T_GENERIC_CONTEXT));
    if (ctx == NULL) {
        return ERR_NOMEM;
    }
    
    memset(ctx, 0, sizeof(T_GENERIC_CONTEXT));
    
    /* Initialize context */
    ctx->entry = (void (*)(ULONG[]))entry;
    memcpy(ctx->args, task->args, sizeof(ctx->args));
    ctx->stack_base = stack;
    ctx->stack_size = size;
    ctx->running = 0;
    ctx->suspended = 1;  /* Start suspended */
    
    /* Initialize synchronization primitives */
    if (pthread_mutex_init(&ctx->mutex, NULL) != 0) {
        free(ctx);
        return ERR_NOTSUPPORTED;
    }
    
    if (pthread_cond_init(&ctx->cond, NULL) != 0) {
        pthread_mutex_destroy(&ctx->mutex);
        free(ctx);
        return ERR_NOTSUPPORTED;
    }
    
    /* Set up thread attributes */
    pthread_attr_init(&attr);
    pthread_attr_setstacksize(&attr, size);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
    
    /* Create thread */
    result = pthread_create(&ctx->thread_id, &attr, t_hw_generic_thread_wrapper, ctx);
    pthread_attr_destroy(&attr);
    
    if (result != 0) {
        pthread_cond_destroy(&ctx->cond);
        pthread_mutex_destroy(&ctx->mutex);
        free(ctx);
        return ERR_NOTSUPPORTED;
    }
    
    /* Store context in TCB */
    task->hw_context = ctx;
    task->context_size = sizeof(T_GENERIC_CONTEXT);
    
    return 0;
}

static void t_hw_generic_switch_context(void *old_tcb, void *new_tcb)
{
    T_TCB *old_task = (T_TCB *)old_tcb;
    T_TCB *new_task = (T_TCB *)new_tcb;
    T_GENERIC_CONTEXT *old_ctx = NULL;
    T_GENERIC_CONTEXT *new_ctx = NULL;
    
    if (new_task == NULL) {
        return;
    }
    
    t_hw_generic_state.context_switches++;
    
    /* Get contexts */
    if (old_task != NULL) {
        old_ctx = (T_GENERIC_CONTEXT *)old_task->hw_context;
    }
    
    new_ctx = (T_GENERIC_CONTEXT *)new_task->hw_context;
    if (new_ctx == NULL) {
        return;
    }
    
    pthread_mutex_lock(&t_hw_generic_state.global_mutex);
    
    /* Suspend old task */
    if (old_ctx != NULL && old_ctx->running) {
        pthread_mutex_lock(&old_ctx->mutex);
        old_ctx->running = 0;
        old_ctx->suspended = 1;
        pthread_mutex_unlock(&old_ctx->mutex);
    }
    
    /* Resume new task */
    pthread_mutex_lock(&new_ctx->mutex);
    new_ctx->running = 1;
    new_ctx->suspended = 0;
    t_hw_generic_state.current_context = new_ctx;
    pthread_cond_signal(&new_ctx->cond);
    pthread_mutex_unlock(&new_ctx->mutex);
    
    pthread_mutex_unlock(&t_hw_generic_state.global_mutex);
    
    /* Yield to allow new task to run */
    sched_yield();
}

static void t_hw_generic_delete_context(void *tcb)
{
    T_TCB *task = (T_TCB *)tcb;
    T_GENERIC_CONTEXT *ctx;
    
    if (task == NULL || task->hw_context == NULL) {
        return;
    }
    
    ctx = (T_GENERIC_CONTEXT *)task->hw_context;
    
    /* Signal thread to exit */
    pthread_mutex_lock(&ctx->mutex);
    ctx->running = 0;
    ctx->suspended = 0;  /* Allow thread to exit */
    pthread_cond_signal(&ctx->cond);
    pthread_mutex_unlock(&ctx->mutex);
    
    /* Wait for thread to finish */
    pthread_join(ctx->thread_id, NULL);
    
    /* Clean up synchronization primitives */
    pthread_cond_destroy(&ctx->cond);
    pthread_mutex_destroy(&ctx->mutex);
    
    /* Free context */
    free(ctx);
    task->hw_context = NULL;
    task->context_size = 0;
}

static void t_hw_generic_enable_int(void)
{
    t_hw_generic_state.interrupts_enabled = 1;
}

static void t_hw_generic_disable_int(void)
{
    t_hw_generic_state.interrupts_enabled = 0;
}

static ULONG t_hw_generic_get_current_sp(void)
{
    /* In generic implementation, return approximate stack pointer */
    volatile char stack_var;
    return (ULONG)&stack_var;
}

/* Hardware operations structure */
T_HW_OPS t_hw_generic_ops = {
    .init = t_hw_generic_init,
    .create_context = t_hw_generic_create_context,
    .switch_context = t_hw_generic_switch_context,
    .delete_context = t_hw_generic_delete_context,
    .enable_interrupts = t_hw_generic_enable_int,
    .disable_interrupts = t_hw_generic_disable_int,
    .get_current_sp = t_hw_generic_get_current_sp
};

/******************************************************************************
* Helper Functions
******************************************************************************/

static void *t_hw_generic_thread_wrapper(void *arg)
{
    T_GENERIC_CONTEXT *ctx = (T_GENERIC_CONTEXT *)arg;
    
    /* Wait until scheduled */
    pthread_mutex_lock(&ctx->mutex);
    while (ctx->suspended) {
        pthread_cond_wait(&ctx->cond, &ctx->mutex);
    }
    
    /* Check if thread should exit */
    if (!ctx->running) {
        pthread_mutex_unlock(&ctx->mutex);
        return NULL;
    }
    
    pthread_mutex_unlock(&ctx->mutex);
    
    /* Save initial context */
    if (setjmp(ctx->registers) == 0) {
        /* Call task entry point */
        if (ctx->entry != NULL) {
            ctx->entry(ctx->args);
        }
    }
    
    /* Task completed - mark as finished */
    pthread_mutex_lock(&ctx->mutex);
    ctx->running = 0;
    pthread_mutex_unlock(&ctx->mutex);
    
    /* Notify scheduler that task has completed */
    /* In real implementation, would call task completion handler */
    
    return NULL;
}

static void t_hw_generic_yield_handler(int sig)
{
    (void)sig;  /* Unused parameter */
    
    /* Cooperative yield point - save context and allow scheduler to run */
    if (t_hw_generic_state.current_context != NULL) {
        T_GENERIC_CONTEXT *ctx = t_hw_generic_state.current_context;
        
        /* Save current context */
        if (setjmp(ctx->registers) != 0) {
            /* Returning from longjmp - context has been restored */
            return;
        }
        
        /* Context saved, allow scheduler to switch tasks */
        sched_yield();
    }
}

static ULONG t_hw_generic_setup_signals(void)
{
    struct sigaction sa;
    
    /* Set up SIGUSR1 as yield signal */
    sa.sa_handler = t_hw_generic_yield_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    
    if (sigaction(SIGUSR1, &sa, NULL) != 0) {
        return ERR_NOTSUPPORTED;
    }
    
    return 0;
}

/******************************************************************************
* Public Hardware Initialization
******************************************************************************/

ULONG t_hw_init(void)
{
    T_STATE *state = T_GET_STATE();
    
    /* Set up generic hardware operations */
    state->hw_ops = &t_hw_generic_ops;
    
    /* Initialize hardware */
    if (state->hw_ops->init) {
        return state->hw_ops->init();
    }
    
    return 0;
}

/******************************************************************************
* Utility Functions for Testing/Debugging
******************************************************************************/

/* Get generic hardware state for debugging */
void t_hw_generic_get_state(ULONG *initialized, ULONG *interrupts_enabled, 
                            ULONG *context_switches)
{
    if (initialized) *initialized = t_hw_generic_state.initialized;
    if (interrupts_enabled) *interrupts_enabled = t_hw_generic_state.interrupts_enabled;
    if (context_switches) *context_switches = t_hw_generic_state.context_switches;
}

/* Force a context switch (for testing) */
void t_hw_generic_force_yield(void)
{
    t_hw_generic_yield_handler(SIGUSR1);
}

/* Check if interrupts are enabled */
ULONG t_hw_generic_interrupts_enabled(void)
{
    return t_hw_generic_state.interrupts_enabled;
}

/* Get current context information */
void *t_hw_generic_get_current_context(void)
{
    return t_hw_generic_state.current_context;
}

/* Simulate interrupt by sending yield signal to current thread */
void t_hw_generic_simulate_interrupt(void)
{
    if (t_hw_generic_state.current_context != NULL) {
        pthread_kill(t_hw_generic_state.current_context->thread_id, SIGUSR1);
    }
}