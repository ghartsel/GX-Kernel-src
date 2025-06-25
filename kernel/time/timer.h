/************************************BEGIN*****************************************
*                       COPYRIGHT %G% %U% BY GHWORKS, LLC 
*                             ALL RIGHTS RESERVED
*                         SPDX-License-Identifier: MIT
* ********************************************************************************
* Name:        timer.h
* Type:        C Header (Private)
* Description: Internal Timer Services Interface
*
* Private structures, constants, and function prototypes for timer subsystem.
* This header is NOT part of the public API.
*
**************************************END***************************************/

#ifndef _TIMER_H_
#define _TIMER_H_

#include "../../gxkernel.h"

/* Timer Configuration Constants */
#define TM_MAX_TIMERS       64      /* Maximum concurrent timers */
#define TM_INVALID_ID       0       /* Invalid timer ID */
#define TM_POOL_MAGIC       0x544D  /* "TM" magic for validation */
#define TM_TICKS_PER_SEC    100     /* Default system tick rate (Hz) */

/* Timer Types */
#define TM_TYPE_ONESHOT     1       /* Single-shot timer */
#define TM_TYPE_PERIODIC    2       /* Repeating timer */
#define TM_TYPE_ABSOLUTE    3       /* Absolute time timer */

/* Timer States */
#define TM_STATE_FREE       0       /* Timer entry available */
#define TM_STATE_ACTIVE     1       /* Timer is running */
#define TM_STATE_EXPIRED    2       /* Timer has expired (pending processing) */
#define TM_STATE_CANCELLED  3       /* Timer was cancelled */

/* Timer Action Types */
#define TM_ACTION_EVENT     1       /* Send events to task */
#define TM_ACTION_WAKEUP    2       /* Wake up sleeping task */

/* Hardware abstraction function pointers */
typedef struct tm_hw_ops {
    ULONG (*init)(void);                    /* Initialize hardware timer */
    ULONG (*get_ticks)(void);              /* Get current tick count */
    ULONG (*set_alarm)(ULONG ticks);       /* Set next alarm tick */
    void  (*enable_interrupt)(void);        /* Enable timer interrupt */
    void  (*disable_interrupt)(void);       /* Disable timer interrupt */
} TM_HW_OPS;

/* Timer Control Block */
typedef struct tm_tcb {
    ULONG       magic;          /* Validation magic number */
    ULONG       timer_id;       /* Unique timer identifier */
    ULONG       state;          /* Current timer state */
    ULONG       type;           /* Timer type (oneshot/periodic/absolute) */
    ULONG       action;         /* Action to take on expiration */
    
    /* Timing Information */
    ULONG       start_ticks;    /* Tick count when timer was started */
    ULONG       delay_ticks;    /* Delay in ticks (relative timers) */
    ULONG       expire_ticks;   /* Absolute tick count for expiration */
    ULONG       period_ticks;   /* Period for repeating timers */
    
    /* Absolute time fields (for tm_evwhen/tm_wkwhen) */
    ULONG       target_date;    /* Target date */
    ULONG       target_time;    /* Target time */
    ULONG       target_tick;    /* Target tick within second */
    
    /* Action-specific data */
    ULONG       task_id;        /* Task ID for wakeup/events */
    ULONG       events;         /* Event mask to send */
    
    /* List management */
    struct tm_tcb *next;        /* Next timer in active list */
    struct tm_tcb *prev;        /* Previous timer in active list */
} TM_TCB;

/* Timer Pool Management */
typedef struct tm_pool {
    ULONG       magic;              /* Pool validation magic */
    ULONG       max_timers;         /* Maximum number of timers */
    ULONG       free_count;         /* Number of free timer blocks */
    ULONG       next_id;            /* Next timer ID to assign */
    TM_TCB      *free_list;         /* Free timer block list */
    TM_TCB      *active_list;       /* Active timer list (sorted by expiration) */
    TM_TCB      timers[TM_MAX_TIMERS];  /* Timer control blocks */
} TM_POOL;

/* System Time Structure */
typedef struct tm_systime {
    ULONG       date;           /* Current date */
    ULONG       time;           /* Current time */
    ULONG       ticks;          /* Current tick within second */
    ULONG       tick_count;     /* Absolute tick counter since boot */
    ULONG       ticks_per_sec;  /* System tick rate */
} TM_SYSTIME;

/* Global Timer State */
typedef struct tm_state {
    TM_POOL     pool;           /* Timer pool */
    TM_SYSTIME  systime;        /* System time */
    TM_HW_OPS   *hw_ops;        /* Hardware operations */
    ULONG       initialized;    /* Initialization flag */
    ULONG       interrupt_count; /* Timer interrupt counter */
} TM_STATE;

/* Internal Function Prototypes */

/* Pool Management */
ULONG tm_pool_init(void);
TM_TCB *tm_pool_alloc(void);
ULONG tm_pool_free(TM_TCB *tcb);
TM_TCB *tm_pool_find(ULONG timer_id);

/* List Management */
void tm_list_insert(TM_TCB *tcb);
void tm_list_remove(TM_TCB *tcb);
TM_TCB *tm_list_get_expired(void);

/* Time Management */
ULONG tm_time_init(void);
ULONG tm_time_update(void);
ULONG tm_time_to_ticks(ULONG date, ULONG time, ULONG ticks);
void tm_ticks_to_time(ULONG tick_count, ULONG *date, ULONG *time, ULONG *ticks);

/* Timer Processing */
ULONG tm_process_expired(void);
ULONG tm_schedule_next(void);
ULONG tm_fire_timer(TM_TCB *tcb);

/* Hardware Abstraction */
ULONG tm_hw_init(void);
extern TM_HW_OPS tm_hw_generic_ops;

/* Utility Functions */
ULONG tm_validate_tcb(TM_TCB *tcb);
ULONG tm_generate_id(void);

/* Global State (defined in timer.c) */
extern TM_STATE tm_global_state;

/* Macros for common operations */
#define TM_GET_STATE()          (&tm_global_state)
#define TM_GET_POOL()           (&tm_global_state.pool)
#define TM_GET_SYSTIME()        (&tm_global_state.systime)
#define TM_IS_VALID_TCB(tcb)    ((tcb) && (tcb)->magic == TM_POOL_MAGIC)
#define TM_GET_CURRENT_TICKS()  (tm_global_state.systime.tick_count)

/* Debug/Statistics Macros (compile-time configurable) */
#ifdef TM_DEBUG
#define TM_DEBUG_PRINT(fmt, ...) printf("[TM] " fmt "\n", ##__VA_ARGS__)
#else
#define TM_DEBUG_PRINT(fmt, ...)
#endif

#endif /* _TIMER_H_ */