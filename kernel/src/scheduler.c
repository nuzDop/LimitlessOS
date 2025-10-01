/*
 * Process and Thread Scheduler
 * Priority-based preemptive scheduler with SMP support
 */

#include "kernel.h"
#include "microkernel.h"
#include "process.h"

/* Global scheduler state */
static struct {
    bool initialized;
    sched_config_t config;
    struct list_head ready_queues[5];  // One per priority level
    struct list_head all_threads;
    thread_t* current_thread;
    uint64_t total_context_switches;
    uint32_t lock;
} scheduler = {0};

/* Simple spinlock operations */
static ALWAYS_INLINE void sched_lock(void) {
    __sync_lock_test_and_set(&scheduler.lock, 1);
}

static ALWAYS_INLINE void sched_unlock(void) {
    __sync_lock_release(&scheduler.lock);
}

/* Initialize scheduler */
status_t sched_init(sched_config_t* config) {
    if (scheduler.initialized) {
        return STATUS_EXISTS;
    }

    if (!config) {
        return STATUS_INVALID;
    }

    scheduler.config = *config;

    /* Initialize ready queues */
    for (int i = 0; i < 5; i++) {
        list_init(&scheduler.ready_queues[i]);
    }

    list_init(&scheduler.all_threads);
    scheduler.current_thread = NULL;
    scheduler.total_context_switches = 0;
    scheduler.initialized = true;

    return STATUS_OK;
}

/* Get current running thread */
thread_t* sched_get_current_thread(void) {
    return scheduler.current_thread;
}

/* Add thread to ready queue */
void sched_add_thread(thread_t* thread) {
    if (!thread) {
        return;
    }

    sched_lock();

    if (thread->priority > PRIORITY_REALTIME) {
        thread->priority = PRIORITY_REALTIME;
    }

    thread->state = PROC_STATE_READY;
    list_add(&thread->list_node, &scheduler.ready_queues[thread->priority]);

    sched_unlock();
}

/* Remove thread from ready queue */
void sched_remove_thread(thread_t* thread) {
    if (!thread) {
        return;
    }

    sched_lock();

    list_del(&thread->list_node);
    thread->state = PROC_STATE_BLOCKED;

    sched_unlock();
}

/* Pick next thread to run */
static thread_t* pick_next_thread(void) {
    /* Priority-based selection: check from highest to lowest priority */
    for (int prio = PRIORITY_REALTIME; prio >= PRIORITY_IDLE; prio--) {
        if (!list_empty(&scheduler.ready_queues[prio])) {
            struct list_head* node = scheduler.ready_queues[prio].next;
            thread_t* thread = list_entry(node, thread_t, list_node);

            /* Remove from ready queue */
            list_del(node);

            return thread;
        }
    }

    return NULL;  // Idle thread
}

/* Context switch (architecture-specific - will be implemented in asm) */
extern void context_switch(struct cpu_context* old_ctx, struct cpu_context* new_ctx);

/* Schedule next thread */
void sched_schedule(void) {
    if (!scheduler.initialized) {
        return;
    }

    sched_lock();

    thread_t* prev = scheduler.current_thread;
    thread_t* next = pick_next_thread();

    if (!next) {
        /* No runnable threads - stay in current or idle */
        sched_unlock();
        return;
    }

    if (prev == next) {
        /* Same thread - just return to ready queue */
        if (next->state == PROC_STATE_READY) {
            list_add(&next->list_node, &scheduler.ready_queues[next->priority]);
        }
        sched_unlock();
        return;
    }

    /* Update states */
    if (prev) {
        if (prev->state == PROC_STATE_RUNNING) {
            prev->state = PROC_STATE_READY;
            /* Add back to ready queue if still runnable */
            list_add(&prev->list_node, &scheduler.ready_queues[prev->priority]);
        }
    }

    next->state = PROC_STATE_RUNNING;
    scheduler.current_thread = next;
    scheduler.total_context_switches++;

    sched_unlock();

    /* Perform context switch */
    if (prev && prev->context && next->context) {
        context_switch(prev->context, next->context);
    }
}

/* Yield CPU to another thread */
void sched_yield(void) {
    if (!scheduler.initialized) {
        return;
    }

    thread_t* current = scheduler.current_thread;
    if (current) {
        current->state = PROC_STATE_READY;
    }

    sched_schedule();
}

/* Thread sleep (simplified - no timer support yet) */
status_t thread_sleep(uint64_t nanoseconds) {
    if (!scheduler.initialized) {
        return STATUS_ERROR;
    }

    thread_t* current = scheduler.current_thread;
    if (!current) {
        return STATUS_ERROR;
    }

    sched_lock();
    current->state = PROC_STATE_BLOCKED;
    sched_unlock();

    /* In real implementation, would set timer and reschedule */
    /* For now, just yield */
    sched_schedule();

    return STATUS_OK;
}

/* Thread yield syscall */
status_t thread_yield(void) {
    sched_yield();
    return STATUS_OK;
}

/* Get scheduler statistics */
void sched_get_stats(uint64_t* context_switches, size_t* thread_count) {
    if (context_switches) {
        *context_switches = scheduler.total_context_switches;
    }

    if (thread_count) {
        size_t count = 0;
        struct list_head* pos;
        list_for_each(pos, &scheduler.all_threads) {
            count++;
        }
        *thread_count = count;
    }
}
