/************************************BEGIN*****************************************
*                       COPYRIGHT %G% %U% BY GHWORKS, LLC 
*                             ALL RIGHTS RESERVED
*                         SPDX-License-Identifier: MIT
* ********************************************************************************
* Name:        semaphore_hw_generic.c
* Type:        C Source
* Description: Generic Hardware Semaphore Abstraction Layer
*
* This file provides a portable hardware abstraction for semaphore functionality
* that can work across different platforms without OS-specific dependencies.
* It uses POSIX semaphores for cross-platform compatibility.
*
**************************************END***************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <semaphore.h>
#include <time.h>
#include <errno.h>
#include "../../gxkernel.h"
#include "semaphore.h"

/* Generic hardware semaphore context structure */
typedef struct sm_generic_context {
    sem_t           posix_sem;      /* POSIX semaphore */
    ULONG           current_count;  /* Current count */
    ULONG           max_count;      /* Maximum count */
    ULONG           wait_count;     /* Number of waiting tasks */
    ULONG           signal_count;   /* Number of signal operations */
} SM_GENERIC_CONTEXT;

/* Generic hardware state */
static struct {
    ULONG           initialized;
    ULONG           total_created;
    ULONG           total_signals;
    ULONG           total_waits;
} sm_hw_generic_state = {0};

/******************************************************************************
* Generic Hardware Semaphore Functions
******************************************************************************/

static ULONG sm_hw_generic_init(void)
{
    if (sm_hw_generic_state.initialized) {
        return 0;
    }
    
    sm_hw_generic_state.total_created = 0;
    sm_hw_generic_state.total_signals = 0;
    sm_hw_generic_state.total_waits = 0;
    sm_hw_generic_state.initialized = 1;
    
    return 0;
}

static ULONG sm_hw_generic_create_semaphore(void *scb, ULONG initial_count, ULONG max_count)
{
    SM_SCB *sem_cb = (SM_SCB *)scb;
    SM_GENERIC_CONTEXT *ctx;
    
    if (sem_cb == NULL) {
        return ERR_BADPARAM;
    }
    
    /* Allocate context structure */
    ctx = malloc(sizeof(SM_GENERIC_CONTEXT));
    if (ctx == NULL) {
        return ERR_NOMEM;
    }
    
    memset(ctx, 0, sizeof(SM_GENERIC_CONTEXT));
    
    /* Initialize POSIX semaphore */
    if (sem_init(&ctx->posix_sem, 0, initial_count) != 0) {
        free(ctx);
        return ERR_NOTSUPPORTED;
    }
    
    ctx->current_count = initial_count;
    ctx->max_count = max_count;
    ctx->wait_count = 0;
    ctx->signal_count = 0;
    
    /* Store context in SCB */
    sem_cb->hw_context = ctx;
    sem_cb->context_size = sizeof(SM_GENERIC_CONTEXT);
    
    sm_hw_generic_state.total_created++;
    
    return 0;
}

static ULONG sm_hw_generic_delete_semaphore(void *scb)
{
    SM_SCB *sem_cb = (SM_SCB *)scb;
    SM_GENERIC_CONTEXT *ctx;
    
    if (sem_cb == NULL || sem_cb->hw_context == NULL) {
        return ERR_BADPARAM;
    }
    
    ctx = (SM_GENERIC_CONTEXT *)sem_cb->hw_context;
    
    /* Destroy POSIX semaphore */
    sem_destroy(&ctx->posix_sem);
    
    /* Free context */
    free(ctx);
    sem_cb->hw_context = NULL;
    sem_cb->context_size = 0;
    
    return 0;
}

static ULONG sm_hw_generic_wait_semaphore(void *scb, ULONG timeout)
{
    SM_SCB *sem_cb = (SM_SCB *)scb;
    SM_GENERIC_CONTEXT *ctx;
    struct timespec abs_timeout;
    int result;
    
    if (sem_cb == NULL || sem_cb->hw_context == NULL) {
        return ERR_BADPARAM;
    }
    
    ctx = (SM_GENERIC_CONTEXT *)sem_cb->hw_context;
    ctx->wait_count++;
    sm_hw_generic_state.total_waits++;
    
    if (timeout == SM_INFINITE_TIMEOUT) {
        /* Wait indefinitely */
        result = sem_wait(&ctx->posix_sem);
    } else {
        /* Wait with timeout */
        clock_gettime(CLOCK_REALTIME, &abs_timeout);
        abs_timeout.tv_sec += timeout / 100;  /* Convert ticks to seconds */
        abs_timeout.tv_nsec += (timeout % 100) * 10000000;  /* Convert remaining to nanoseconds */
        
        if (abs_timeout.tv_nsec >= 1000000000) {
            abs_timeout.tv_sec++;
            abs_timeout.tv_nsec -= 1000000000;
        }
        
        result = sem_timedwait(&ctx->posix_sem, &abs_timeout);
    }
    
    if (result != 0) {
        if (errno == ETIMEDOUT) {
            return ERR_TIMEOUT;
        } else {
            return ERR_NOTSUPPORTED;
        }
    }
    
    if (ctx->current_count > 0) {
        ctx->current_count--;
    }
    
    return 0;
}

static ULONG sm_hw_generic_signal_semaphore(void *scb)
{
    SM_SCB *sem_cb = (SM_SCB *)scb;
    SM_GENERIC_CONTEXT *ctx;
    
    if (sem_cb == NULL || sem_cb->hw_context == NULL) {
        return ERR_BADPARAM;
    }
    
    ctx = (SM_GENERIC_CONTEXT *)sem_cb->hw_context;
    
    if (ctx->current_count >= ctx->max_count) {
        return ERR_SEMFULL;
    }
    
    /* Signal POSIX semaphore */
    if (sem_post(&ctx->posix_sem) != 0) {
        return ERR_NOTSUPPORTED;
    }
    
    ctx->current_count++;
    ctx->signal_count++;
    sm_hw_generic_state.total_signals++;
    
    return 0;
}

static ULONG sm_hw_generic_get_count(void *scb)
{
    SM_SCB *sem_cb = (SM_SCB *)scb;
    SM_GENERIC_CONTEXT *ctx;
    int sval;
    
    if (sem_cb == NULL || sem_cb->hw_context == NULL) {
        return 0;
    }
    
    ctx = (SM_GENERIC_CONTEXT *)sem_cb->hw_context;
    
    if (sem_getvalue(&ctx->posix_sem, &sval) == 0) {
        return (ULONG)sval;
    }
    
    return ctx->current_count;
}

/* Hardware operations structure */
SM_HW_OPS sm_hw_generic_ops = {
    .init = sm_hw_generic_init,
    .create_semaphore = sm_hw_generic_create_semaphore,
    .delete_semaphore = sm_hw_generic_delete_semaphore,
    .wait_semaphore = sm_hw_generic_wait_semaphore,
    .signal_semaphore = sm_hw_generic_signal_semaphore,
    .get_count = sm_hw_generic_get_count
};

/******************************************************************************
* Utility Functions for Testing/Debugging
******************************************************************************/

/* Get generic hardware state for debugging */
void sm_hw_generic_get_state(ULONG *initialized, ULONG *total_created, 
                             ULONG *total_signals, ULONG *total_waits)
{
    if (initialized) *initialized = sm_hw_generic_state.initialized;
    if (total_created) *total_created = sm_hw_generic_state.total_created;
    if (total_signals) *total_signals = sm_hw_generic_state.total_signals;
    if (total_waits) *total_waits = sm_hw_generic_state.total_waits;
}

/* Get semaphore context statistics */
void sm_hw_generic_get_context_stats(void *scb, ULONG *current_count, ULONG *max_count,
                                     ULONG *wait_count, ULONG *signal_count)
{
    SM_SCB *sem_cb = (SM_SCB *)scb;
    SM_GENERIC_CONTEXT *ctx;
    
    if (sem_cb == NULL || sem_cb->hw_context == NULL) {
        return;
    }
    
    ctx = (SM_GENERIC_CONTEXT *)sem_cb->hw_context;
    
    if (current_count) *current_count = ctx->current_count;
    if (max_count) *max_count = ctx->max_count;
    if (wait_count) *wait_count = ctx->wait_count;
    if (signal_count) *signal_count = ctx->signal_count;
}