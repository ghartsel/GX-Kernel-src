/************************************BEGIN*****************************************
*                       COPYRIGHT %G% %U% BY GHWORKS, LLC 
*                             ALL RIGHTS RESERVED
*                         SPDX-License-Identifier: MIT
* ********************************************************************************
* Name:        event_hw_generic.c
* Type:        C Source
* Description: Generic Hardware Event Abstraction Layer
*
* This file provides a portable hardware abstraction for event functionality
* that can work across different platforms without OS-specific dependencies.
* It uses POSIX condition variables and mutexes for event signaling.
*
* For embedded systems, this would be replaced with platform-specific
* implementations (event_hw_stm32f4.c, event_hw_riscv.c, etc.)
*
* Functions:
*   ev_hw_generic_init        - Initialize generic event hardware
*   ev_hw_generic_create      - Create event object
*   ev_hw_generic_delete      - Delete event object
*   ev_hw_generic_signal      - Signal event object
*   ev_hw_generic_wait        - Wait for event
*   ev_hw_generic_clear       - Clear event object
*
**************************************END***************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <time.h>
#include <errno.h>
#include "../../gxkernel.h"
#include "event.h"

/* Generic hardware event context structure */
typedef struct ev_generic_context {
    pthread_mutex_t mutex;          /* Mutex for synchronization */
    pthread_cond_t  condition;      /* Condition variable for waiting */
    volatile int    signaled;       /* Event signaled flag */
    volatile int    waiting;        /* Task waiting flag */
    ULONG           signal_count;   /* Number of times signaled */
    ULONG           wait_count;     /* Number of times waited */
} EV_GENERIC_CONTEXT;

/* Generic hardware state */
static struct {
    ULONG               initialized;
    pthread_mutex_t     global_mutex;
    ULONG               total_events_created;
    ULONG               total_signals;
    ULONG               total_waits;
} ev_hw_generic_state = {0};

/* Forward declarations */
static struct timespec ev_hw_timeout_to_timespec(ULONG timeout_ticks);

/******************************************************************************
* Generic Hardware Event Functions
******************************************************************************/

static ULONG ev_hw_generic_init(void)
{
    if (ev_hw_generic_state.initialized) {
        return 0;
    }
    
    /* Initialize global mutex */
    if (pthread_mutex_init(&ev_hw_generic_state.global_mutex, NULL) != 0) {
        return ERR_NOTSUPPORTED;
    }
    
    ev_hw_generic_state.total_events_created = 0;
    ev_hw_generic_state.total_signals = 0;
    ev_hw_generic_state.total_waits = 0;
    ev_hw_generic_state.initialized = 1;
    
    return 0;
}

static ULONG ev_hw_generic_create_event(void *ecb)
{
    EV_ECB *event_cb = (EV_ECB *)ecb;
    EV_GENERIC_CONTEXT *ctx;
    
    if (event_cb == NULL) {
        return ERR_BADPARAM;
    }
    
    /* Allocate context structure */
    ctx = malloc(sizeof(EV_GENERIC_CONTEXT));
    if (ctx == NULL) {
        return ERR_NOMEM;
    }
    
    memset(ctx, 0, sizeof(EV_GENERIC_CONTEXT));
    
    /* Initialize synchronization primitives */
    if (pthread_mutex_init(&ctx->mutex, NULL) != 0) {
        free(ctx);
        return ERR_NOTSUPPORTED;
    }
    
    if (pthread_cond_init(&ctx->condition, NULL) != 0) {
        pthread_mutex_destroy(&ctx->mutex);
        free(ctx);
        return ERR_NOTSUPPORTED;
    }
    
    ctx->signaled = 0;
    ctx->waiting = 0;
    ctx->signal_count = 0;
    ctx->wait_count = 0;
    
    /* Store context in ECB */
    event_cb->hw_context = ctx;
    event_cb->context_size = sizeof(EV_GENERIC_CONTEXT);
    
    ev_hw_generic_state.total_events_created++;
    
    return 0;
}

static ULONG ev_hw_generic_delete_event(void *ecb)
{
    EV_ECB *event_cb = (EV_ECB *)ecb;
    EV_GENERIC_CONTEXT *ctx;
    
    if (event_cb == NULL || event_cb->hw_context == NULL) {
        return ERR_BADPARAM;
    }
    
    ctx = (EV_GENERIC_CONTEXT *)event_cb->hw_context;
    
    /* Wake up any waiting threads */
    pthread_mutex_lock(&ctx->mutex);
    ctx->signaled = 1;
    pthread_cond_broadcast(&ctx->condition);
    pthread_mutex_unlock(&ctx->mutex);
    
    /* Clean up synchronization primitives */
    pthread_cond_destroy(&ctx->condition);
    pthread_mutex_destroy(&ctx->mutex);
    
    /* Free context */
    free(ctx);
    event_cb->hw_context = NULL;
    event_cb->context_size = 0;
    
    return 0;
}

static ULONG ev_hw_generic_signal_event(void *ecb)
{
    EV_ECB *event_cb = (EV_ECB *)ecb;
    EV_GENERIC_CONTEXT *ctx;
    
    if (event_cb == NULL || event_cb->hw_context == NULL) {
        return ERR_BADPARAM;
    }
    
    ctx = (EV_GENERIC_CONTEXT *)event_cb->hw_context;
    
    pthread_mutex_lock(&ctx->mutex);
    
    ctx->signaled = 1;
    ctx->signal_count++;
    ev_hw_generic_state.total_signals++;
    
    /* Wake up waiting thread */
    if (ctx->waiting) {
        pthread_cond_signal(&ctx->condition);
    }
    
    pthread_mutex_unlock(&ctx->mutex);
    
    return 0;
}

static ULONG ev_hw_generic_wait_event(void *ecb, ULONG timeout)
{
    EV_ECB *event_cb = (EV_ECB *)ecb;
    EV_GENERIC_CONTEXT *ctx;
    struct timespec abs_timeout;
    int result;
    ULONG error = 0;
    
    if (event_cb == NULL || event_cb->hw_context == NULL) {
        return ERR_BADPARAM;
    }
    
    ctx = (EV_GENERIC_CONTEXT *)event_cb->hw_context;
    
    pthread_mutex_lock(&ctx->mutex);
    
    ctx->waiting = 1;
    ctx->wait_count++;
    ev_hw_generic_state.total_waits++;
    
    /* Check if already signaled */
    if (ctx->signaled) {
        ctx->signaled = 0;  /* Auto-reset event */
        ctx->waiting = 0;
        pthread_mutex_unlock(&ctx->mutex);
        return 0;
    }
    
    /* Wait with timeout */
    if (timeout == EV_INFINITE_TIMEOUT) {
        /* Wait indefinitely */
        while (!ctx->signaled && error == 0) {
            result = pthread_cond_wait(&ctx->condition, &ctx->mutex);
            if (result != 0) {
                error = ERR_NOTSUPPORTED;
            }
        }
    } else {
        /* Wait with timeout */
        abs_timeout = ev_hw_timeout_to_timespec(timeout);
        
        while (!ctx->signaled && error == 0) {
            result = pthread_cond_timedwait(&ctx->condition, &ctx->mutex, &abs_timeout);
            if (result == ETIMEDOUT) {
                error = ERR_TIMEOUT;
                break;
            } else if (result != 0) {
                error = ERR_NOTSUPPORTED;
                break;
            }
        }
    }
    
    if (error == 0 && ctx->signaled) {
        ctx->signaled = 0;  /* Auto-reset event */
    }
    
    ctx->waiting = 0;
    pthread_mutex_unlock(&ctx->mutex);
    
    return error;
}

static ULONG ev_hw_generic_clear_event(void *ecb)
{
    EV_ECB *event_cb = (EV_ECB *)ecb;
    EV_GENERIC_CONTEXT *ctx;
    
    if (event_cb == NULL || event_cb->hw_context == NULL) {
        return ERR_BADPARAM;
    }
    
    ctx = (EV_GENERIC_CONTEXT *)event_cb->hw_context;
    
    pthread_mutex_lock(&ctx->mutex);
    ctx->signaled = 0;
    pthread_mutex_unlock(&ctx->mutex);
    
    return 0;
}

/* Hardware operations structure */
EV_HW_OPS ev_hw_generic_ops = {
    .init = ev_hw_generic_init,
    .create_event = ev_hw_generic_create_event,
    .delete_event = ev_hw_generic_delete_event,
    .signal_event = ev_hw_generic_signal_event,
    .wait_event = ev_hw_generic_wait_event,
    .clear_event = ev_hw_generic_clear_event
};

/******************************************************************************
* Helper Functions
******************************************************************************/

static struct timespec ev_hw_timeout_to_timespec(ULONG timeout_ticks)
{
    struct timespec ts, current_time;
    ULONG timeout_ms = EV_TICKS_TO_MS(timeout_ticks);
    
    /* Get current time */
    clock_gettime(CLOCK_REALTIME, &current_time);
    
    /* Add timeout */
    ts.tv_sec = current_time.tv_sec + (timeout_ms / 1000);
    ts.tv_nsec = current_time.tv_nsec + ((timeout_ms % 1000) * 1000000);
    
    /* Handle nanosecond overflow */
    if (ts.tv_nsec >= 1000000000) {
        ts.tv_sec++;
        ts.tv_nsec -= 1000000000;
    }
    
    return ts;
}

/******************************************************************************
* Utility Functions for Testing/Debugging
******************************************************************************/

/* Get generic hardware state for debugging */
void ev_hw_generic_get_state(ULONG *initialized, ULONG *events_created, 
                             ULONG *total_signals, ULONG *total_waits)
{
    if (initialized) *initialized = ev_hw_generic_state.initialized;
    if (events_created) *events_created = ev_hw_generic_state.total_events_created;
    if (total_signals) *total_signals = ev_hw_generic_state.total_signals;
    if (total_waits) *total_waits = ev_hw_generic_state.total_waits;
}

/* Get event context statistics */
void ev_hw_generic_get_context_stats(void *ecb, ULONG *signal_count, 
                                     ULONG *wait_count, ULONG *is_signaled)
{
    EV_ECB *event_cb = (EV_ECB *)ecb;
    EV_GENERIC_CONTEXT *ctx;
    
    if (event_cb == NULL || event_cb->hw_context == NULL) {
        return;
    }
    
    ctx = (EV_GENERIC_CONTEXT *)event_cb->hw_context;
    
    pthread_mutex_lock(&ctx->mutex);
    
    if (signal_count) *signal_count = ctx->signal_count;
    if (wait_count) *wait_count = ctx->wait_count;
    if (is_signaled) *is_signaled = ctx->signaled ? 1 : 0;
    
    pthread_mutex_unlock(&ctx->mutex);
}

/* Force signal an event (for testing) */
void ev_hw_generic_force_signal(void *ecb)
{
    ev_hw_generic_signal_event(ecb);
}

/* Check if event is currently signaled */
ULONG ev_hw_generic_is_signaled(void *ecb)
{
    EV_ECB *event_cb = (EV_ECB *)ecb;
    EV_GENERIC_CONTEXT *ctx;
    ULONG signaled;
    
    if (event_cb == NULL || event_cb->hw_context == NULL) {
        return 0;
    }
    
    ctx = (EV_GENERIC_CONTEXT *)event_cb->hw_context;
    
    pthread_mutex_lock(&ctx->mutex);
    signaled = ctx->signaled ? 1 : 0;
    pthread_mutex_unlock(&ctx->mutex);
    
    return signaled;
}