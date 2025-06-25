/************************************BEGIN*****************************************
*                       COPYRIGHT %G% %U% BY GHWORKS, LLC 
*                             ALL RIGHTS RESERVED
*                         SPDX-License-Identifier: MIT
* ********************************************************************************
* Name:        event.h
* Type:        C Header (Private)
* Description: Internal Event Services Interface
*
* Private structures, constants, and function prototypes for event subsystem.
* This header is NOT part of the public API.
*
**************************************END***************************************/

#ifndef _EVENT_H_
#define _EVENT_H_

#include "../../gxkernel.h"

/* Event Configuration Constants */
#define EV_MAX_EVENTS       32      /* Maximum number of event bits (32-bit mask) */
#define EV_INVALID_MASK     0       /* Invalid event mask */
#define EV_POOL_MAGIC       0x4556  /* "EV" magic for validation */
#define EV_ALL_EVENTS       0xFFFFFFFFUL /* All possible events */

/* Event Control Block States */
#define EV_STATE_FREE       0       /* Event control block available */
#define EV_STATE_WAITING    1       /* Task waiting for events */
#define EV_STATE_SIGNALED   2       /* Events have been signaled */

/* Event Wait Conditions (from gxkernel.h but defined here for clarity) */
#ifndef EV_ANY
#define EV_ANY              0x01    /* Wait for ANY of the specified events */
#endif
#ifndef EV_ALL  
#define EV_ALL              0x00    /* Wait for ALL of the specified events */
#endif
#ifndef EV_NOWAIT
#define EV_NOWAIT           0x02    /* Non-blocking event receive */
#endif

/* Hardware abstraction function pointers */
typedef struct ev_hw_ops {
    ULONG (*init)(void);                    /* Initialize hardware */
    ULONG (*create_event)(void *ecb);       /* Create event object */
    ULONG (*delete_event)(void *ecb);       /* Delete event object */
    ULONG (*signal_event)(void *ecb);       /* Signal event object */
    ULONG (*wait_event)(void *ecb, ULONG timeout); /* Wait for event */
    ULONG (*clear_event)(void *ecb);        /* Clear event object */
} EV_HW_OPS;

/* Event Control Block */
typedef struct ev_ecb {
    ULONG       magic;              /* Validation magic number */
    ULONG       task_id;            /* Task ID that owns this event block */
    ULONG       state;              /* Current state of event block */
    
    /* Event Masks */
    ULONG       pending_events;     /* Events that have been sent but not received */
    ULONG       waiting_events;     /* Events this task is waiting for */
    ULONG       received_events;    /* Events received in last ev_receive call */
    
    /* Wait Conditions */
    ULONG       wait_condition;     /* EV_ANY or EV_ALL */
    ULONG       wait_flags;         /* Additional wait flags (EV_NOWAIT, etc.) */
    ULONG       timeout_ticks;      /* Timeout in system ticks */
    ULONG       wait_start_time;    /* Time when wait started */
    
    /* Statistics */
    ULONG       events_sent;        /* Total events sent to this task */
    ULONG       events_received;    /* Total events received by this task */
    ULONG       wait_count;         /* Number of times task has waited */
    ULONG       timeout_count;      /* Number of timeouts */
    
    /* Hardware-Specific Context */
    void        *hw_context;        /* Hardware-specific event context */
    ULONG       context_size;       /* Size of hardware context */
} EV_ECB;

/* Event Pool Management */
typedef struct ev_pool {
    ULONG       magic;              /* Pool validation magic */
    ULONG       max_tasks;          /* Maximum number of tasks */
    ULONG       active_count;       /* Number of active event blocks */
    EV_ECB      event_blocks[MAX_TASK]; /* Event control blocks (one per task) */
} EV_POOL;

/* Global Event State */
typedef struct ev_state {
    EV_POOL     pool;               /* Event pool */
    EV_HW_OPS   *hw_ops;           /* Hardware operations */
    ULONG       initialized;        /* Initialization flag */
    ULONG       total_events_sent;  /* Global event counter */
    ULONG       total_events_received; /* Global receive counter */
} EV_STATE;

/* Internal Function Prototypes */

/* Pool Management */
ULONG ev_pool_init(void);
EV_ECB *ev_pool_get_task_ecb(ULONG task_id);
ULONG ev_pool_validate_task_id(ULONG task_id);

/* Event Processing */
ULONG ev_check_conditions(EV_ECB *ecb);
ULONG ev_update_pending(EV_ECB *ecb, ULONG new_events);
ULONG ev_clear_received(EV_ECB *ecb, ULONG events_to_clear);
ULONG ev_timeout_expired(EV_ECB *ecb);

/* Wait Management */
ULONG ev_start_wait(EV_ECB *ecb, ULONG events, ULONG flags, ULONG timeout);
ULONG ev_complete_wait(EV_ECB *ecb, ULONG *events_received);
ULONG ev_cancel_wait(EV_ECB *ecb);

/* Hardware Abstraction */
ULONG ev_hw_init(void);
extern EV_HW_OPS ev_hw_generic_ops;
extern EV_HW_OPS ev_hw_stm32f4_ops;

/* Utility Functions */
ULONG ev_validate_ecb(EV_ECB *ecb);
ULONG ev_validate_events(ULONG events);
ULONG ev_validate_flags(ULONG flags);
ULONG ev_get_current_task_id(void);

/* Statistics and Debugging */
ULONG ev_get_statistics(ULONG task_id, ULONG *sent, ULONG *received, 
                       ULONG *waits, ULONG *timeouts);
void ev_update_statistics(EV_ECB *ecb, ULONG operation);

/* Global State (defined in event.c) */
extern EV_STATE ev_global_state;

/* Macros for common operations */
#define EV_GET_STATE()          (&ev_global_state)
#define EV_GET_POOL()           (&ev_global_state.pool)
#define EV_IS_VALID_ECB(ecb)    ((ecb) && (ecb)->magic == EV_POOL_MAGIC)
#define EV_GET_TASK_ECB(tid)    (&ev_global_state.pool.event_blocks[tid])

/* Event condition checking */
#define EV_CHECK_ANY_CONDITION(pending, waiting)   ((pending) & (waiting))
#define EV_CHECK_ALL_CONDITION(pending, waiting)   (((pending) & (waiting)) == (waiting))

/* Critical section macros (would integrate with task system) */
#define EV_ENTER_CRITICAL()     /* Would call task interrupt disable */
#define EV_EXIT_CRITICAL()      /* Would call task interrupt enable */

/* Debug/Statistics Macros */
#ifdef EV_DEBUG
#define EV_DEBUG_PRINT(fmt, ...) printf("[EVENT] " fmt "\n", ##__VA_ARGS__)
#else
#define EV_DEBUG_PRINT(fmt, ...)
#endif

/* Statistics operation types */
#define EV_STAT_SEND            1
#define EV_STAT_RECEIVE         2
#define EV_STAT_WAIT_START      3
#define EV_STAT_WAIT_COMPLETE   4
#define EV_STAT_TIMEOUT         5

/* Event validation macros */
#define EV_IS_VALID_EVENT_MASK(mask)    ((mask) != 0 && (mask) <= EV_ALL_EVENTS)
#define EV_IS_VALID_TASK_ID(tid)        ((tid) < MAX_TASK)
#define EV_IS_VALID_FLAGS(flags)        (((flags) & ~(EV_ANY | EV_NOWAIT)) == 0)

/* Timeout conversion macros */
#define EV_TICKS_TO_MS(ticks)           ((ticks) * 10)  /* Assuming 100Hz tick rate */
#define EV_MS_TO_TICKS(ms)              ((ms) / 10)
#define EV_INFINITE_TIMEOUT             0xFFFFFFFFUL

/* Event mask manipulation */
#define EV_SET_EVENTS(mask, events)     ((mask) |= (events))
#define EV_CLEAR_EVENTS(mask, events)   ((mask) &= ~(events))
#define EV_HAS_EVENTS(mask, events)     (((mask) & (events)) != 0)
#define EV_HAS_ALL_EVENTS(mask, events) (((mask) & (events)) == (events))

#endif /* _EVENT_H_ */