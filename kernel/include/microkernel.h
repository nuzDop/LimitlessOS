#ifndef LIMITLESS_MICROKERNEL_H
#define LIMITLESS_MICROKERNEL_H

#include "kernel.h"

/*
 * LimitlessOS Microkernel Architecture
 *
 * Core principles:
 * - Minimal kernel: IPC, scheduling, memory management, interrupt handling
 * - User-space drivers: All device drivers run in user space
 * - Capability-based security: Fine-grained permissions
 * - Zero-copy IPC: High-performance message passing
 * - Deterministic latency: Real-time scheduling support
 */

/* ============================================================================
 * List Data Structure (needed by process and other structures)
 * ============================================================================ */

struct list_head {
    struct list_head* next;
    struct list_head* prev;
};

/* ============================================================================
 * Process Management
 * ============================================================================ */

typedef uint64_t pid_t;
typedef uint64_t tid_t;

#define PID_INVALID 0
#define TID_INVALID 0

/* Process states */
typedef enum {
    PROC_STATE_CREATED,
    PROC_STATE_READY,
    PROC_STATE_RUNNING,
    PROC_STATE_BLOCKED,
    PROC_STATE_ZOMBIE,
    PROC_STATE_TERMINATED,
} proc_state_t;

/* Process priorities */
typedef enum {
    PRIORITY_IDLE = 0,
    PRIORITY_LOW = 1,
    PRIORITY_NORMAL = 2,
    PRIORITY_HIGH = 3,
    PRIORITY_REALTIME = 4,
} priority_t;

/* Process and thread descriptors (defined in process.h) */
struct process;
struct thread;
typedef struct process process_t;
typedef struct thread thread_t;

/* ============================================================================
 * Inter-Process Communication (IPC)
 * ============================================================================ */

typedef uint64_t ipc_endpoint_t;
typedef uint64_t ipc_msg_id_t;

#define IPC_ENDPOINT_INVALID 0

/* IPC message structure */
#define IPC_MSG_DATA_SIZE 120

typedef struct ipc_message {
    ipc_msg_id_t id;
    pid_t sender;
    uint32_t size;
    uint32_t flags;
    uint8_t data[IPC_MSG_DATA_SIZE];
} ALIGNED(128) ipc_message_t;

/* IPC flags */
#define IPC_FLAG_SYNC      BIT(0)  // Synchronous (wait for reply)
#define IPC_FLAG_ASYNC     BIT(1)  // Asynchronous (fire and forget)
#define IPC_FLAG_ZEROCOPY  BIT(2)  // Zero-copy transfer
#define IPC_FLAG_PRIORITY  BIT(3)  // High-priority message

/* IPC syscalls */
status_t ipc_endpoint_create(ipc_endpoint_t* out_endpoint);
status_t ipc_endpoint_destroy(ipc_endpoint_t endpoint);
status_t ipc_send(ipc_endpoint_t endpoint, ipc_message_t* msg, uint64_t timeout_ns);
status_t ipc_receive(ipc_endpoint_t endpoint, ipc_message_t* msg, uint64_t timeout_ns);
status_t ipc_reply(ipc_endpoint_t endpoint, ipc_msg_id_t msg_id, const void* data, size_t size);

/* ============================================================================
 * Memory Management
 * ============================================================================ */

/* Virtual memory flags */
#define VM_FLAG_READ       BIT(0)
#define VM_FLAG_WRITE      BIT(1)
#define VM_FLAG_EXEC       BIT(2)
#define VM_FLAG_USER       BIT(3)
#define VM_FLAG_NOCACHE    BIT(4)
#define VM_FLAG_SHARED     BIT(5)

/* Memory regions */
#define VM_REGION_KERNEL_START  0xFFFFFF8000000000UL
#define VM_REGION_USER_START    0x0000000000400000UL
#define VM_REGION_USER_END      0x00007FFFFFFFF000UL

/* Address space (defined in vmm.h) */
struct address_space;
typedef struct address_space address_space_t;

/* Memory syscalls */
status_t vm_map(vaddr_t vaddr, paddr_t paddr, size_t size, uint32_t flags);
status_t vm_unmap(vaddr_t vaddr, size_t size);
status_t vm_protect(vaddr_t vaddr, size_t size, uint32_t flags);
status_t vm_allocate(size_t size, uint32_t flags, vaddr_t* out_vaddr);
status_t vm_free(vaddr_t vaddr);

/* Physical memory allocator */
status_t pmm_init(paddr_t start, size_t size);
paddr_t pmm_alloc_page(void);
void pmm_free_page(paddr_t page);
paddr_t pmm_alloc_pages(size_t count);
void pmm_free_pages(paddr_t base, size_t count);
size_t pmm_get_free_memory(void);
size_t pmm_get_total_memory(void);

/* ============================================================================
 * Capability System
 * ============================================================================ */

typedef uint64_t cap_t;

#define CAP_INVALID 0

/* Capability types */
typedef enum {
    CAP_TYPE_NONE = 0,
    CAP_TYPE_MEMORY,
    CAP_TYPE_IPC_ENDPOINT,
    CAP_TYPE_THREAD,
    CAP_TYPE_PROCESS,
    CAP_TYPE_IRQ,
    CAP_TYPE_IOPORT,
    CAP_TYPE_DEVICE,
} cap_type_t;

/* Capability permissions */
#define CAP_PERM_READ      BIT(0)
#define CAP_PERM_WRITE     BIT(1)
#define CAP_PERM_EXECUTE   BIT(2)
#define CAP_PERM_GRANT     BIT(3)  // Can grant this capability to others
#define CAP_PERM_REVOKE    BIT(4)  // Can revoke this capability

/* Capability descriptor */
typedef struct capability {
    cap_t id;
    cap_type_t type;
    uint32_t permissions;
    uint64_t object_id;
    uint64_t metadata;
} capability_t;

/* Capability syscalls */
status_t cap_create(cap_type_t type, uint64_t object_id, uint32_t perms, cap_t* out_cap);
status_t cap_destroy(cap_t cap);
status_t cap_grant(pid_t target_pid, cap_t cap);
status_t cap_revoke(pid_t target_pid, cap_t cap);
status_t cap_derive(cap_t parent_cap, uint32_t new_perms, cap_t* out_cap);

/* ============================================================================
 * Interrupt Handling
 * ============================================================================ */

typedef uint32_t irq_t;
typedef void (*irq_handler_t)(irq_t irq, void* context);

#define IRQ_INVALID 0xFFFFFFFF

/* IRQ syscalls */
status_t irq_register(irq_t irq, irq_handler_t handler, void* context);
status_t irq_unregister(irq_t irq);
status_t irq_enable(irq_t irq);
status_t irq_disable(irq_t irq);
status_t irq_wait(irq_t irq, uint64_t timeout_ns);

/* ============================================================================
 * Scheduling
 * ============================================================================ */

/* Scheduler policies */
typedef enum {
    SCHED_POLICY_ROUNDROBIN,
    SCHED_POLICY_PRIORITY,
    SCHED_POLICY_REALTIME,
    SCHED_POLICY_FIFO,
} sched_policy_t;

/* Scheduler configuration */
typedef struct sched_config {
    sched_policy_t policy;
    uint64_t time_slice_ns;
    bool preemptive;
    bool smp_enabled;
} sched_config_t;

/* Scheduler syscalls */
status_t sched_init(sched_config_t* config);
void sched_schedule(void);
thread_t* sched_get_current_thread(void);
void sched_add_thread(thread_t* thread);
void sched_remove_thread(thread_t* thread);
void sched_yield(void);

/* ============================================================================
 * List Data Structure (functions)
 * ============================================================================ */

static ALWAYS_INLINE void list_init(struct list_head* list) {
    list->next = list;
    list->prev = list;
}

static ALWAYS_INLINE void list_add(struct list_head* new_node, struct list_head* head) {
    new_node->next = head->next;
    new_node->prev = head;
    head->next->prev = new_node;
    head->next = new_node;
}

static ALWAYS_INLINE void list_del(struct list_head* node) {
    node->prev->next = node->next;
    node->next->prev = node->prev;
    node->next = node;
    node->prev = node;
}

static ALWAYS_INLINE bool list_empty(struct list_head* head) {
    return head->next == head;
}

#define list_entry(ptr, type, member) \
    container_of(ptr, type, member)

#define list_for_each(pos, head) \
    for (pos = (head)->next; pos != (head); pos = pos->next)

#define list_for_each_safe(pos, n, head) \
    for (pos = (head)->next, n = pos->next; pos != (head); pos = n, n = pos->next)

#endif /* LIMITLESS_MICROKERNEL_H */
