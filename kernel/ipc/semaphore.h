/************************************BEGIN*****************************************
*                       COPYRIGHT %G% %U% BY GHWORKS, LLC 
*                             ALL RIGHTS RESERVED
*                         SPDX-License-Identifier: MIT
* ********************************************************************************
* Name:        semaphore.h
* Type:        C Header (Private)
* Description: Internal Semaphore Services Interface
*
* Private structures, constants, and function prototypes for semaphore subsystem.
* This header is NOT part of the public API.
*
**************************************END***************************************/

#ifndef _SEMAPHORE_H_
#define _SEMAPHORE_H_

#include "../../gxkernel.h"

/* Semaphore Configuration Constants */
#define SM_MAX_SEMAPHORES   64      /* Maximum number of semaphores */
#define SM_INVALID_ID       0       /* Invalid semaphore ID */
#define SM_POOL_MAGIC       0x534D  /* "SM" magic for validation */
#define SM_NAME_SIZE        4       /* Semaphore name size */
#define SM_MAX_COUNT        0x7FFFFFFF /* Maximum semaphore count */

/* Semaphore States */
#define SM_STATE_FREE       0       /* Semaphore entry available */
#define SM_STATE_ACTIVE     1       /* Semaphore is active */
#define SM_STATE_DELETED    2       /* Semaphore marked for deletion */

/* Semaphore Wait Modes (from gxkernel.h but defined here for clarity) */
#ifndef SM_FIFO
#define SM_FIFO             0x00    /* FIFO waiting order */
#endif
#ifndef SM_PRIOR
#define SM_PRIOR            0x01    /* Priority-based waiting order */
#endif
#ifndef SM_NOWAIT
#define SM_NOWAIT           0x02    /* Non-blocking semaphore operation */
#endif

/* Hardware abstraction function pointers */
typedef struct sm_hw_ops {
    ULONG (*init)(void);                    /* Initialize hardware */
    ULONG (*create_semaphore)(void *scb, ULONG initial_count, ULONG max_count);
    ULONG (*delete_semaphore)(void *scb);   /* Delete semaphore object */
    ULONG (*wait_semaphore)(void *scb, ULONG timeout); /* Wait (P operation) */
    ULONG (*signal_semaphore)(void *scb);   /* Signal (V operation) */
    ULONG (*get_count)(void *scb);          /* Get current count */
} SM_HW_OPS;

/* Task Wait Node for semaphore waiting queue */
typedef struct sm_wait_node {
    ULONG               task_id;        /* Waiting task ID */
    ULONG               priority;       /* Task priority */
    ULONG               wait_start;     /* Time wait started */
    struct sm_wait_node *next;          /* Next in wait queue */
    struct sm_wait_node *prev;          /* Previous in wait queue */
} SM_WAIT_NODE;

/* Semaphore Control Block */
typedef struct sm_scb {
    ULONG       magic;              /* Validation magic number */
    ULONG       semaphore_id;       /* Unique semaphore identifier */
    char        name[SM_NAME_SIZE]; /* 4-character semaphore name */
    ULONG       state;              /* Current semaphore state */
    ULONG       flags;              /* Semaphore creation flags */
    
    /* Semaphore Count */
    volatile LONG current_count;    /* Current semaphore count */
    ULONG       initial_count;      /* Initial count value */
    ULONG       maximum_count;      /* Maximum count value */
    
    /* Wait Queue Management */
    SM_WAIT_NODE *wait_queue_head;  /* Head of waiting task queue */
    SM_WAIT_NODE *wait_queue_tail;  /* Tail of waiting task queue */
    ULONG       wait_count;         /* Number of waiting tasks */
    ULONG       wait_mode;          /* FIFO or priority-based waiting */
    
    /* Statistics */
    ULONG       total_waits;        /* Total P operations */
    ULONG       total_signals;      /* Total V operations */
    ULONG       total_timeouts;     /* Total timeout occurrences */
    ULONG       max_wait_time;      /* Maximum wait time observed */
    
    /* List Management */
    struct sm_scb *next;            /* Next semaphore in list */
    struct sm_scb *prev;            /* Previous semaphore in list */
    
    /* Hardware-Specific Context */
    void        *hw_context;        /* Hardware-specific context data */
    ULONG       context_size;       /* Size of hardware context */
} SM_SCB;

/* Semaphore Pool Management */
typedef struct sm_pool {
    ULONG       magic;              /* Pool validation magic */
    ULONG       max_semaphores;     /* Maximum number of semaphores */
    ULONG       active_count;       /* Number of active semaphores */
    ULONG       next_id;            /* Next semaphore ID to assign */
    SM_SCB      *free_list;         /* Free semaphore block list */
    SM_SCB      *active_list;       /* Active semaphore list */
    SM_SCB      semaphores[SM_MAX_SEMAPHORES]; /* Semaphore control blocks */
} SM_POOL;

/* Global Semaphore State */
typedef struct sm_state {
    SM_POOL     pool;               /* Semaphore pool */
    SM_HW_OPS   *hw_ops;           /* Hardware operations */
    ULONG       initialized;        /* Initialization flag */
    ULONG       total_created;      /* Total semaphores created */
    ULONG       total_deleted;      /* Total semaphores deleted */
} SM_STATE;

/* Internal Function Prototypes */

/* Pool Management */
ULONG sm_pool_init(void);
SM_SCB *sm_pool_alloc(void);
ULONG sm_pool_free(SM_SCB *scb);
SM_SCB *sm_pool_find(ULONG semaphore_id);
SM_SCB *sm_pool_find_by_name(char name[4]);

/* Wait Queue Management */
ULONG sm_wait_queue_add(SM_SCB *scb, ULONG task_id, ULONG priority);
ULONG sm_wait_queue_remove(SM_SCB *scb, ULONG task_id);
ULONG sm_wait_queue_get_next(SM_SCB *scb);
ULONG sm_wait_queue_clear(SM_SCB *scb);

/* Semaphore Operations */
ULONG sm_increment_count(SM_SCB *scb);
ULONG sm_decrement_count(SM_SCB *scb);
ULONG sm_check_available(SM_SCB *scb);

/* Hardware Abstraction */
ULONG sm_hw_init(void);
extern SM_HW_OPS sm_hw_generic_ops;
extern SM_HW_OPS sm_hw_stm32f4_ops;

/* Utility Functions */
ULONG sm_validate_scb(SM_SCB *scb);
ULONG sm_validate_count(LONG count);
ULONG sm_validate_flags(ULONG flags);
ULONG sm_generate_id(void);
void sm_name_copy(char dest[4], char src[4]);
ULONG sm_name_compare(char name1[4], char name2[4]);
ULONG sm_get_current_task_id(void);
ULONG sm_get_current_priority(void);

/* Statistics and Debugging */
ULONG sm_get_statistics(ULONG semaphore_id, ULONG *waits, ULONG *signals, 
                       ULONG *timeouts, ULONG *max_wait);
void sm_update_statistics(SM_SCB *scb, ULONG operation, ULONG wait_time);

/* Global State (defined in semaphore.c) */
extern SM_STATE sm_global_state;

/* Macros for common operations */
#define SM_GET_STATE()          (&sm_global_state)
#define SM_GET_POOL()           (&sm_global_state.pool)
#define SM_IS_VALID_SCB(scb)    ((scb) && (scb)->magic == SM_POOL_MAGIC)

/* Count manipulation macros */
#define SM_INCREMENT_COUNT(scb) ((scb)->current_count++)
#define SM_DECREMENT_COUNT(scb) ((scb)->current_count--)
#define SM_GET_COUNT(scb)       ((scb)->current_count)
#define SM_IS_AVAILABLE(scb)    ((scb)->current_count > 0)

/* Critical section macros (would integrate with task system) */
#define SM_ENTER_CRITICAL()     /* Would call task interrupt disable */
#define SM_EXIT_CRITICAL()      /* Would call task interrupt enable */

/* Debug/Statistics Macros */
#ifdef SM_DEBUG
#define SM_DEBUG_PRINT(fmt, ...) printf("[SEM] " fmt "\n", ##__VA_ARGS__)
#else
#define SM_DEBUG_PRINT(fmt, ...)
#endif

/* Statistics operation types */
#define SM_STAT_WAIT            1
#define SM_STAT_SIGNAL          2
#define SM_STAT_TIMEOUT         3

/* Validation macros */
#define SM_IS_VALID_ID(id)      ((id) != SM_INVALID_ID)
#define SM_IS_VALID_COUNT(cnt)  ((cnt) >= 0 && (cnt) <= SM_MAX_COUNT)
#define SM_IS_VALID_FLAGS(flags) (((flags) & ~(SM_FIFO | SM_PRIOR | SM_NOWAIT)) == 0)

/* Wait queue insertion modes */
#define SM_INSERT_FIFO          0   /* Insert at tail (FIFO) */
#define SM_INSERT_PRIORITY      1   /* Insert by priority */

/* Timeout handling */
#define SM_INFINITE_TIMEOUT     0xFFFFFFFFUL
#define SM_NO_TIMEOUT           0

#endif /* _SEMAPHORE_H_ */