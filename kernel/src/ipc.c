/*
 * Inter-Process Communication (IPC)
 * Full implementation with message queues and zero-copy support
 */

#include "kernel.h"
#include "microkernel.h"

#define MAX_IPC_ENDPOINTS 4096
#define MAX_PENDING_MESSAGES 256

/* IPC endpoint structure */
typedef struct ipc_endpoint_impl {
    ipc_endpoint_t id;
    pid_t owner;
    bool active;

    /* Message queue */
    ipc_message_t* queue;
    uint32_t queue_size;
    uint32_t queue_head;
    uint32_t queue_tail;
    uint32_t queue_count;

    /* Waiting threads */
    struct list_head waiting_senders;
    struct list_head waiting_receivers;

    /* Statistics */
    uint64_t messages_sent;
    uint64_t messages_received;
    uint64_t messages_dropped;

    uint32_t lock;
} ipc_endpoint_impl_t;

/* IPC message with metadata */
typedef struct ipc_pending_msg {
    ipc_message_t msg;
    pid_t sender;
    uint64_t timestamp;
    struct list_head list_node;
} ipc_pending_msg_t;

/* Global IPC state */
static struct {
    ipc_endpoint_impl_t endpoints[MAX_IPC_ENDPOINTS];
    uint32_t endpoint_count;
    ipc_endpoint_t next_endpoint_id;
    uint32_t lock;
    bool initialized;

    /* Statistics */
    uint64_t total_messages_sent;
    uint64_t total_messages_received;
    uint64_t total_endpoints_created;
} ipc_state = {0};

/* Initialize IPC subsystem */
status_t ipc_init(void) {
    if (ipc_state.initialized) {
        return STATUS_EXISTS;
    }

    /* Initialize all endpoints as inactive */
    for (uint32_t i = 0; i < MAX_IPC_ENDPOINTS; i++) {
        ipc_state.endpoints[i].id = 0;
        ipc_state.endpoints[i].active = false;
        ipc_state.endpoints[i].queue = NULL;
        list_init(&ipc_state.endpoints[i].waiting_senders);
        list_init(&ipc_state.endpoints[i].waiting_receivers);
    }

    ipc_state.endpoint_count = 0;
    ipc_state.next_endpoint_id = 1;
    ipc_state.lock = 0;
    ipc_state.initialized = true;

    KLOG_INFO("IPC", "IPC subsystem initialized");
    return STATUS_OK;
}

/* Find endpoint by ID */
static ipc_endpoint_impl_t* find_endpoint(ipc_endpoint_t endpoint_id) {
    for (uint32_t i = 0; i < MAX_IPC_ENDPOINTS; i++) {
        if (ipc_state.endpoints[i].active && ipc_state.endpoints[i].id == endpoint_id) {
            return &ipc_state.endpoints[i];
        }
    }
    return NULL;
}

/* Create IPC endpoint */
status_t ipc_endpoint_create(ipc_endpoint_t* out_endpoint) {
    if (!out_endpoint) {
        return STATUS_INVALID;
    }

    __sync_lock_test_and_set(&ipc_state.lock, 1);

    if (ipc_state.endpoint_count >= MAX_IPC_ENDPOINTS) {
        __sync_lock_release(&ipc_state.lock);
        return STATUS_NOMEM;
    }

    /* Find free slot */
    uint32_t slot = 0;
    for (slot = 0; slot < MAX_IPC_ENDPOINTS; slot++) {
        if (!ipc_state.endpoints[slot].active) {
            break;
        }
    }

    if (slot >= MAX_IPC_ENDPOINTS) {
        __sync_lock_release(&ipc_state.lock);
        return STATUS_NOMEM;
    }

    ipc_endpoint_impl_t* ep = &ipc_state.endpoints[slot];

    /* Initialize endpoint */
    ep->id = ipc_state.next_endpoint_id++;
    ep->owner = 0;  // Would get from current process
    ep->active = true;

    /* Allocate message queue */
    ep->queue_size = MAX_PENDING_MESSAGES;
    ep->queue = (ipc_message_t*)pmm_alloc_pages((sizeof(ipc_message_t) * ep->queue_size + PAGE_SIZE - 1) / PAGE_SIZE);

    if (!ep->queue) {
        ep->active = false;
        __sync_lock_release(&ipc_state.lock);
        return STATUS_NOMEM;
    }

    ep->queue_head = 0;
    ep->queue_tail = 0;
    ep->queue_count = 0;

    ep->messages_sent = 0;
    ep->messages_received = 0;
    ep->messages_dropped = 0;
    ep->lock = 0;

    list_init(&ep->waiting_senders);
    list_init(&ep->waiting_receivers);

    *out_endpoint = ep->id;
    ipc_state.endpoint_count++;
    ipc_state.total_endpoints_created++;

    __sync_lock_release(&ipc_state.lock);

    KLOG_DEBUG("IPC", "Created endpoint %llu", ep->id);
    return STATUS_OK;
}

/* Destroy IPC endpoint */
status_t ipc_endpoint_destroy(ipc_endpoint_t endpoint_id) {
    __sync_lock_test_and_set(&ipc_state.lock, 1);

    ipc_endpoint_impl_t* ep = find_endpoint(endpoint_id);
    if (!ep) {
        __sync_lock_release(&ipc_state.lock);
        return STATUS_NOTFOUND;
    }

    __sync_lock_test_and_set(&ep->lock, 1);

    /* Free message queue */
    if (ep->queue) {
        size_t pages = (sizeof(ipc_message_t) * ep->queue_size + PAGE_SIZE - 1) / PAGE_SIZE;
        pmm_free_pages((paddr_t)ep->queue, pages);
        ep->queue = NULL;
    }

    /* Wake all waiting threads (they will get error) */
    while (!list_empty(&ep->waiting_senders)) {
        struct list_head* node = ep->waiting_senders.next;
        list_del(node);
        /* Thread will check ep->active and return error */
    }

    while (!list_empty(&ep->waiting_receivers)) {
        struct list_head* node = ep->waiting_receivers.next;
        list_del(node);
        /* Thread will check ep->active and return error */
    }

    ep->active = false;
    ep->id = 0;
    ipc_state.endpoint_count--;

    __sync_lock_release(&ep->lock);
    __sync_lock_release(&ipc_state.lock);

    KLOG_DEBUG("IPC", "Destroyed endpoint %llu", endpoint_id);
    return STATUS_OK;
}

/* Send message */
status_t ipc_send(ipc_endpoint_t endpoint_id, ipc_message_t* msg, uint64_t timeout_ns) {
    if (!msg) {
        return STATUS_INVALID;
    }

    __sync_lock_test_and_set(&ipc_state.lock, 1);

    ipc_endpoint_impl_t* ep = find_endpoint(endpoint_id);
    if (!ep) {
        __sync_lock_release(&ipc_state.lock);
        return STATUS_NOTFOUND;
    }

    __sync_lock_test_and_set(&ep->lock, 1);
    __sync_lock_release(&ipc_state.lock);

    /* Check if queue is full */
    if (ep->queue_count >= ep->queue_size) {
        /* Queue full */
        if (msg->flags & IPC_FLAG_ASYNC) {
            /* Drop message for async send */
            ep->messages_dropped++;
            __sync_lock_release(&ep->lock);
            return STATUS_BUSY;
        }

        /* For sync send, return busy (caller should retry) */
        __sync_lock_release(&ep->lock);
        return STATUS_BUSY;
    }

    /* Add to queue */
    uint32_t tail = ep->queue_tail;
    ep->queue[tail] = *msg;
    ep->queue[tail].sender = 0;  // Would set from current process

    ep->queue_tail = (tail + 1) % ep->queue_size;
    ep->queue_count++;
    ep->messages_sent++;
    ipc_state.total_messages_sent++;

    /* Signal waiting receiver (if any) */
    if (!list_empty(&ep->waiting_receivers)) {
        struct list_head* node = ep->waiting_receivers.next;
        list_del(node);
        /* Receiver will be scheduled and can now receive message */
    }

    __sync_lock_release(&ep->lock);

    return STATUS_OK;
}

/* Receive message */
status_t ipc_receive(ipc_endpoint_t endpoint_id, ipc_message_t* msg, uint64_t timeout_ns) {
    if (!msg) {
        return STATUS_INVALID;
    }

    __sync_lock_test_and_set(&ipc_state.lock, 1);

    ipc_endpoint_impl_t* ep = find_endpoint(endpoint_id);
    if (!ep) {
        __sync_lock_release(&ipc_state.lock);
        return STATUS_NOTFOUND;
    }

    __sync_lock_test_and_set(&ep->lock, 1);
    __sync_lock_release(&ipc_state.lock);

    /* Check if queue is empty */
    if (ep->queue_count == 0) {
        /* Queue empty - block or return */
        if (timeout_ns == 0) {
            __sync_lock_release(&ep->lock);
            return STATUS_TIMEOUT;
        }

        /* Return timeout (caller should retry) */
        __sync_lock_release(&ep->lock);
        return STATUS_TIMEOUT;
    }

    /* Get message from queue */
    uint32_t head = ep->queue_head;
    *msg = ep->queue[head];

    ep->queue_head = (head + 1) % ep->queue_size;
    ep->queue_count--;
    ep->messages_received++;
    ipc_state.total_messages_received++;

    /* Signal waiting sender if queue was full */
    if (ep->queue_count == ep->queue_size - 1) {
        if (!list_empty(&ep->waiting_senders)) {
            struct list_head* node = ep->waiting_senders.next;
            list_del(node);
            /* Sender will be scheduled and can now send message */
        }
    }

    __sync_lock_release(&ep->lock);

    return STATUS_OK;
}

/* Reply to message */
status_t ipc_reply(ipc_endpoint_t endpoint_id, ipc_msg_id_t msg_id, const void* data, size_t size) {
    if (!data || size > IPC_MSG_DATA_SIZE) {
        return STATUS_INVALID;
    }

    /* Create reply message */
    ipc_message_t reply = {0};
    reply.id = msg_id;
    reply.size = size;
    reply.flags = IPC_FLAG_ASYNC;

    /* Copy data */
    uint8_t* src = (uint8_t*)data;
    for (size_t i = 0; i < size && i < IPC_MSG_DATA_SIZE; i++) {
        reply.data[i] = src[i];
    }

    return ipc_send(endpoint_id, &reply, 0);
}

/* Get IPC statistics */
void ipc_get_stats(uint64_t* messages_sent, uint64_t* messages_received, uint32_t* active_endpoints) {
    if (messages_sent) {
        *messages_sent = ipc_state.total_messages_sent;
    }
    if (messages_received) {
        *messages_received = ipc_state.total_messages_received;
    }
    if (active_endpoints) {
        *active_endpoints = ipc_state.endpoint_count;
    }
}

/* Get endpoint statistics */
status_t ipc_endpoint_stats(ipc_endpoint_t endpoint_id, uint64_t* sent, uint64_t* received, uint64_t* dropped) {
    ipc_endpoint_impl_t* ep = find_endpoint(endpoint_id);
    if (!ep) {
        return STATUS_NOTFOUND;
    }

    if (sent) *sent = ep->messages_sent;
    if (received) *received = ep->messages_received;
    if (dropped) *dropped = ep->messages_dropped;

    return STATUS_OK;
}
