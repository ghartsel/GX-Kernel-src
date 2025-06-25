/************************************BEGIN*****************************************
*                       COPYRIGHT %G% %U% BY GHWORKS, LLC 
*                             ALL RIGHTS RESERVED
*                         SPDX-License-Identifier: MIT
* ********************************************************************************
* Name:        timer_hw_generic.c
* Type:        C Source
* Description: Generic Hardware Timer Abstraction Layer
*
* This file provides a portable hardware abstraction for timer functionality
* that can work across different platforms without OS-specific dependencies.
*
* For embedded systems, this would be replaced with platform-specific
* implementations (timer_hw_arm.c, timer_hw_riscv.c, etc.)
*
* Functions:
*   tm_hw_generic_init        - Initialize generic timer hardware
*   tm_hw_generic_get_ticks   - Get current tick count
*   tm_hw_generic_set_alarm   - Set next alarm
*   tm_hw_generic_enable_int  - Enable timer interrupt
*   tm_hw_generic_disable_int - Disable timer interrupt
*   tm_hw_init                - Initialize hardware layer
*
**************************************END***************************************/

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <signal.h>
#include <sys/time.h>
#include <unistd.h>
#include "../../gxkernel.h"
#include "timer.h"

/* Generic hardware state */
static struct {
    ULONG           initialized;
    ULONG           tick_count;
    ULONG           ticks_per_sec;
    struct timespec boot_time;
    struct timespec last_time;
    timer_t         alarm_timer;
    ULONG           alarm_set;
} tm_hw_state = {0};

/* Forward declarations */
static void tm_hw_alarm_handler(int sig);
static ULONG tm_hw_get_monotonic_ticks(void);

/******************************************************************************
* Generic Hardware Timer Functions
******************************************************************************/

static ULONG tm_hw_generic_init(void)
{
    struct sigaction sa;
    struct sigevent se;
    
    if (tm_hw_state.initialized) {
        return 0;
    }
    
    /* Initialize state */
    tm_hw_state.tick_count = 0;
    tm_hw_state.ticks_per_sec = TM_TICKS_PER_SEC;
    tm_hw_state.alarm_set = 0;
    
    /* Record boot time */
    if (clock_gettime(CLOCK_MONOTONIC, &tm_hw_state.boot_time) != 0) {
        return ERR_NOTSUPPORTED;
    }
    tm_hw_state.last_time = tm_hw_state.boot_time;
    
    /* Set up signal handler for alarm */
    sa.sa_handler = tm_hw_alarm_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    
    if (sigaction(SIGALRM, &sa, NULL) != 0) {
        return ERR_NOTSUPPORTED;
    }
    
    /* Create timer */
    se.sigev_notify = SIGEV_SIGNAL;
    se.sigev_signo = SIGALRM;
    se.sigev_value.sival_ptr = &tm_hw_state.alarm_timer;
    
    if (timer_create(CLOCK_MONOTONIC, &se, &tm_hw_state.alarm_timer) != 0) {
        return ERR_NOTSUPPORTED;
    }
    
    tm_hw_state.initialized = 1;
    
    return 0;
}

static ULONG tm_hw_generic_get_ticks(void)
{
    if (!tm_hw_state.initialized) {
        return 0;
    }
    
    return tm_hw_get_monotonic_ticks();
}

static ULONG tm_hw_generic_set_alarm(ULONG target_ticks)
{
    struct itimerspec its;
    ULONG current_ticks, ticks_to_wait;
    ULONG seconds, nanoseconds;
    
    if (!tm_hw_state.initialized) {
        return ERR_NOTINIT;
    }
    
    current_ticks = tm_hw_get_monotonic_ticks();
    
    if (target_ticks <= current_ticks) {
        /* Alarm is in the past or now - fire immediately */
        ticks_to_wait = 1;
    } else {
        ticks_to_wait = target_ticks - current_ticks;
    }
    
    /* Convert ticks to time */
    seconds = ticks_to_wait / tm_hw_state.ticks_per_sec;
    nanoseconds = ((ticks_to_wait % tm_hw_state.ticks_per_sec) * 1000000000UL) / tm_hw_state.ticks_per_sec;
    
    /* Set up one-shot timer */
    its.it_value.tv_sec = seconds;
    its.it_value.tv_nsec = nanoseconds;
    its.it_interval.tv_sec = 0;
    its.it_interval.tv_nsec = 0;
    
    if (timer_settime(tm_hw_state.alarm_timer, 0, &its, NULL) != 0) {
        return ERR_NOTSUPPORTED;
    }
    
    tm_hw_state.alarm_set = 1;
    
    return 0;
}

static void tm_hw_generic_enable_int(void)
{
    /* In generic implementation, interrupts are always "enabled" */
    /* On real hardware, this would enable timer interrupt in NVIC/etc */
}

static void tm_hw_generic_disable_int(void)
{
    /* In generic implementation, we can cancel pending alarms */
    if (tm_hw_state.initialized && tm_hw_state.alarm_set) {
        struct itimerspec its;
        its.it_value.tv_sec = 0;
        its.it_value.tv_nsec = 0;
        its.it_interval.tv_sec = 0;
        its.it_interval.tv_nsec = 0;
        
        timer_settime(tm_hw_state.alarm_timer, 0, &its, NULL);
        tm_hw_state.alarm_set = 0;
    }
}

/* Hardware operations structure */
TM_HW_OPS tm_hw_generic_ops = {
    .init = tm_hw_generic_init,
    .get_ticks = tm_hw_generic_get_ticks,
    .set_alarm = tm_hw_generic_set_alarm,
    .enable_interrupt = tm_hw_generic_enable_int,
    .disable_interrupt = tm_hw_generic_disable_int
};

/******************************************************************************
* Helper Functions
******************************************************************************/

static ULONG tm_hw_get_monotonic_ticks(void)
{
    struct timespec current_time;
    ULONG elapsed_sec, elapsed_nsec;
    ULONG total_ticks;
    
    if (clock_gettime(CLOCK_MONOTONIC, &current_time) != 0) {
        return tm_hw_state.tick_count;
    }
    
    /* Calculate elapsed time since boot */
    elapsed_sec = current_time.tv_sec - tm_hw_state.boot_time.tv_sec;
    if (current_time.tv_nsec >= tm_hw_state.boot_time.tv_nsec) {
        elapsed_nsec = current_time.tv_nsec - tm_hw_state.boot_time.tv_nsec;
    } else {
        elapsed_sec--;
        elapsed_nsec = 1000000000UL + current_time.tv_nsec - tm_hw_state.boot_time.tv_nsec;
    }
    
    /* Convert to ticks */
    total_ticks = (elapsed_sec * tm_hw_state.ticks_per_sec) + 
                  (elapsed_nsec * tm_hw_state.ticks_per_sec) / 1000000000UL;
    
    return total_ticks;
}

static void tm_hw_alarm_handler(int sig)
{
    (void)sig;  /* Unused parameter */
    
    tm_hw_state.alarm_set = 0;
    
    /* Update tick count */
    tm_hw_state.tick_count = tm_hw_get_monotonic_ticks();
    
    /* Call the kernel timer tick handler */
    tm_tick();
}

/******************************************************************************
* Public Hardware Initialization
******************************************************************************/

ULONG tm_hw_init(void)
{
    TM_STATE *state = TM_GET_STATE();
    
    /* Set up hardware operations */
    state->hw_ops = &tm_hw_generic_ops;
    
    /* Initialize hardware */
    if (state->hw_ops->init) {
        return state->hw_ops->init();
    }
    
    return 0;
}

/******************************************************************************
* Additional Utility Functions for Testing/Debugging
******************************************************************************/

/* Function to get hardware state for debugging */
void tm_hw_get_state(ULONG *initialized, ULONG *tick_count, ULONG *ticks_per_sec)
{
    if (initialized) *initialized = tm_hw_state.initialized;
    if (tick_count) *tick_count = tm_hw_state.tick_count;
    if (ticks_per_sec) *ticks_per_sec = tm_hw_state.ticks_per_sec;
}

/* Function to force a tick (for testing) */
void tm_hw_force_tick(void)
{
    tm_hw_alarm_handler(SIGALRM);
}

/* Function to check if alarm is set */
ULONG tm_hw_is_alarm_set(void)
{
    return tm_hw_state.alarm_set;
}

/* Function to set tick rate (for testing different rates) */
ULONG tm_hw_set_tick_rate(ULONG ticks_per_sec)
{
    if (ticks_per_sec == 0 || ticks_per_sec > 10000) {
        return ERR_BADPARAM;
    }
    
    tm_hw_state.ticks_per_sec = ticks_per_sec;
    return 0;
}