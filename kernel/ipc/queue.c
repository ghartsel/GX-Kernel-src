/************************************BEGIN*****************************************
*                       COPYRIGHT %G% %U% BY GHWORKS, LLC 
*                             ALL RIGHTS RESERVED
*                         SPDX-License-Identifier: MIT
* ********************************************************************************
* Name:        queue.c
* Type:        C Source
* Description: Message Queue Services Implementation
*
* Interface (public) Routines:
*   q_broadcast  - Broadcast message to all waiting tasks
*   q_create     - Create a new message queue
*   q_delete     - Delete a message queue
*   q_ident      - Identify queue by name
*   q_receive    - Receive message from queue
*   q_send       - Send message to queue
*   q_urgent     - Send urgent message to front of queue
*   q_vcreate    - Create variable-size message queue
*   q_vdelete    - Delete variable-size message queue
*   q_vident     - Identify variable-size queue by name
*   q_vreceive   - Receive variable-size message
*   q_vsend      - Send variable-size message
*
* Private Functions:
*   Queue pool management
*   Buffer allocation/deallocation
*   Message operations
*   Synchronization with semaphores
*   Hardware abstraction
*   Statistics tracking
*
**************************************END***************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../gxkernel.h"
#include "../../gxkCfg.h"
#include "queue.h"
#include "semaphore.h"

/* Global queue state */
Q_STATE q_global_state = {0};

/* Forward declarations for internal functions */
static ULONG q_init_once(void);

/******************************************************************************
* Queue Pool Management Functions
******************************************************************************/

ULONG q_pool_init(void)
{
    Q_POOL *pool = Q_GET_POOL();
    int i;
    
    memset(pool, 0, sizeof(Q_POOL));
    
    pool->magic = Q_POOL_MAGIC;
    pool->max_queues = MAX_Q;
    pool->active_count = 0;
    pool->next_id = 1;
    
    /* Initialize free list */
    for (i = 0; i < MAX_Q - 1; i++) {
        pool->queues[i].next = &pool->queues[i + 1];
        pool->queues[i].queue_id = 0;
        pool->queues[i].state = Q_STATE_FREE;
        pool->queues[i].magic = 0;
    }
    pool->queues[MAX_Q - 1].next = NULL;
    pool->free_list = &pool->queues[0];
    pool->active_list = NULL;
    
    return 0;
}

Q_QCB *q_pool_alloc(void)
{
    Q_POOL *pool = Q_GET_POOL();
    Q_QCB *qcb;
    
    if (pool->free_list == NULL) {
        return NULL;
    }
    
    qcb = pool->free_list;
    pool->free_list = qcb->next;
    pool->active_count++;
    
    /* Initialize QCB */
    memset(qcb, 0, sizeof(Q_QCB));
    qcb->magic = Q_POOL_MAGIC;
    qcb->queue_id = q_generate_id();
    qcb->state = Q_STATE_ACTIVE;
    
    /* Add to active list */
    qcb->next = pool->active_list;
    if (pool->active_list != NULL) {
        pool->active_list->prev = qcb;
    }
    pool->active_list = qcb;
    
    return qcb;
}

ULONG q_pool_free(Q_QCB *qcb)
{
    Q_POOL *pool = Q_GET_POOL();
    
    if (!Q_IS_VALID_QCB(qcb)) {
        return ERR_BADPARAM;
    }
    
    /* Remove from active list */
    if (qcb->prev != NULL) {
        qcb->prev->next = qcb->next;
    } else {
        pool->active_list = qcb->next;
    }
    
    if (qcb->next != NULL) {
        qcb->next->prev = qcb->prev;
    }
    
    /* Free buffer allocation */
    if (qcb->buf.start != qcb->buf.end) {
        q_buffer_free(qcb->buf.start, qcb->count);
    }
    
    /* Delete synchronization semaphore */
    q_sync_delete(qcb);
    
    /* Delete hardware context */
    Q_STATE *state = Q_GET_STATE();
    if (state->hw_ops && state->hw_ops->delete_queue) {
        state->hw_ops->delete_queue(qcb);
    }
    
    /* Clear the QCB */
    memset(qcb, 0, sizeof(Q_QCB));
    qcb->state = Q_STATE_FREE;
    
    /* Add to free list */
    qcb->next = pool->free_list;
    pool->free_list = qcb;
    pool->active_count--;
    
    return 0;
}

Q_QCB *q_pool_find(ULONG queue_id)
{
    Q_POOL *pool = Q_GET_POOL();
    Q_QCB *qcb;
    
    if (!Q_IS_VALID_ID(queue_id)) {
        return NULL;
    }
    
    /* Search active list */
    qcb = pool->active_list;
    while (qcb != NULL) {
        if (qcb->queue_id == queue_id && Q_IS_VALID_QCB(qcb)) {
            return qcb;
        }
        qcb = qcb->next;
    }
    
    return NULL;
}

Q_QCB *q_pool_find_by_name(char name[4])
{
    Q_POOL *pool = Q_GET_POOL();
    Q_QCB *qcb;
    
    /* Search active list */
    qcb = pool->active_list;
    while (qcb != NULL) {
        if (Q_IS_VALID_QCB(qcb) && q_name_compare(qcb->name, name)) {
            return qcb;
        }
        qcb = qcb->next;
    }
    
    return NULL;
}

/******************************************************************************
* Buffer Pool Management Functions
******************************************************************************/

ULONG q_buffer_pool_init(void)
{
    Q_BUFFER_POOL *pool = Q_GET_BUFFER_POOL();
    
    memset(pool, 0, sizeof(Q_BUFFER_POOL));
    
    pool->magic = Q_POOL_MAGIC;
    pool->total_buffers = Q_MAX_BUFFERS;
    pool->next_available = 0;
    pool->buffers_allocated = 0;
    pool->allocation_failures = 0;
    
    return 0;
}

ULONG q_buffer_alloc(ULONG count)
{
    Q_BUFFER_POOL *pool = Q_GET_BUFFER_POOL();
    ULONG start_index;
    
    if (count == 0 || pool->next_available + count > pool->total_buffers) {
        pool->allocation_failures++;
        return Q_MAX_BUFFERS; /* Invalid index indicates failure */
    }
    
    start_index = pool->next_available;
    pool->next_available += count;
    pool->buffers_allocated += count;
    
    return start_index;
}

ULONG q_buffer_free(ULONG start_index, ULONG count)
{
    Q_BUFFER_POOL *pool = Q_GET_BUFFER_POOL();
    
    if (start_index >= pool->total_buffers || count == 0) {
        return ERR_BADPARAM;
    }
    
    /* Note: Simple implementation doesn't actually reclaim memory */
    /* A more sophisticated implementation would maintain a free list */
    pool->buffers_allocated -= count;
    
    return 0;
}

ULONG q_buffer_available(void)
{
    Q_BUFFER_POOL *pool = Q_GET_BUFFER_POOL();
    return pool->total_buffers - pool->next_available;
}

Q_MSGBUF *q_buffer_get(ULONG index)
{
    Q_BUFFER_POOL *pool = Q_GET_BUFFER_POOL();
    
    if (index >= pool->total_buffers) {
        return NULL;
    }
    
    return &pool->buffers[index];
}

/******************************************************************************
* Message Operations Functions
******************************************************************************/

ULONG q_message_enqueue(Q_QCB *qcb, ULONG msg[Q_MSG_SIZE], ULONG urgent)
{
    Q_MSGBUF *buffer;
    ULONG target_index;
    int i;
    
    if (!Q_IS_VALID_QCB(qcb) || !Q_IS_VALID_MSG(msg)) {
        return ERR_BADPARAM;
    }
    
    if (Q_BUFFER_FULL(qcb)) {
        q_update_statistics(qcb, Q_STAT_OVERFLOW, 1);
        return ERR_QFULL;
    }
    
    if (urgent) {
        /* Urgent message goes to the front (output position - 1) */
        target_index = (qcb->buf.nextout == qcb->buf.start) ? 
                      qcb->buf.end : (qcb->buf.nextout - 1);
        qcb->buf.nextout = target_index;
    } else {
        /* Normal message goes to the back (input position) */
        target_index = qcb->buf.nextin;
        Q_ADVANCE_INDEX(qcb, qcb->buf.nextin);
    }
    
    /* Copy message to buffer */
    buffer = q_buffer_get(target_index);
    if (buffer == NULL) {
        return ERR_NOMGB;
    }
    
    for (i = 0; i < Q_MSG_SIZE; i++) {
        buffer->msg[i] = msg[i];
    }
    
    qcb->current_messages++;
    if (qcb->current_messages > qcb->high_water_mark) {
        qcb->high_water_mark = qcb->current_messages;
    }
    
    return 0;
}

ULONG q_message_dequeue(Q_QCB *qcb, ULONG msg[Q_MSG_SIZE])
{
    Q_MSGBUF *buffer;
    int i;
    
    if (!Q_IS_VALID_QCB(qcb) || !Q_IS_VALID_MSG(msg)) {
        return ERR_BADPARAM;
    }
    
    if (Q_BUFFER_EMPTY(qcb)) {
        return ERR_NOMSG;
    }
    
    /* Get message from output position */
    buffer = q_buffer_get(qcb->buf.nextout);
    if (buffer == NULL) {
        return ERR_NOMGB;
    }
    
    /* Copy message from buffer */
    for (i = 0; i < Q_MSG_SIZE; i++) {
        msg[i] = buffer->msg[i];
    }
    
    Q_ADVANCE_INDEX(qcb, qcb->buf.nextout);
    qcb->current_messages--;
    
    return 0;
}

ULONG q_message_peek(Q_QCB *qcb, ULONG msg[Q_MSG_SIZE])
{
    Q_MSGBUF *buffer;
    int i;
    
    if (!Q_IS_VALID_QCB(qcb) || !Q_IS_VALID_MSG(msg)) {
        return ERR_BADPARAM;
    }
    
    if (Q_BUFFER_EMPTY(qcb)) {
        return ERR_NOMSG;
    }
    
    /* Get message from output position without removing it */
    buffer = q_buffer_get(qcb->buf.nextout);
    if (buffer == NULL) {
        return ERR_NOMGB;
    }
    
    /* Copy message from buffer */
    for (i = 0; i < Q_MSG_SIZE; i++) {
        msg[i] = buffer->msg[i];
    }
    
    return 0;
}

ULONG q_message_count(Q_QCB *qcb)
{
    if (!Q_IS_VALID_QCB(qcb)) {
        return 0;
    }
    
    return qcb->current_messages;
}

ULONG q_message_space(Q_QCB *qcb)
{
    if (!Q_IS_VALID_QCB(qcb)) {
        return 0;
    }
    
    return qcb->count - qcb->current_messages;
}

/******************************************************************************
* Synchronization Functions
******************************************************************************/

ULONG q_sync_create(Q_QCB *qcb)
{
    ULONG error;
    
    if (!Q_IS_VALID_QCB(qcb)) {
        return ERR_BADPARAM;
    }
    
    /* Create semaphore name */
    sprintf(qcb->semname, "q%02x", (unsigned int)(qcb->queue_id & 0xFF));
    
    /* Create semaphore with initial count of 0 (no messages) */
    error = sm_create(qcb->semname, 0, (SM_LOCAL | SM_FIFO), &qcb->semid);
    
    return error;
}

ULONG q_sync_delete(Q_QCB *qcb)
{
    if (!Q_IS_VALID_QCB(qcb) || qcb->semid == 0) {
        return 0;
    }
    
    return sm_delete(qcb->semid);
}

ULONG q_sync_wait_message(Q_QCB *qcb, ULONG flags, ULONG timeout)
{
    ULONG sem_flags;
    
    if (!Q_IS_VALID_QCB(qcb)) {
        return ERR_BADPARAM;
    }
    
    /* Convert queue flags to semaphore flags */
    sem_flags = (flags & Q_NOWAIT) ? SM_NOWAIT : SM_WAIT;
    
    return sm_p(qcb->semid, sem_flags, timeout);
}

ULONG q_sync_signal_message(Q_QCB *qcb)
{
    if (!Q_IS_VALID_QCB(qcb)) {
        return ERR_BADPARAM;
    }
    
    return sm_v(qcb->semid);
}

/******************************************************************************
* Utility Functions
******************************************************************************/

ULONG q_validate_qcb(Q_QCB *qcb)
{
    return Q_IS_VALID_QCB(qcb) ? 0 : ERR_BADPARAM;
}

ULONG q_validate_count(ULONG count)
{
    return Q_IS_VALID_COUNT(count) ? 0 : ERR_BADPARAM;
}

ULONG q_validate_flags(ULONG flags)
{
    return Q_IS_VALID_FLAGS(flags) ? 0 : ERR_BADPARAM;
}

ULONG q_generate_id(void)
{
    Q_POOL *pool = Q_GET_POOL();
    ULONG id = pool->next_id++;
    
    if (pool->next_id == 0) {
        pool->next_id = 1;  /* Wrap around, skip 0 */
    }
    
    return id;
}

void q_name_copy(char dest[4], char src[4])
{
    int i;
    for (i = 0; i < 4; i++) {
        dest[i] = src ? src[i] : '\0';
    }
}

ULONG q_name_compare(char name1[4], char name2[4])
{
    int i;
    for (i = 0; i < 4; i++) {
        if (name1[i] != name2[i]) {
            return 0;
        }
    }
    return 1;
}

ULONG q_disable_interrupts(void)
{
    Q_STATE *state = Q_GET_STATE();
    ULONG old_level = state->interrupts_disabled;
    
    state->interrupts_disabled++;
    
    return old_level;
}

void q_enable_interrupts(void)
{
    Q_STATE *state = Q_GET_STATE();
    
    if (state->interrupts_disabled > 0) {
        state->interrupts_disabled--;
    }
}

/******************************************************************************
* Statistics Functions
******************************************************************************/

void q_update_statistics(Q_QCB *qcb, ULONG operation, ULONG value)
{
    Q_STATE *state = Q_GET_STATE();
    
    if (!Q_IS_VALID_QCB(qcb)) {
        return;
    }
    
    switch (operation) {
        case Q_STAT_SEND:
            qcb->total_sent++;
            state->total_messages_sent++;
            break;
            
        case Q_STAT_RECEIVE:
            qcb->total_received++;
            state->total_messages_received++;
            break;
            
        case Q_STAT_BROADCAST:
            qcb->total_broadcasts++;
            break;
            
        case Q_STAT_URGENT:
            qcb->total_sent++;
            state->total_messages_sent++;
            break;
            
        case Q_STAT_OVERFLOW:
            qcb->total_overflows++;
            state->total_buffer_overflows++;
            break;
            
        default:
            break;
    }
}

ULONG q_get_statistics(ULONG queue_id, ULONG *sent, ULONG *received, 
                      ULONG *broadcasts, ULONG *overflows)
{
    Q_QCB *qcb = q_pool_find(queue_id);
    
    if (qcb == NULL) {
        return ERR_OBJID;
    }
    
    if (sent) *sent = qcb->total_sent;
    if (received) *received = qcb->total_received;
    if (broadcasts) *broadcasts = qcb->total_broadcasts;
    if (overflows) *overflows = qcb->total_overflows;
    
    return 0;
}

ULONG q_get_buffer_statistics(ULONG *total, ULONG *allocated, ULONG *available)
{
    Q_BUFFER_POOL *pool = Q_GET_BUFFER_POOL();
    
    if (total) *total = pool->total_buffers;
    if (allocated) *allocated = pool->buffers_allocated;
    if (available) *available = q_buffer_available();
    
    return 0;
}

static ULONG q_init_once(void)
{
    Q_STATE *state = Q_GET_STATE();
    
    if (state->initialized) {
        return 0;
    }
    
    /* Initialize state structure */
    memset(state, 0, sizeof(Q_STATE));
    state->magic = Q_POOL_MAGIC;
    
    /* Initialize queue pool */
    q_pool_init();
    
    /* Initialize buffer pool */
    q_buffer_pool_init();
    
    /* Initialize hardware abstraction */
    q_hw_init();
    
    state->total_queues_created = 0;
    state->total_queues_deleted = 0;
    state->total_messages_sent = 0;
    state->total_messages_received = 0;
    state->total_buffer_overflows = 0;
    state->initialized = 1;
    
    return 0;
}

/******************************************************************************
* Public API Functions
******************************************************************************/

ULONG q_broadcast(ULONG qid, ULONG msg_buf[4], ULONG *count)
{
    Q_STATE *state = Q_GET_STATE();
    Q_QCB *qcb;
    ULONG error;
    
    error = q_init_once();
    if (error != 0) {
        return error;
    }
    
    if (count == NULL) {
        return ERR_BADPARAM;
    }
    
    qcb = q_pool_find(qid);
    if (qcb == NULL) {
        return ERR_OBJID;
    }
    
    if (!Q_IS_VALID_MSG(msg_buf)) {
        return ERR_BADPARAM;
    }
    
    Q_ENTER_CRITICAL();
    
    /* Hardware-specific broadcast if available */
    if (state->hw_ops && state->hw_ops->broadcast_message) {
        error = state->hw_ops->broadcast_message(qcb, msg_buf, count);
    } else {
        /* Software implementation - currently just sends to one receiver */
        error = q_message_enqueue(qcb, msg_buf, 0);
        if (error == 0) {
            *count = 1;
            q_sync_signal_message(qcb);
        } else {
            *count = 0;
        }
    }
    
    if (error == 0) {
        q_update_statistics(qcb, Q_STAT_BROADCAST, *count);
    }
    
    Q_EXIT_CRITICAL();
    
    return error;
}

ULONG q_create(char name[4], ULONG count, ULONG flags, ULONG *qid)
{
    Q_STATE *state = Q_GET_STATE();
    Q_QCB *qcb;
    ULONG error;
    ULONG buffer_start;
    
    error = q_init_once();
    if (error != 0) {
        return error;
    }
    
    if (qid == NULL) {
        return ERR_BADPARAM;
    }
    
    error = q_validate_count(count);
    if (error != 0) {
        return error;
    }
    
    error = q_validate_flags(flags);
    if (error != 0) {
        return error;
    }
    
    /* Check if buffer space is available */
    if (q_buffer_available() < count) {
        return ERR_NOMGB;
    }
    
    qcb = q_pool_alloc();
    if (qcb == NULL) {
        return ERR_NOQCB;
    }
    
    /* Allocate buffer space */
    buffer_start = q_buffer_alloc(count);
    if (buffer_start >= Q_MAX_BUFFERS) {
        q_pool_free(qcb);
        return ERR_NOMGB;
    }
    
    /* Initialize QCB fields */
    q_name_copy(qcb->name, name);
    qcb->count = count;
    qcb->flags = flags;
    qcb->current_messages = 0;
    qcb->high_water_mark = 0;
    
    /* Initialize buffer descriptor */
    qcb->buf.start = buffer_start;
    qcb->buf.end = buffer_start + count - 1;
    qcb->buf.nextin = buffer_start;
    qcb->buf.nextout = buffer_start;
    
    /* Create synchronization semaphore */
    error = q_sync_create(qcb);
    if (error != 0) {
        q_buffer_free(buffer_start, count);
        q_pool_free(qcb);
        return error;
    }
    
    /* Create hardware queue if available */
    if (state->hw_ops && state->hw_ops->create_queue) {
        error = state->hw_ops->create_queue(qcb);
        if (error != 0) {
            q_sync_delete(qcb);
            q_buffer_free(buffer_start, count);
            q_pool_free(qcb);
            return error;
        }
    }
    
    *qid = qcb->queue_id;
    state->total_queues_created++;
    
    return 0;
}

ULONG q_delete(ULONG qid)
{
    Q_STATE *state = Q_GET_STATE();
    Q_QCB *qcb;
    
    q_init_once();
    
    qcb = q_pool_find(qid);
    if (qcb == NULL) {
        return ERR_OBJID;
    }
    
    if (qcb->state != Q_STATE_ACTIVE) {
        return ERR_OBJDEL;
    }
    
    Q_ENTER_CRITICAL();
    
    qcb->state = Q_STATE_DELETING;
    
    /* Free the queue */
    q_pool_free(qcb);
    state->total_queues_deleted++;
    
    Q_EXIT_CRITICAL();
    
    return 0;
}

ULONG q_ident(char name[4], ULONG node, ULONG *qid)
{
    Q_QCB *qcb;
    
    q_init_once();
    
    if (qid == NULL) {
        return ERR_BADPARAM;
    }
    
    /* Node parameter ignored for now (single-node implementation) */
    
    qcb = q_pool_find_by_name(name);
    if (qcb == NULL) {
        return ERR_OBJNF;
    }
    
    *qid = qcb->queue_id;
    
    return 0;
}

ULONG q_receive(ULONG qid, ULONG flags, ULONG timeout, ULONG msg_buf[4])
{
    Q_STATE *state = Q_GET_STATE();
    Q_QCB *qcb;
    ULONG error;
    
    q_init_once();
    
    if (!Q_IS_VALID_MSG(msg_buf)) {
        return ERR_BADPARAM;
    }
    
    qcb = q_pool_find(qid);
    if (qcb == NULL) {
        return ERR_OBJID;
    }
    
    if (qcb->state != Q_STATE_ACTIVE) {
        return ERR_OBJDEL;
    }
    
    Q_ENTER_CRITICAL();
    
    /* Check if message is immediately available */
    if (!Q_BUFFER_EMPTY(qcb)) {
        error = q_message_dequeue(qcb, msg_buf);
        if (error == 0) {
            q_update_statistics(qcb, Q_STAT_RECEIVE, 1);
        }
        Q_EXIT_CRITICAL();
        return error;
    }
    
    /* No message available - check for no-wait flag */
    if (flags & Q_NOWAIT) {
        Q_EXIT_CRITICAL();
        return ERR_NOMSG;
    }
    
    Q_EXIT_CRITICAL();
    
    /* Wait for message using semaphore */
    error = q_sync_wait_message(qcb, flags, timeout);
    if (error != 0) {
        return error;
    }
    
    /* Message should now be available */
    Q_ENTER_CRITICAL();
    error = q_message_dequeue(qcb, msg_buf);
    if (error == 0) {
        q_update_statistics(qcb, Q_STAT_RECEIVE, 1);
    }
    Q_EXIT_CRITICAL();
    
    return error;
}

ULONG q_send(ULONG qid, ULONG msg_buf[4])
{
    Q_STATE *state = Q_GET_STATE();
    Q_QCB *qcb;
    ULONG error;
    
    q_init_once();
    
    if (!Q_IS_VALID_MSG(msg_buf)) {
        return ERR_BADPARAM;
    }
    
    qcb = q_pool_find(qid);
    if (qcb == NULL) {
        return ERR_OBJID;
    }
    
    if (qcb->state != Q_STATE_ACTIVE) {
        return ERR_OBJDEL;
    }
    
    Q_ENTER_CRITICAL();
    
    /* Hardware-specific send if available */
    if (state->hw_ops && state->hw_ops->send_message) {
        error = state->hw_ops->send_message(qcb, msg_buf);
    } else {
        /* Software implementation */
        error = q_message_enqueue(qcb, msg_buf, 0);
        if (error == 0) {
            /* Signal waiting receiver */
            q_sync_signal_message(qcb);
        }
    }
    
    if (error == 0) {
        q_update_statistics(qcb, Q_STAT_SEND, 1);
    }
    
    Q_EXIT_CRITICAL();
    
    return error;
}

ULONG q_urgent(ULONG qid, ULONG msg_buf[4])
{
    Q_STATE *state = Q_GET_STATE();
    Q_QCB *qcb;
    ULONG error;
    
    q_init_once();
    
    if (!Q_IS_VALID_MSG(msg_buf)) {
        return ERR_BADPARAM;
    }
    
    qcb = q_pool_find(qid);
    if (qcb == NULL) {
        return ERR_OBJID;
    }
    
    if (qcb->state != Q_STATE_ACTIVE) {
        return ERR_OBJDEL;
    }
    
    Q_ENTER_CRITICAL();
    
    /* Send as urgent message (front of queue) */
    error = q_message_enqueue(qcb, msg_buf, 1);
    if (error == 0) {
        /* Signal waiting receiver */
        q_sync_signal_message(qcb);
        q_update_statistics(qcb, Q_STAT_URGENT, 1);
    }
    
    Q_EXIT_CRITICAL();
    
    return error;
}

/* Variable-size queue functions - stub implementations */
ULONG q_vcreate(char name[4], ULONG flags, ULONG maxnum, ULONG maxlen, ULONG *qid)
{
    /* Variable-size queues not implemented yet */
    return ERR_BADPARAM;
}

ULONG q_vdelete(ULONG qid)
{
    /* Variable-size queues not implemented yet */
    return ERR_BADPARAM;
}

ULONG q_vident(char name[4], ULONG node, ULONG *qid)
{
    /* Variable-size queues not implemented yet */
    return ERR_OBJNF;
}

ULONG q_vreceive(ULONG qid, ULONG flags, ULONG timeout, void *msgbuf, ULONG buf_len, ULONG *msg_len)
{
    /* For now, call fixed-size receive as compatibility measure */
    return q_receive(qid, flags, timeout, (ULONG*)msgbuf);
}

ULONG q_vsend(ULONG qid, void *msgbuf, ULONG msg_len)
{
    /* For now, call fixed-size send as compatibility measure */
    return q_send(qid, (ULONG*)msgbuf);
}

/******************************************************************************
* Hardware Abstraction
******************************************************************************/

ULONG q_hw_init(void)
{
    Q_STATE *state = Q_GET_STATE();
    
    /* Set up hardware operations - would be selected based on platform */
    #ifdef STM32F4
    state->hw_ops = &q_hw_stm32f4_ops;
    #else
    state->hw_ops = &q_hw_generic_ops;
    #endif
    
    /* Initialize hardware */
    if (state->hw_ops && state->hw_ops->init) {
        return state->hw_ops->init();
    }
    
    return 0;
}

void q_hw_cleanup(void)
{
    Q_STATE *state = Q_GET_STATE();
    
    if (state->hw_ops && state->hw_ops->cleanup) {
        state->hw_ops->cleanup();
    }
}

/******************************************************************************
* Legacy Compatibility and Additional Functions
******************************************************************************/

/* Initialize queue system (called during kernel initialization) */
ULONG gxk_q_init(void)
{
    return q_init_once();
}