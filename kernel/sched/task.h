/************************************BEGIN*****************************************
*                       COPYRIGHT %G% %U% BY GHWORKS, LLC 
*                             ALL RIGHTS RESERVED
*                         SPDX-License-Identifier: MIT
* ********************************************************************************
* Name:        task.h
* Type:        C Header (Private)
* Description: Internal Task Management Interface
*
* Private structures, constants, and function prototypes for task subsystem.
* This header is NOT part of the public API.
*
**************************************END***************************************/

#ifndef _TASK_H_
#define _TASK_H_

#include "../../gxkernel.h"

/* Task Configuration Constants */
#define T_MAX_TASKS         64      /* Maximum concurrent tasks */
#define T_INVALID_ID        0       /* Invalid task ID */
#define T_POOL_MAGIC        0x5443  /* "TC" magic for validation */
#define T_REG_COUNT         7       /* Number of task registers */
#define T_NAME_SIZE         4       /* Task name size */

/* Task States */
#define T_STATE_FREE        0       /* Task entry available */
#define T_STATE_CREATED     1       /* Task created but not started */
#define T_STATE_READY       2       /* Ready to run */
#define T_STATE_RUNNING     3       /* Currently executing */
#define T_STATE_SUSPENDED   4       /* Explicitly suspended */
#define T_STATE_BLOCKED     5       /* Waiting on IPC or timer */
#define T_STATE_DELETED     6       /* Marked for deletion */

/* Task Priority Constants */
#define T_MIN_PRIORITY      1       /* Highest priority (lowest number) */
#define T_MAX_PRIORITY      255     /* Lowest priority (highest number) */
#define T_IDLE_PRIORITY     255     /* Idle task priority */
#define T_DEFAULT_PRIORITY  128     /* Default task priority */

/* Stack Size Constants */
#define T_MIN_STACK_SIZE    512     /* Minimum stack size (bytes) */
#define T_DEFAULT_STACK_SIZE 2048   /* Default stack size (bytes) */
#define T_MAX_STACK_SIZE    65536   /* Maximum stack size (bytes) */

/* Context Switch Reasons */
#define T_SWITCH_VOLUNTARY  1       /* Task voluntarily yielded */
#define T_SWITCH_PREEMPTED  2       /* Task was preempted */
#define T_SWITCH_BLOCKED    3       /* Task blocked on IPC/timer */
#define T_SWITCH_SUSPENDED  4       /* Task was suspended */
#define T_SWITCH_DELETED    5       /* Task was deleted */

/* Hardware abstraction function pointers */
typedef struct t_hw_ops {
    ULONG (*init)(void);                        /* Initialize hardware */
    ULONG (*create_context)(void *tcb, void *entry, void *stack, ULONG size);
    void  (*switch_context)(void *old_tcb, void *new_tcb);
    void  (*delete_context)(void *tcb);
    void  (*enable_interrupts)(void);
    void  (*disable_interrupts)(void);
    ULONG (*get_current_sp)(void);              /* Get current stack pointer */
} T_HW_OPS;

/* Task Control Block */
typedef struct t_tcb {
    ULONG       magic;              /* Validation magic number */
    ULONG       task_id;            /* Unique task identifier */
    char        name[T_NAME_SIZE];  /* 4-character task name */
    ULONG       state;              /* Current task state */
    ULONG       priority;           /* Task priority (1-255) */
    
    /* Stack Information */
    void        *stack_base;        /* Base of allocated stack */
    ULONG       stack_size;         /* Total stack size */
    void        *stack_pointer;     /* Current stack pointer */
    ULONG       stack_used;         /* Maximum stack usage */
    
    /* Task Attributes */
    ULONG       flags;              /* Task creation flags */
    ULONG       mode;               /* Current task mode */
    ULONG       reg[T_REG_COUNT];   /* Task registers */
    
    /* Entry Point and Arguments */
    void        (*entry_point)(ULONG args[]);  /* Task entry function */
    ULONG       args[4];            /* Task arguments */
    
    /* Timing Information */
    ULONG       create_time;        /* Tick count when created */
    ULONG       start_time;         /* Tick count when started */
    ULONG       run_time;           /* Total run time in ticks */
    ULONG       last_run;           /* Last time scheduled */
    
    /* Scheduling Information */
    ULONG       time_slice;         /* Time slice for round-robin */
    ULONG       slice_remaining;    /* Remaining time slice */
    ULONG       cpu_usage;          /* CPU usage percentage */
    
    /* IPC and Synchronization */
    ULONG       event_mask;         /* Events this task is waiting for */
    ULONG       pending_events;     /* Events received but not processed */
    void        *wait_object;       /* Object task is waiting on */
    ULONG       wait_timeout;       /* Timeout for wait operation */
    
    /* List Management */
    struct t_tcb *next;             /* Next task in list */
    struct t_tcb *prev;             /* Previous task in list */
    
    /* Hardware-Specific Context */
    void        *hw_context;        /* Hardware-specific context data */
    ULONG       context_size;       /* Size of hardware context */
} T_TCB;

/* Task Pool Management */
typedef struct t_pool {
    ULONG       magic;              /* Pool validation magic */
    ULONG       max_tasks;          /* Maximum number of tasks */
    ULONG       active_count;       /* Number of active tasks */
    ULONG       next_id;            /* Next task ID to assign */
    T_TCB       *free_list;         /* Free task block list */
    T_TCB       tasks[T_MAX_TASKS]; /* Task control blocks */
} T_POOL;

/* Scheduler State */
typedef struct t_scheduler {
    ULONG       magic;              /* Scheduler validation magic */
    T_TCB       *current_task;      /* Currently running task */
    T_TCB       *ready_lists[T_MAX_PRIORITY + 1];  /* Priority-based ready lists */
    ULONG       ready_mask;         /* Bitmap of non-empty ready lists */
    ULONG       preemption_enabled; /* Preemption enable flag */
    ULONG       context_switches;   /* Total context switches */
    ULONG       last_switch_time;   /* Time of last context switch */
    ULONG       idle_time;          /* Time spent in idle task */
} T_SCHEDULER;

/* Global Task State */
typedef struct t_state {
    T_POOL      pool;               /* Task pool */
    T_SCHEDULER scheduler;          /* Scheduler state */
    T_HW_OPS    *hw_ops;           /* Hardware operations */
    ULONG       initialized;        /* Initialization flag */
    ULONG       interrupts_disabled; /* Interrupt disable count */
    ULONG       total_stack_used;   /* Total stack memory allocated */
} T_STATE;

/* Internal Function Prototypes */

/* Pool Management */
ULONG t_pool_init(void);
T_TCB *t_pool_alloc(void);
ULONG t_pool_free(T_TCB *tcb);
T_TCB *t_pool_find(ULONG task_id);
T_TCB *t_pool_find_by_name(char name[4]);

/* Scheduler Functions */
ULONG t_scheduler_init(void);
void t_scheduler_add_ready(T_TCB *tcb);
void t_scheduler_remove_ready(T_TCB *tcb);
T_TCB *t_scheduler_get_highest_ready(void);
void t_scheduler_reschedule(void);
void t_scheduler_preempt(void);
void t_scheduler_yield(void);

/* Context Management */
ULONG t_context_create(T_TCB *tcb, void (*entry)(ULONG args[]), ULONG args[]);
void t_context_switch(T_TCB *old_task, T_TCB *new_task);
void t_context_delete(T_TCB *tcb);

/* Stack Management */
ULONG t_stack_alloc(T_TCB *tcb, ULONG size);
void t_stack_free(T_TCB *tcb);
ULONG t_stack_check(T_TCB *tcb);
ULONG t_stack_usage(T_TCB *tcb);

/* Task State Management */
void t_set_state(T_TCB *tcb, ULONG new_state);
ULONG t_validate_priority(ULONG priority);
ULONG t_validate_flags(ULONG flags);

/* Interrupt Management */
ULONG t_disable_interrupts(void);
void t_enable_interrupts(ULONG level);

/* Hardware Abstraction */
ULONG t_hw_init(void);
extern T_HW_OPS t_hw_generic_ops;
extern T_HW_OPS t_hw_stm32f4_ops;

/* Utility Functions */
ULONG t_validate_tcb(T_TCB *tcb);
ULONG t_generate_id(void);
void t_name_copy(char dest[4], char src[4]);
ULONG t_name_compare(char name1[4], char name2[4]);

/* Statistics and Debugging */
ULONG t_get_cpu_usage(ULONG task_id);
ULONG t_get_stack_usage(ULONG task_id);
void t_update_statistics(void);

/* Global State (defined in task.c) */
extern T_STATE t_global_state;

/* Macros for common operations */
#define T_GET_STATE()           (&t_global_state)
#define T_GET_POOL()            (&t_global_state.pool)
#define T_GET_SCHEDULER()       (&t_global_state.scheduler)
#define T_IS_VALID_TCB(tcb)     ((tcb) && (tcb)->magic == T_POOL_MAGIC)
#define T_GET_CURRENT_TASK()    (t_global_state.scheduler.current_task)

/* Priority operations */
#define T_PRIORITY_TO_INDEX(pri)    ((pri) <= T_MAX_PRIORITY ? (pri) : T_MAX_PRIORITY)
#define T_SET_READY_BIT(pri)        (t_global_state.scheduler.ready_mask |= (1UL << T_PRIORITY_TO_INDEX(pri)))
#define T_CLEAR_READY_BIT(pri)      (t_global_state.scheduler.ready_mask &= ~(1UL << T_PRIORITY_TO_INDEX(pri)))
#define T_IS_READY_BIT_SET(pri)     (t_global_state.scheduler.ready_mask & (1UL << T_PRIORITY_TO_INDEX(pri)))

/* Find highest priority ready task using bit scan */
#define T_FIND_HIGHEST_PRIORITY() __builtin_ctz(t_global_state.scheduler.ready_mask)

/* Critical section macros */
#define T_ENTER_CRITICAL()      t_disable_interrupts()
#define T_EXIT_CRITICAL(level)  t_enable_interrupts(level)

/* Debug/Statistics Macros */
#ifdef T_DEBUG
#define T_DEBUG_PRINT(fmt, ...) printf("[TASK] " fmt "\n", ##__VA_ARGS__)
#else
#define T_DEBUG_PRINT(fmt, ...)
#endif

/* Task mode bit manipulation */
#define T_MODE_SET(tcb, mask)       ((tcb)->mode |= (mask))
#define T_MODE_CLEAR(tcb, mask)     ((tcb)->mode &= ~(mask))
#define T_MODE_TEST(tcb, mask)      ((tcb)->mode & (mask))

/* Stack checking macros */
#define T_STACK_OVERFLOW_PATTERN    0xDEADBEEF
#define T_STACK_UNDERFLOW_PATTERN   0xFEEDFACE

/* Performance monitoring */
#define T_PERF_START()              /* Platform-specific performance counter start */
#define T_PERF_STOP()               /* Platform-specific performance counter stop */

#endif /* _TASK_H_ */