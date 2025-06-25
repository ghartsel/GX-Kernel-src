/************************************BEGIN*****************************************
*                       COPYRIGHT %G% %U% BY GHWORKS, LLC 
*                             ALL RIGHTS RESERVED
*                         SPDX-License-Identifier: MIT
* ********************************************************************************
* Name:        queue_hw_generic.c
* Type:        C Source
* Description: Generic Hardware Abstraction for Queue Services
*
* This module provides a portable implementation of queue hardware abstraction
* using POSIX-compatible synchronization primitives. This implementation is
* suitable for development hosts, simulation environments, and platforms
* without specialized hardware queue support.
*
* Key Features:
* - POSIX mutex and condition variable based synchronization
* - Software-only message queue implementation
* - Thread-safe operations
* - Timeout support using relative timing
* - Compatible with pthread-based systems
*
**************************************END***************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <pthread.h>
#include <sys/time.h>
#include "../../gxkernel.h"
#include "../../gxkCfg.h"
#include "queue.h"

/******************************************************************************
* Platform-specific includes and definitions
******************************************************************************/

#ifdef _WIN32
    /* Windows compatibility layer */
        #define PTHREAD_MUTEX_INITIALIZER PTHREAD_MUTEX_INITIALIZER
    typedef HANDLE pthread_mutex_t;
    typedef HANDLE pthread_cond_t;
    typedef DWORD pthread_t;
#else
    /* POSIX systems */
    #include <pthread.h>
    #include <unistd.h>
#endif

/******************************************************************************
* Hardware abstraction state
******************************************************************************/

typedef struct {
    pthread_mutex_t global_mutex;      /* Global queue system mutex */
    pthread_cond_t  global_cond;       /* Global condition variable */
    ULONG           initialized;       /* Initialization flag */
    ULONG           total_operations;  /* Total queue operations */
    ULONG           active_waits;      /* Number of active waiters */
} Q_HW_GENERIC_STATE;

static Q_HW_GENERIC_STATE q_hw_state = {
    .global_mutex = PTHREAD_MUTEX_INITIALIZER,
    .global_cond = PTHREAD_COND_INITIALIZER,
    .initialized = 0,
    .total_operations = 0,
    .active_waits = 0
};

/******************************************************************************
* Utility Functions
******************************************************************************/

static ULONG q_hw_get_current_time_ms(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (ULONG)(tv.tv_sec * 1000 + tv.tv_usec / 1000);
}

static void q_hw_add_timeout_ms(struct timespec *abstime, ULONG timeout_ms)
{
    struct timeval now;
    gettimeofday(&now, NULL);
    
    abstime->tv_sec = now.tv_sec + (timeout_ms / 1000);
    abstime->tv_nsec = (now.tv_usec * 1000) + ((timeout_ms % 1000) * 1000000);
    
    if (abstime->tv_nsec >= 1000000000) {
        abstime->tv_sec++;
        abstime->tv_nsec -= 1000000000;
    }
}

static void q_hw_lock_global(void)
{
    pthread_mutex_lock(&q_hw_state.global_mutex);
}

static void q_hw_unlock_global(void)
{
    pthread_mutex_unlock(&q_hw_state.global_mutex);
}

static ULONG q_hw_wait_condition(ULONG timeout_ms)
{
    struct timespec abstime;
    int result;
    
    q_hw_state.active_waits++;
    
    if (timeout_ms == 0) {
        /* Wait indefinitely */
        result = pthread_cond_wait(&q_hw_state.global_cond, &q_hw_state.global_mutex);
    } else {
        /* Wait with timeout */
        q_hw_add_timeout_ms(&abstime, timeout_ms);
        result = pthread_cond_timedwait(&q_hw_state.global_cond, &q_hw_state.global_mutex, &abstime);
    }
    
    q_hw_state.active_waits--;
    
    if (result == ETIMEDOUT) {
        return ERR_TIMEOUT;
    } else if (result != 0) {
        return ERR_INTERNAL;
    }
    
    return 0;
}

static void q_hw_signal_condition(void)
{
    pthread_cond_broadcast(&q_hw_state.global_cond);
}

/******************************************************************************
* Hardware Abstraction Implementation
******************************************************************************/

static ULONG q_hw_generic_init(void)
{
    int result;
    
    if (q_hw_state.initialized) {
        return 0;
    }
    
    /* Initialize mutex and condition variable */
    result = pthread_mutex_init(&q_hw_state.global_mutex, NULL);
    if (result != 0) {
        return ERR_NORESOURCE;
    }
    
    result = pthread_cond_init(&q_hw_state.global_cond, NULL);
    if (result != 0) {
        pthread_mutex_destroy(&q_hw_state.global_mutex);
        return ERR_NORESOURCE;
    }
    
    q_hw_state.initialized = 1;
    q_hw_state.total_operations = 0;
    q_hw_state.active_waits = 0;
    
    return 0;
}

static ULONG q_hw_generic_create_queue(Q_QCB *qcb)
{
    if (!Q_IS_VALID_QCB(qcb)) {
        return ERR_BADPARAM;
    }
    
    q_hw_lock_global();
    
    /* Generic implementation doesn't need special queue creation */
    /* All synchronization is handled through the global mutex/condition */
    
    q_hw_state.total_operations++;
    
    q_hw_unlock_global();
    
    return 0;
}

static ULONG q_hw_generic_delete_queue(Q_QCB *qcb)
{
    if (!Q_IS_VALID_QCB(qcb)) {
        return ERR_BADPARAM;
    }
    
    q_hw_lock_global();
    
    /* Wake up any tasks waiting on this queue */
    q_hw_signal_condition();
    
    q_hw_state.total_operations++;
    
    q_hw_unlock_global();
    
    return 0;
}

static ULONG q_hw_generic_send_message(Q_QCB *qcb, ULONG msg[Q_MSG_SIZE])
{
    ULONG error;
    
    if (!Q_IS_VALID_QCB(qcb) || !Q_IS_VALID_MSG(msg)) {
        return ERR_BADPARAM;
    }
    
    q_hw_lock_global();
    
    /* Perform the actual message enqueue operation */
    error = q_message_enqueue(qcb, msg, 0);
    
    if (error == 0) {
        /* Signal waiting receivers */
        q_hw_signal_condition();
        q_hw_state.total_operations++;
    }
    
    q_hw_unlock_global();
    
    return error;
}

static ULONG q_hw_generic_receive_message(Q_QCB *qcb, ULONG msg[Q_MSG_SIZE], ULONG timeout)
{
    ULONG error;
    ULONG wait_start;
    ULONG elapsed;
    
    if (!Q_IS_VALID_QCB(qcb) || !Q_IS_VALID_MSG(msg)) {
        return ERR_BADPARAM;
    }
    
    q_hw_lock_global();
    
    wait_start = q_hw_get_current_time_ms();
    
    /* Check if message is immediately available */
    error = q_message_dequeue(qcb, msg);
    if (error == 0) {
        q_hw_state.total_operations++;
        q_hw_unlock_global();
        return 0;
    }
    
    /* No message available - need to wait */
    if (error != ERR_NOMSG) {
        q_hw_unlock_global();
        return error;
    }
    
    /* Wait for message with timeout */
    while (1) {
        /* Check timeout */
        if (timeout > 0) {
            elapsed = q_hw_get_current_time_ms() - wait_start;
            if (elapsed >= timeout) {
                q_hw_unlock_global();
                return ERR_TIMEOUT;
            }
        }
        
        /* Wait for signal */
        error = q_hw_wait_condition(timeout > 0 ? (timeout - elapsed) : 0);
        if (error == ERR_TIMEOUT) {
            q_hw_unlock_global();
            return ERR_TIMEOUT;
        } else if (error != 0) {
            q_hw_unlock_global();
            return error;
        }
        
        /* Try to dequeue message again */
        error = q_message_dequeue(qcb, msg);
        if (error == 0) {
            q_hw_state.total_operations++;
            q_hw_unlock_global();
            return 0;
        }
        
        /* Continue waiting if still no message */
        if (error != ERR_NOMSG) {
            q_hw_unlock_global();
            return error;
        }
    }
}

static ULONG q_hw_generic_broadcast_message(Q_QCB *qcb, ULONG msg[Q_MSG_SIZE], ULONG *count)
{
    ULONG error;
    
    if (!Q_IS_VALID_QCB(qcb) || !Q_IS_VALID_MSG(msg) || count == NULL) {
        return ERR_BADPARAM;
    }
    
    q_hw_lock_global();
    
    /* Generic implementation: just send one message */
    /* A more sophisticated implementation could maintain a list of waiting tasks */
    error = q_message_enqueue(qcb, msg, 0);
    
    if (error == 0) {
        *count = (q_hw_state.active_waits > 0) ? 1 : 0;
        
        /* Wake up all waiting tasks */
        q_hw_signal_condition();
        q_hw_state.total_operations++;
    } else {
        *count = 0;
    }
    
    q_hw_unlock_global();
    
    return error;
}

static void q_hw_generic_cleanup(void)
{
    if (!q_hw_state.initialized) {
        return;
    }
    
    q_hw_lock_global();
    
    /* Wake up all waiting tasks */
    q_hw_signal_condition();
    
    q_hw_unlock_global();
    
    /* Destroy synchronization objects */
    pthread_cond_destroy(&q_hw_state.global_cond);
    pthread_mutex_destroy(&q_hw_state.global_mutex);
    
    q_hw_state.initialized = 0;
}

/******************************************************************************
* Hardware Operations Table
******************************************************************************/

Q_HW_OPS q_hw_generic_ops = {
    .init = q_hw_generic_init,
    .create_queue = q_hw_generic_create_queue,
    .delete_queue = q_hw_generic_delete_queue,
    .send_message = q_hw_generic_send_message,
    .receive_message = q_hw_generic_receive_message,
    .broadcast_message = q_hw_generic_broadcast_message,
    .cleanup = q_hw_generic_cleanup
};

/******************************************************************************
* Debug and Diagnostics Functions
******************************************************************************/

ULONG q_hw_generic_get_statistics(ULONG *operations, ULONG *active_waits)
{
    q_hw_lock_global();
    
    if (operations) {
        *operations = q_hw_state.total_operations;
    }
    
    if (active_waits) {
        *active_waits = q_hw_state.active_waits;
    }
    
    q_hw_unlock_global();
    
    return 0;
}

void q_hw_generic_reset_statistics(void)
{
    q_hw_lock_global();
    q_hw_state.total_operations = 0;
    q_hw_unlock_global();
}

ULONG q_hw_generic_test_condition_variable(void)
{
    ULONG error;
    
    q_hw_lock_global();
    
    /* Test signal/wait functionality */
    q_hw_signal_condition();
    
    /* Test timeout functionality */
    error = q_hw_wait_condition(1); /* 1ms timeout */
    
    q_hw_unlock_global();
    
    return (error == ERR_TIMEOUT) ? 0 : ERR_INTERNAL;
}