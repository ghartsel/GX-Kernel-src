/************************************BEGIN*****************************************
*                       COPYRIGHT %G% %U% BY GHWORKS, LLC 
*                             ALL RIGHTS RESERVED
*                         SPDX-License-Identifier: MIT
* ********************************************************************************
* Name:        queue.h
* Type:        C Header
* Description: Private header for message queue services
*
* This file contains internal structures, constants, and function prototypes
* for the queue implementation. It should not be included by user code.
*
**************************************END***************************************/

#ifndef QUEUE_H
#define QUEUE_H

#include "../../gxkernel.h"
#include "../../gxkCfg.h"

/******************************************************************************
* Constants and Configuration
******************************************************************************/

#define Q_POOL_MAGIC           0x51455545UL  /* 'QUEUE' */
#define Q_INVALID_ID           0
#define Q_DEFAULT_COUNT        16
#define Q_MIN_COUNT            4
#define Q_MAX_COUNT            1024
#define Q_MSG_SIZE             4             /* 4 ULONG words per message */

/* Queue buffer management */
#define Q_MAX_BUFFERS          MAX_BUF
#define Q_BUFFER_WATERMARK     (Q_MAX_BUFFERS * 3 / 4)

/* Statistics operations */
#define Q_STAT_SEND            1
#define Q_STAT_RECEIVE         2
#define Q_STAT_BROADCAST       3
#define Q_STAT_URGENT          4
#define Q_STAT_OVERFLOW        5

/******************************************************************************
* Type Definitions
******************************************************************************/

/* Message buffer structure - matches original MSGBUF */
typedef struct {
    ULONG msg[Q_MSG_SIZE];
} Q_MSGBUF;

/* Queue buffer descriptor - matches original QBUFDESC */
typedef struct {
    ULONG start;        /* Start index in global buffer pool */
    ULONG end;          /* End index in global buffer pool */
    ULONG nextin;       /* Next input position */  
    ULONG nextout;      /* Next output position */
} Q_BUFDESC;

/* Queue states */
typedef enum {
    Q_STATE_FREE = 0,
    Q_STATE_ACTIVE,
    Q_STATE_DELETING
} Q_STATE_TYPE;

/* Queue Control Block - matches original QDESC */
typedef struct q_qcb {
    /* Standard object fields */
    char name[4];                   /* Queue name */
    ULONG queue_id;                 /* Unique queue ID */
    ULONG magic;                    /* Magic number for validation */
    Q_STATE_TYPE state;             /* Queue state */
    
    /* Queue configuration */
    ULONG count;                    /* Maximum number of messages */
    ULONG flags;                    /* Queue flags (Q_FIFO, Q_PRIOR, etc.) */
    ULONG mode;                     /* Queue mode */
    
    /* Buffer management */
    Q_BUFDESC buf;                  /* Buffer descriptor */
    ULONG current_messages;         /* Current message count */
    ULONG high_water_mark;          /* Peak message count */
    
    /* Synchronization */
    char semname[4];                /* Associated semaphore name */
    ULONG semid;                    /* Semaphore ID for blocking */
    
    /* Statistics */
    ULONG total_sent;               /* Total messages sent */
    ULONG total_received;           /* Total messages received */
    ULONG total_broadcasts;         /* Total broadcast operations */
    ULONG total_overflows;          /* Total overflow conditions */
    ULONG max_wait_time;            /* Maximum wait time observed */
    
    /* Pool management */
    struct q_qcb *next;             /* Next in list */
    struct q_qcb *prev;             /* Previous in list */
} Q_QCB;

/* Queue Pool Structure */
typedef struct {
    ULONG magic;                    /* Pool validation magic */
    ULONG max_queues;              /* Maximum number of queues */
    ULONG active_count;            /* Number of active queues */
    ULONG next_id;                 /* Next queue ID to assign */
    Q_QCB *free_list;              /* Free queue list */
    Q_QCB *active_list;            /* Active queue list */
    Q_QCB queues[MAX_Q];           /* Queue storage */
} Q_POOL;

/* Global Buffer Pool Structure */
typedef struct {
    ULONG magic;                   /* Buffer pool validation */
    ULONG total_buffers;           /* Total number of buffers */
    ULONG next_available;          /* Next available buffer index */
    ULONG buffers_allocated;       /* Number of allocated buffers */
    ULONG allocation_failures;     /* Buffer allocation failures */
    Q_MSGBUF buffers[Q_MAX_BUFFERS]; /* Message buffer storage */
} Q_BUFFER_POOL;

/* Hardware abstraction operations */
typedef struct {
    ULONG (*init)(void);
    ULONG (*create_queue)(Q_QCB *qcb);
    ULONG (*delete_queue)(Q_QCB *qcb);
    ULONG (*send_message)(Q_QCB *qcb, ULONG msg[Q_MSG_SIZE]);
    ULONG (*receive_message)(Q_QCB *qcb, ULONG msg[Q_MSG_SIZE], ULONG timeout);
    ULONG (*broadcast_message)(Q_QCB *qcb, ULONG msg[Q_MSG_SIZE], ULONG *count);
    void (*cleanup)(void);
} Q_HW_OPS;

/* Global Queue System State */
typedef struct {
    ULONG magic;                   /* State validation magic */
    ULONG initialized;             /* Initialization flag */
    ULONG interrupts_disabled;     /* Critical section nesting */
    
    /* Hardware abstraction */
    Q_HW_OPS *hw_ops;              /* Hardware operations */
    
    /* Pool management */
    Q_POOL queue_pool;             /* Queue control block pool */
    Q_BUFFER_POOL buffer_pool;     /* Message buffer pool */
    
    /* Global statistics */
    ULONG total_queues_created;    /* Total queues created */
    ULONG total_queues_deleted;    /* Total queues deleted */
    ULONG total_messages_sent;     /* Total messages sent */
    ULONG total_messages_received; /* Total messages received */
    ULONG total_buffer_overflows;  /* Total buffer allocation failures */
} Q_STATE;

/******************************************************************************
* Macros for accessing global state
******************************************************************************/

extern Q_STATE q_global_state;

#define Q_GET_STATE()           (&q_global_state)
#define Q_GET_POOL()            (&q_global_state.queue_pool)
#define Q_GET_BUFFER_POOL()     (&q_global_state.buffer_pool)

/******************************************************************************
* Validation macros
******************************************************************************/

#define Q_IS_VALID_QCB(qcb)     ((qcb) != NULL && (qcb)->magic == Q_POOL_MAGIC)
#define Q_IS_VALID_ID(id)       ((id) != Q_INVALID_ID && (id) != 0)
#define Q_IS_VALID_COUNT(count) ((count) >= Q_MIN_COUNT && (count) <= Q_MAX_COUNT)
#define Q_IS_VALID_FLAGS(flags) (((flags) & ~(Q_FIFO | Q_PRIOR | Q_GLOBAL | Q_LOCAL | Q_PRIBUF)) == 0)
#define Q_IS_VALID_MSG(msg)     ((msg) != NULL)

/******************************************************************************
* Critical section macros (hardware abstracted)
******************************************************************************/

#define Q_ENTER_CRITICAL()      q_disable_interrupts()
#define Q_EXIT_CRITICAL()       q_enable_interrupts()

/******************************************************************************
* Buffer management macros
******************************************************************************/

#define Q_BUFFER_FULL(qcb)      (((qcb)->buf.nextin + 1 == (qcb)->buf.nextout) || \
                                ((qcb)->buf.nextin == (qcb)->buf.end && (qcb)->buf.nextout == (qcb)->buf.start))

#define Q_BUFFER_EMPTY(qcb)     ((qcb)->buf.nextin == (qcb)->buf.nextout)

#define Q_ADVANCE_INDEX(qcb, idx) \
    do { \
        (idx) = ((idx) == (qcb)->buf.end) ? (qcb)->buf.start : ((idx) + 1); \
    } while(0)

/******************************************************************************
* Function Prototypes - Pool Management
******************************************************************************/

ULONG q_pool_init(void);
Q_QCB *q_pool_alloc(void);
ULONG q_pool_free(Q_QCB *qcb);
Q_QCB *q_pool_find(ULONG queue_id);
Q_QCB *q_pool_find_by_name(char name[4]);

/******************************************************************************
* Function Prototypes - Buffer Management
******************************************************************************/

ULONG q_buffer_pool_init(void);
ULONG q_buffer_alloc(ULONG count);
ULONG q_buffer_free(ULONG start_index, ULONG count);
ULONG q_buffer_available(void);
Q_MSGBUF *q_buffer_get(ULONG index);

/******************************************************************************
* Function Prototypes - Queue Operations
******************************************************************************/

ULONG q_message_enqueue(Q_QCB *qcb, ULONG msg[Q_MSG_SIZE], ULONG urgent);
ULONG q_message_dequeue(Q_QCB *qcb, ULONG msg[Q_MSG_SIZE]);
ULONG q_message_peek(Q_QCB *qcb, ULONG msg[Q_MSG_SIZE]);
ULONG q_message_count(Q_QCB *qcb);
ULONG q_message_space(Q_QCB *qcb);

/******************************************************************************
* Function Prototypes - Synchronization
******************************************************************************/

ULONG q_sync_create(Q_QCB *qcb);
ULONG q_sync_delete(Q_QCB *qcb);
ULONG q_sync_wait_message(Q_QCB *qcb, ULONG flags, ULONG timeout);
ULONG q_sync_signal_message(Q_QCB *qcb);

/******************************************************************************
* Function Prototypes - Utility Functions
******************************************************************************/

ULONG q_validate_qcb(Q_QCB *qcb);
ULONG q_validate_count(ULONG count);
ULONG q_validate_flags(ULONG flags);
ULONG q_generate_id(void);
void q_name_copy(char dest[4], char src[4]);
ULONG q_name_compare(char name1[4], char name2[4]);
ULONG q_disable_interrupts(void);
void q_enable_interrupts(void);

/******************************************************************************
* Function Prototypes - Statistics and Diagnostics
******************************************************************************/

void q_update_statistics(Q_QCB *qcb, ULONG operation, ULONG value);
ULONG q_get_statistics(ULONG queue_id, ULONG *sent, ULONG *received, 
                      ULONG *broadcasts, ULONG *overflows);
ULONG q_get_buffer_statistics(ULONG *total, ULONG *allocated, ULONG *available);

/******************************************************************************
* Function Prototypes - Hardware Abstraction
******************************************************************************/

ULONG q_hw_init(void);
void q_hw_cleanup(void);

/* External hardware operation structures */
extern Q_HW_OPS q_hw_generic_ops;
extern Q_HW_OPS q_hw_stm32f4_ops;

/******************************************************************************
* Function Prototypes - Legacy Integration
******************************************************************************/

ULONG gxk_q_init(void);

#endif /* QUEUE_H */