#ifndef LIMITLESS_AI_HOOKS_H
#define LIMITLESS_AI_HOOKS_H

/*
 * LimitlessOS AI Integration Hooks
 * 
 * This subsystem provides hook points throughout the kernel for AI-driven
 * optimization, telemetry collection, and intelligent decision-making.
 * 
 * Design Principles:
 * - Zero overhead when AI is disabled
 * - Minimal performance impact (< 1% CPU overhead)
 * - Modular and extensible
 * - Kernel-userspace IPC for AI processing
 * - Real-time telemetry streaming
 */

#include "kernel.h"
#include "microkernel.h"

/* AI Hook Configuration */
typedef struct ai_config {
    bool enabled;                       // Master AI enable/disable
    bool telemetry_enabled;             // Collect telemetry data
    bool decision_support_enabled;      // AI-guided decisions
    bool realtime_inference;            // Real-time ML inference in kernel
    uint32_t telemetry_buffer_size;     // Telemetry buffer size (KB)
    uint32_t decision_cache_size;       // Decision cache entries
    ipc_endpoint_t ai_service_endpoint; // Userspace AI service IPC endpoint
} ai_config_t;

/* AI Hook Types */
typedef enum {
    AI_HOOK_SCHEDULER,          // Scheduler decisions
    AI_HOOK_MEMORY,             // Memory allocation/paging
    AI_HOOK_IO,                 // I/O operations
    AI_HOOK_IPC,                // IPC message routing
    AI_HOOK_NETWORK,            // Network packet processing
    AI_HOOK_SECURITY,           // Security policy decisions
    AI_HOOK_POWER,              // Power management
    AI_HOOK_DRIVER,             // Driver behavior
    AI_HOOK_COUNT
} ai_hook_type_t;

/* Telemetry Event Types */
typedef enum {
    AI_EVENT_PROCESS_CREATE,
    AI_EVENT_PROCESS_EXIT,
    AI_EVENT_THREAD_SWITCH,
    AI_EVENT_PAGE_FAULT,
    AI_EVENT_PAGE_ALLOC,
    AI_EVENT_IPC_SEND,
    AI_EVENT_IPC_RECV,
    AI_EVENT_IO_READ,
    AI_EVENT_IO_WRITE,
    AI_EVENT_NETWORK_RX,
    AI_EVENT_NETWORK_TX,
    AI_EVENT_INTERRUPT,
    AI_EVENT_SYSCALL,
    AI_EVENT_SECURITY_CHECK,
    AI_EVENT_POWER_STATE,
    AI_EVENT_COUNT
} ai_event_type_t;

/* Telemetry Event Structure (cache-line aligned for performance) */
typedef struct ai_telemetry_event {
    ai_event_type_t type;       // Event type
    uint64_t timestamp;         // CPU timestamp counter
    uint32_t cpu_id;            // CPU core ID
    uint32_t pid;               // Process ID
    uint64_t data[4];           // Event-specific data
    uint32_t flags;             // Additional flags
    uint32_t reserved;          // Reserved for alignment
} ALIGNED(64) ai_telemetry_event_t;

/* AI Decision Request */
typedef struct ai_decision_request {
    ai_hook_type_t hook_type;   // Which subsystem
    uint32_t context_id;        // Context identifier
    uint64_t input_data[8];     // Input parameters
    uint32_t urgency;           // Decision urgency (0=low, 100=critical)
    bool use_cache;             // Use cached decision if available
} ai_decision_request_t;

/* AI Decision Response */
typedef struct ai_decision_response {
    uint32_t decision_id;       // Unique decision ID
    uint64_t output_data[8];    // Output parameters
    uint32_t confidence;        // Confidence level (0-100)
    bool cache_result;          // Should this be cached?
    uint64_t expiry_time;       // Cache expiry timestamp
} ai_decision_response_t;

/* AI Hook Statistics */
typedef struct ai_hook_stats {
    uint64_t events_generated;  // Total telemetry events
    uint64_t decisions_requested; // Total decision requests
    uint64_t decisions_cached;  // Cache hits
    uint64_t decisions_ai;      // AI-generated decisions
    uint64_t decisions_fallback; // Fallback to default
    uint64_t total_latency_ns;  // Total decision latency
    uint64_t max_latency_ns;    // Maximum latency
} ai_hook_stats_t;

/* AI Hooks API */

/* Initialize AI hooks subsystem */
status_t ai_hooks_init(ai_config_t* config);

/* Shutdown AI hooks */
void ai_hooks_shutdown(void);

/* Enable/disable AI processing */
status_t ai_hooks_enable(bool enable);

/* Configure AI subsystem */
status_t ai_hooks_configure(ai_config_t* config);

/* Get current configuration */
ai_config_t* ai_hooks_get_config(void);

/* Get AI hook statistics */
ai_hook_stats_t* ai_hooks_get_stats(ai_hook_type_t hook_type);

/* Register userspace AI service */
status_t ai_hooks_register_service(ipc_endpoint_t endpoint);

/* Telemetry API */

/* Emit telemetry event (fast path - inline candidate) */
static inline void ai_emit_event(ai_event_type_t type, uint32_t pid, 
                                  uint64_t data0, uint64_t data1) {
    // Fast path: check if telemetry is enabled
    extern bool g_ai_telemetry_enabled;
    if (!g_ai_telemetry_enabled) return;
    
    // Call slow path for actual logging
    extern void ai_emit_event_slow(ai_event_type_t type, uint32_t pid,
                                   uint64_t data0, uint64_t data1,
                                   uint64_t data2, uint64_t data3);
    ai_emit_event_slow(type, pid, data0, data1, 0, 0);
}

/* Full telemetry event emission */
void ai_emit_event_full(ai_telemetry_event_t* event);

/* Get telemetry buffer (for userspace consumption) */
status_t ai_get_telemetry_buffer(void** buffer, size_t* size);

/* Decision Support API */

/* Request AI decision (synchronous) */
status_t ai_request_decision(ai_decision_request_t* request,
                             ai_decision_response_t* response,
                             uint64_t timeout_ns);

/* Request AI decision (asynchronous) */
status_t ai_request_decision_async(ai_decision_request_t* request,
                                   void (*callback)(ai_decision_response_t*, void*),
                                   void* user_data);

/* Cache management */
status_t ai_cache_decision(ai_hook_type_t hook_type, uint32_t context_id,
                          ai_decision_response_t* decision);

status_t ai_invalidate_cache(ai_hook_type_t hook_type);

/* Convenience Wrappers for Common Operations */

/* Scheduler: Should we preempt this thread? */
static inline bool ai_should_preempt(tid_t current_tid, tid_t candidate_tid) {
    extern bool g_ai_decision_enabled;
    if (!g_ai_decision_enabled) return false; // Use default scheduler logic
    
    ai_decision_request_t req = {
        .hook_type = AI_HOOK_SCHEDULER,
        .context_id = current_tid,
        .input_data = {current_tid, candidate_tid, 0, 0, 0, 0, 0, 0},
        .urgency = 50,
        .use_cache = true
    };
    
    ai_decision_response_t resp;
    status_t status = ai_request_decision(&req, &resp, 1000000); // 1ms timeout
    
    if (SUCCESS(status) && resp.confidence > 70) {
        return resp.output_data[0] != 0;
    }
    
    return false; // Fallback to default
}

/* Memory: Should we swap this page? */
static inline bool ai_should_swap_page(paddr_t page, uint32_t access_count) {
    extern bool g_ai_decision_enabled;
    if (!g_ai_decision_enabled) return false;
    
    // Implementation similar to ai_should_preempt
    return false; // Fallback to default LRU
}

/* I/O: Predict next block to read-ahead */
static inline uint64_t ai_predict_readahead(uint64_t current_block, uint32_t size) {
    extern bool g_ai_decision_enabled;
    if (!g_ai_decision_enabled) return current_block + size; // Sequential
    
    // AI can predict non-sequential patterns
    return current_block + size;
}

/* Network: Prioritize this packet? */
static inline uint32_t ai_get_packet_priority(void* packet, size_t size) {
    extern bool g_ai_decision_enabled;
    if (!g_ai_decision_enabled) return 0; // Default priority
    
    // AI can analyze packet contents for QoS
    return 0;
}

/* Security: Is this operation suspicious? */
static inline bool ai_is_suspicious(pid_t pid, const char* operation) {
    extern bool g_ai_decision_enabled;
    if (!g_ai_decision_enabled) return false;
    
    ai_emit_event(AI_EVENT_SECURITY_CHECK, pid, (uint64_t)operation, 0);
    
    // AI anomaly detection could flag unusual patterns
    return false;
}

/* Power Management: Should we enter low-power state? */
static inline bool ai_should_power_save(uint32_t idle_time_ms) {
    extern bool g_ai_decision_enabled;
    if (!g_ai_decision_enabled) return idle_time_ms > 100;
    
    // AI can predict workload patterns
    return idle_time_ms > 100;
}

/* Global AI state (defined in ai_hooks.c) */
extern bool g_ai_telemetry_enabled;
extern bool g_ai_decision_enabled;

/* Kernel Module Interface */

/* AI hook callback type */
typedef void (*ai_hook_callback_t)(ai_telemetry_event_t* event);

/* Register custom AI hook */
status_t ai_register_hook(ai_hook_type_t type, ai_hook_callback_t callback);

/* Unregister AI hook */
status_t ai_unregister_hook(ai_hook_type_t type);

#endif /* LIMITLESS_AI_HOOKS_H */
