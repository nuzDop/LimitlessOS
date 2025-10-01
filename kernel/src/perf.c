/*
 * Performance Optimization Subsystem
 * Implementation of lazy loading, fast paths, and performance monitoring
 */

#include "kernel.h"
#include "perf.h"
#include "microkernel.h"
#include "string.h"

/* Global performance state */
static struct {
    perf_config_t config;
    perf_metrics_t metrics;
    fastpath_cache_t cache;
    lazy_module_t* modules[128];
    uint32_t module_count;
    bool initialized;
    bool idle;
    uint64_t idle_enter_time;
    uint32_t lock;
} perf_state = {0};

/* Simple string compare (kernel doesn't have strcmp) */
static int perf_strcmp(const char* s1, const char* s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}

/* Exported globals for inline helpers */
uint32_t g_perf_fastpath_flags = 0;
perf_metrics_t g_perf_metrics = {0};

/* Initialize performance subsystem */
status_t perf_init(void) {
    if (perf_state.initialized) {
        return STATUS_EXISTS;
    }

    /* Default configuration: balanced mode */
    perf_state.config.mode = PERF_MODE_BALANCED;
    perf_state.config.fastpath_flags = PERF_FASTPATH_ENABLED |
                                       PERF_FASTPATH_SYSCALL |
                                       PERF_FASTPATH_IPC;
    perf_state.config.lazy_flags = PERF_LAZY_MODULES | PERF_LAZY_DRIVERS;
    perf_state.config.aggressive_idle = false;
    perf_state.config.hardware_accel = true;
    perf_state.config.idle_timeout_ms = 5000;
    perf_state.config.cache_size_kb = 64;

    /* Initialize fast path cache */
    for (int i = 0; i < FASTPATH_CACHE_SIZE; i++) {
        perf_state.cache.entries[i].valid = false;
        perf_state.cache.entries[i].access_count = 0;
    }

    /* Initialize metrics */
    perf_state.metrics = (perf_metrics_t){0};
    g_perf_metrics = perf_state.metrics;

    /* Enable fast paths by default */
    g_perf_fastpath_flags = perf_state.config.fastpath_flags;

    perf_state.module_count = 0;
    perf_state.idle = false;
    perf_state.lock = 0;
    perf_state.initialized = true;

    KLOG_INFO("PERF", "Performance subsystem initialized (mode: BALANCED)");
    return STATUS_OK;
}

/* Set performance mode */
status_t perf_set_mode(perf_mode_t mode) {
    __sync_lock_test_and_set(&perf_state.lock, 1);

    perf_state.config.mode = mode;

    switch (mode) {
        case PERF_MODE_IDLE:
            /* Minimal resource usage */
            perf_state.config.fastpath_flags = 0;
            perf_state.config.lazy_flags = PERF_LAZY_MODULES |
                                           PERF_LAZY_DRIVERS |
                                           PERF_LAZY_SERVICES |
                                           PERF_LAZY_GRAPHICS;
            perf_state.config.aggressive_idle = true;
            perf_state.config.hardware_accel = false;
            break;

        case PERF_MODE_BALANCED:
            /* Default mode */
            perf_state.config.fastpath_flags = PERF_FASTPATH_ENABLED |
                                               PERF_FASTPATH_SYSCALL |
                                               PERF_FASTPATH_IPC;
            perf_state.config.lazy_flags = PERF_LAZY_MODULES | PERF_LAZY_DRIVERS;
            perf_state.config.aggressive_idle = false;
            perf_state.config.hardware_accel = true;
            break;

        case PERF_MODE_PERFORMANCE:
            /* Maximum performance */
            perf_state.config.fastpath_flags = PERF_FASTPATH_ENABLED |
                                               PERF_FASTPATH_SYSCALL |
                                               PERF_FASTPATH_IPC |
                                               PERF_FASTPATH_MEMORY |
                                               PERF_FASTPATH_IO;
            perf_state.config.lazy_flags = 0;  /* Load everything */
            perf_state.config.aggressive_idle = false;
            perf_state.config.hardware_accel = true;
            break;

        case PERF_MODE_POWER_SAVE:
            /* Minimum power consumption */
            perf_state.config.fastpath_flags = PERF_FASTPATH_ENABLED;
            perf_state.config.lazy_flags = PERF_LAZY_MODULES |
                                           PERF_LAZY_DRIVERS |
                                           PERF_LAZY_SERVICES |
                                           PERF_LAZY_GRAPHICS;
            perf_state.config.aggressive_idle = true;
            perf_state.config.hardware_accel = false;
            break;

        default:
            __sync_lock_release(&perf_state.lock);
            return STATUS_INVALID;
    }

    g_perf_fastpath_flags = perf_state.config.fastpath_flags;

    __sync_lock_release(&perf_state.lock);

    KLOG_INFO("PERF", "Performance mode changed to %d", mode);
    return STATUS_OK;
}

/* Get current performance mode */
perf_mode_t perf_get_mode(void) {
    return perf_state.config.mode;
}

/* Enable fast path */
status_t perf_fastpath_enable(uint32_t flags) {
    __sync_lock_test_and_set(&perf_state.lock, 1);
    perf_state.config.fastpath_flags |= flags;
    g_perf_fastpath_flags = perf_state.config.fastpath_flags;
    __sync_lock_release(&perf_state.lock);
    return STATUS_OK;
}

/* Disable fast path */
status_t perf_fastpath_disable(uint32_t flags) {
    __sync_lock_test_and_set(&perf_state.lock, 1);
    perf_state.config.fastpath_flags &= ~flags;
    g_perf_fastpath_flags = perf_state.config.fastpath_flags;
    __sync_lock_release(&perf_state.lock);
    return STATUS_OK;
}

/* Check if fast path is enabled */
bool perf_is_fastpath_enabled(uint32_t flag) {
    return (g_perf_fastpath_flags & flag) != 0;
}

/* Insert entry into fast path cache */
status_t perf_cache_insert(uint64_t key, uint64_t value) {
    uint32_t index = (uint32_t)(key % FASTPATH_CACHE_SIZE);

    /* Evict if occupied */
    if (perf_state.cache.entries[index].valid) {
        perf_state.cache.evictions++;
    }

    perf_state.cache.entries[index].key = key;
    perf_state.cache.entries[index].value = value;
    perf_state.cache.entries[index].timestamp = perf_timestamp_ns();
    perf_state.cache.entries[index].access_count = 1;
    perf_state.cache.entries[index].valid = true;

    return STATUS_OK;
}

/* Lookup entry in fast path cache */
status_t perf_cache_lookup(uint64_t key, uint64_t* out_value) {
    if (!out_value) {
        return STATUS_INVALID;
    }

    uint32_t index = (uint32_t)(key % FASTPATH_CACHE_SIZE);
    fastpath_cache_entry_t* entry = &perf_state.cache.entries[index];

    if (entry->valid && entry->key == key) {
        /* Cache hit */
        *out_value = entry->value;
        entry->access_count++;
        perf_state.cache.hits++;
        return STATUS_OK;
    }

    /* Cache miss */
    perf_state.cache.misses++;
    return STATUS_NOTFOUND;
}

/* Invalidate cache entry */
void perf_cache_invalidate(uint64_t key) {
    uint32_t index = (uint32_t)(key % FASTPATH_CACHE_SIZE);
    perf_state.cache.entries[index].valid = false;
}

/* Clear entire cache */
void perf_cache_clear(void) {
    for (int i = 0; i < FASTPATH_CACHE_SIZE; i++) {
        perf_state.cache.entries[i].valid = false;
    }
}

/* Register lazy-loadable module */
status_t perf_register_lazy_module(lazy_module_t* module) {
    if (!module || !module->name) {
        return STATUS_INVALID;
    }

    __sync_lock_test_and_set(&perf_state.lock, 1);

    if (perf_state.module_count >= 128) {
        __sync_lock_release(&perf_state.lock);
        return STATUS_NOMEM;
    }

    perf_state.modules[perf_state.module_count++] = module;
    module->loaded = false;

    __sync_lock_release(&perf_state.lock);

    KLOG_DEBUG("PERF", "Registered lazy module: %s (priority: %d)",
               module->name, module->priority);
    return STATUS_OK;
}

/* Find module by name */
static lazy_module_t* find_module(const char* name) {
    for (uint32_t i = 0; i < perf_state.module_count; i++) {
        if (perf_strcmp(perf_state.modules[i]->name, name) == 0) {
            return perf_state.modules[i];
        }
    }
    return NULL;
}

/* Load module on-demand */
status_t perf_load_module(const char* name) {
    if (!name) {
        return STATUS_INVALID;
    }

    __sync_lock_test_and_set(&perf_state.lock, 1);

    lazy_module_t* module = find_module(name);
    if (!module) {
        __sync_lock_release(&perf_state.lock);
        return STATUS_NOTFOUND;
    }

    if (module->loaded) {
        __sync_lock_release(&perf_state.lock);
        return STATUS_EXISTS;
    }

    /* Load dependencies first */
    for (uint32_t i = 0; i < module->dependencies_count; i++) {
        const char* dep_name = module->dependencies[i];
        lazy_module_t* dep = find_module(dep_name);
        if (dep && !dep->loaded) {
            __sync_lock_release(&perf_state.lock);
            status_t status = perf_load_module(dep_name);
            if (FAILED(status)) {
                return status;
            }
            __sync_lock_test_and_set(&perf_state.lock, 1);
        }
    }

    /* Call init function */
    status_t status = STATUS_OK;
    if (module->init_fn) {
        status = module->init_fn();
    }

    if (!FAILED(status)) {
        module->loaded = true;
        KLOG_INFO("PERF", "Loaded module: %s", name);
    }

    __sync_lock_release(&perf_state.lock);
    return status;
}

/* Unload module */
status_t perf_unload_module(const char* name) {
    if (!name) {
        return STATUS_INVALID;
    }

    __sync_lock_test_and_set(&perf_state.lock, 1);

    lazy_module_t* module = find_module(name);
    if (!module) {
        __sync_lock_release(&perf_state.lock);
        return STATUS_NOTFOUND;
    }

    if (!module->loaded) {
        __sync_lock_release(&perf_state.lock);
        return STATUS_INVALID;
    }

    /* Call cleanup function */
    if (module->cleanup_fn) {
        module->cleanup_fn();
    }

    module->loaded = false;

    __sync_lock_release(&perf_state.lock);

    KLOG_INFO("PERF", "Unloaded module: %s", name);
    return STATUS_OK;
}

/* Check if module is loaded */
bool perf_is_module_loaded(const char* name) {
    if (!name) {
        return false;
    }

    lazy_module_t* module = find_module(name);
    return module && module->loaded;
}

/* Enter idle mode */
status_t perf_enter_idle(void) {
    if (perf_state.idle) {
        return STATUS_EXISTS;
    }

    perf_state.idle = true;
    perf_state.idle_enter_time = perf_timestamp_ns();

    KLOG_DEBUG("PERF", "Entered idle mode");
    return STATUS_OK;
}

/* Exit idle mode */
status_t perf_exit_idle(void) {
    if (!perf_state.idle) {
        return STATUS_INVALID;
    }

    uint64_t idle_duration = perf_timestamp_ns() - perf_state.idle_enter_time;
    perf_state.idle = false;

    KLOG_DEBUG("PERF", "Exited idle mode (duration: %llu ns)", idle_duration);
    return STATUS_OK;
}

/* Check if system is idle */
bool perf_is_idle(void) {
    return perf_state.idle;
}

/* Enable hardware acceleration */
status_t perf_enable_hardware_accel(void) {
    perf_state.config.hardware_accel = true;
    KLOG_INFO("PERF", "Hardware acceleration enabled");
    return STATUS_OK;
}

/* Disable hardware acceleration */
status_t perf_disable_hardware_accel(void) {
    perf_state.config.hardware_accel = false;
    KLOG_INFO("PERF", "Hardware acceleration disabled");
    return STATUS_OK;
}

/* Get performance metrics */
void perf_get_metrics(perf_metrics_t* metrics) {
    if (!metrics) {
        return;
    }

    __sync_lock_test_and_set(&perf_state.lock, 1);

    *metrics = perf_state.metrics;

    /* Calculate cache hit rate */
    uint64_t total = perf_state.cache.hits + perf_state.cache.misses;
    if (total > 0) {
        metrics->cache_hit_rate = (uint32_t)((perf_state.cache.hits * 100) / total);
    } else {
        metrics->cache_hit_rate = 0;
    }

    metrics->cache_hits = perf_state.cache.hits;
    metrics->cache_misses = perf_state.cache.misses;

    __sync_lock_release(&perf_state.lock);
}

/* Reset metrics */
void perf_reset_metrics(void) {
    __sync_lock_test_and_set(&perf_state.lock, 1);
    perf_state.metrics = (perf_metrics_t){0};
    g_perf_metrics = perf_state.metrics;
    perf_state.cache.hits = 0;
    perf_state.cache.misses = 0;
    perf_state.cache.evictions = 0;
    __sync_lock_release(&perf_state.lock);
}

/* Update CPU utilization */
void perf_update_cpu_utilization(uint32_t utilization) {
    if (utilization > 100) {
        utilization = 100;
    }
    perf_state.metrics.cpu_utilization = utilization;
    g_perf_metrics.cpu_utilization = utilization;
}

/* Update memory pressure */
void perf_update_memory_pressure(uint32_t pressure) {
    if (pressure > 100) {
        pressure = 100;
    }
    perf_state.metrics.memory_pressure = pressure;
    g_perf_metrics.memory_pressure = pressure;
}

/* Get high-resolution timestamp (nanoseconds) */
uint64_t perf_timestamp_ns(void) {
    /* Use TSC (Time Stamp Counter) on x86_64 */
    uint32_t lo, hi;
    __asm__ volatile("rdtsc" : "=a"(lo), "=d"(hi));
    uint64_t tsc = ((uint64_t)hi << 32) | lo;

    /* Convert to nanoseconds (assuming 2.4 GHz CPU) */
    /* In production, this would use calibrated TSC frequency */
    return (tsc * 1000) / 2400;
}

/* Get CPU cycle count */
uint64_t perf_cycles(void) {
    uint32_t lo, hi;
    __asm__ volatile("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | lo;
}
