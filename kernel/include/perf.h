/*
 * Performance Optimization Subsystem
 * Lazy loading, fast paths, and performance monitoring
 */

#ifndef PERF_H
#define PERF_H

#include <stdint.h>
#include <stdbool.h>
#include "kernel.h"

/* Performance modes */
typedef enum perf_mode {
    PERF_MODE_IDLE,          /* Minimal resource usage */
    PERF_MODE_BALANCED,      /* Default mode */
    PERF_MODE_PERFORMANCE,   /* Maximum performance */
    PERF_MODE_POWER_SAVE     /* Minimum power consumption */
} perf_mode_t;

/* Fast path optimization flags */
#define PERF_FASTPATH_ENABLED    (1 << 0)
#define PERF_FASTPATH_SYSCALL    (1 << 1)
#define PERF_FASTPATH_IPC        (1 << 2)
#define PERF_FASTPATH_MEMORY     (1 << 3)
#define PERF_FASTPATH_IO         (1 << 4)

/* Lazy loading flags */
#define PERF_LAZY_MODULES        (1 << 0)
#define PERF_LAZY_DRIVERS        (1 << 1)
#define PERF_LAZY_SERVICES       (1 << 2)
#define PERF_LAZY_GRAPHICS       (1 << 3)

/* Performance metrics */
typedef struct perf_metrics {
    /* CPU metrics */
    uint64_t cpu_cycles;
    uint64_t cpu_instructions;
    uint32_t cpu_utilization;      /* 0-100% */

    /* Memory metrics */
    uint64_t memory_allocated;
    uint64_t memory_freed;
    uint64_t memory_peak;
    uint32_t memory_pressure;      /* 0-100% */

    /* I/O metrics */
    uint64_t io_reads;
    uint64_t io_writes;
    uint64_t io_bytes_read;
    uint64_t io_bytes_written;

    /* Syscall metrics */
    uint64_t syscalls_total;
    uint64_t syscalls_fast_path;
    uint64_t syscalls_slow_path;

    /* IPC metrics */
    uint64_t ipc_messages;
    uint64_t ipc_fast_path;
    uint64_t ipc_latency_avg_ns;

    /* Cache metrics */
    uint64_t cache_hits;
    uint64_t cache_misses;
    uint32_t cache_hit_rate;       /* 0-100% */

    /* Boot/wake metrics */
    uint64_t boot_time_ms;
    uint64_t wake_time_ms;
    uint64_t app_launch_avg_ms;
} perf_metrics_t;

/* Module load priority */
typedef enum module_priority {
    MODULE_PRIORITY_CRITICAL,    /* Load immediately at boot */
    MODULE_PRIORITY_HIGH,        /* Load early in boot */
    MODULE_PRIORITY_NORMAL,      /* Load when needed */
    MODULE_PRIORITY_LOW,         /* Load on-demand */
    MODULE_PRIORITY_LAZY         /* Load only if explicitly requested */
} module_priority_t;

/* Module metadata for lazy loading */
typedef struct lazy_module {
    const char* name;
    module_priority_t priority;
    uint32_t dependencies_count;
    const char** dependencies;
    bool loaded;
    void* module_data;
    status_t (*init_fn)(void);
    status_t (*cleanup_fn)(void);
} lazy_module_t;

/* Fast path cache entry */
typedef struct fastpath_cache_entry {
    uint64_t key;
    uint64_t value;
    uint64_t timestamp;
    uint32_t access_count;
    bool valid;
} fastpath_cache_entry_t;

/* Fast path cache */
#define FASTPATH_CACHE_SIZE 256
typedef struct fastpath_cache {
    fastpath_cache_entry_t entries[FASTPATH_CACHE_SIZE];
    uint32_t hits;
    uint32_t misses;
    uint32_t evictions;
} fastpath_cache_t;

/* Performance configuration */
typedef struct perf_config {
    perf_mode_t mode;
    uint32_t fastpath_flags;
    uint32_t lazy_flags;
    bool aggressive_idle;
    bool hardware_accel;
    uint32_t idle_timeout_ms;
    uint32_t cache_size_kb;
} perf_config_t;

/* Initialize performance subsystem */
status_t perf_init(void);

/* Performance mode management */
status_t perf_set_mode(perf_mode_t mode);
perf_mode_t perf_get_mode(void);

/* Fast path operations */
status_t perf_fastpath_enable(uint32_t flags);
status_t perf_fastpath_disable(uint32_t flags);
bool perf_is_fastpath_enabled(uint32_t flag);

/* Fast path cache */
status_t perf_cache_insert(uint64_t key, uint64_t value);
status_t perf_cache_lookup(uint64_t key, uint64_t* out_value);
void perf_cache_invalidate(uint64_t key);
void perf_cache_clear(void);

/* Lazy loading */
status_t perf_register_lazy_module(lazy_module_t* module);
status_t perf_load_module(const char* name);
status_t perf_unload_module(const char* name);
bool perf_is_module_loaded(const char* name);

/* Idle management */
status_t perf_enter_idle(void);
status_t perf_exit_idle(void);
bool perf_is_idle(void);

/* Hardware acceleration */
status_t perf_enable_hardware_accel(void);
status_t perf_disable_hardware_accel(void);

/* Metrics and monitoring */
void perf_get_metrics(perf_metrics_t* metrics);
void perf_reset_metrics(void);
void perf_update_cpu_utilization(uint32_t utilization);
void perf_update_memory_pressure(uint32_t pressure);

/* Performance profiling */
uint64_t perf_timestamp_ns(void);
uint64_t perf_cycles(void);

/* Inline helpers for fast path checking */
static inline bool perf_should_use_fastpath(void) {
    extern uint32_t g_perf_fastpath_flags;
    return (g_perf_fastpath_flags & PERF_FASTPATH_ENABLED) != 0;
}

static inline void perf_record_syscall(bool fast_path) {
    extern perf_metrics_t g_perf_metrics;
    g_perf_metrics.syscalls_total++;
    if (fast_path) {
        g_perf_metrics.syscalls_fast_path++;
    } else {
        g_perf_metrics.syscalls_slow_path++;
    }
}

static inline void perf_record_ipc(bool fast_path, uint64_t latency_ns) {
    extern perf_metrics_t g_perf_metrics;
    g_perf_metrics.ipc_messages++;
    if (fast_path) {
        g_perf_metrics.ipc_fast_path++;
    }
    /* Update running average */
    uint64_t total = g_perf_metrics.ipc_messages;
    g_perf_metrics.ipc_latency_avg_ns =
        ((g_perf_metrics.ipc_latency_avg_ns * (total - 1)) + latency_ns) / total;
}

#endif /* PERF_H */
