/*
 * Timer Subsystem
 * Full implementation with PIT, TSC, and callback support
 */

#include "hal.h"

/* PIT (Programmable Interval Timer) constants */
#define PIT_FREQUENCY 1193182
#define PIT_CHANNEL0 0x40
#define PIT_CHANNEL1 0x41
#define PIT_CHANNEL2 0x42
#define PIT_COMMAND 0x43

/* PIT command modes */
#define PIT_CMD_BINARY 0x00
#define PIT_CMD_MODE2 0x04    /* Rate generator */
#define PIT_CMD_MODE3 0x06    /* Square wave */
#define PIT_CMD_RW_BOTH 0x30  /* Read/Write LSB then MSB */
#define PIT_CMD_CHANNEL0 0x00

/* Maximum timers */
#define MAX_TIMERS 64

/* Timer entry */
typedef struct timer_entry {
    bool active;
    bool periodic;
    uint64_t interval_ns;
    uint64_t next_fire_tick;
    timer_callback_t callback;
    void* context;
} timer_entry_t;

static uint64_t timer_ticks = 0;
static uint64_t timer_frequency = 1000;  // 1000 Hz = 1ms resolution
static bool timer_initialized = false;

/* Timer callback table */
static timer_entry_t timers[MAX_TIMERS];
static uint32_t timer_count = 0;

/* I/O port access - use HAL functions from header */

/* Configure PIT frequency */
static void pit_set_frequency(uint32_t hz) {
    if (hz < 19 || hz > PIT_FREQUENCY) {
        hz = 1000;  /* Default to 1000 Hz */
    }

    uint32_t divisor = PIT_FREQUENCY / hz;

    /* Send command */
    outb(PIT_COMMAND, PIT_CMD_CHANNEL0 | PIT_CMD_RW_BOTH | PIT_CMD_MODE2 | PIT_CMD_BINARY);

    /* Send frequency divisor (LSB then MSB) */
    outb(PIT_CHANNEL0, (uint8_t)(divisor & 0xFF));
    outb(PIT_CHANNEL0, (uint8_t)((divisor >> 8) & 0xFF));
}

/* Read TSC (Time Stamp Counter) */
static inline uint64_t read_tsc(void) {
    uint32_t lo, hi;
    __asm__ volatile("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | lo;
}

/* Initialize timer subsystem */
status_t hal_timer_init(void) {
    if (timer_initialized) {
        return STATUS_EXISTS;
    }

    /* Initialize timer state */
    timer_ticks = 0;
    timer_frequency = 1000;  /* 1000 Hz = 1ms per tick */
    timer_count = 0;

    for (uint32_t i = 0; i < MAX_TIMERS; i++) {
        timers[i].active = false;
    }

    /* Configure PIT to desired frequency */
    pit_set_frequency(1000);  /* 1000 Hz */

    timer_initialized = true;

    return STATUS_OK;
}

/* Timer interrupt handler (called by IRQ handler) */
void hal_timer_tick(void) {
    timer_ticks++;

    /* Process timers */
    for (uint32_t i = 0; i < MAX_TIMERS; i++) {
        timer_entry_t* timer = &timers[i];

        if (!timer->active) {
            continue;
        }

        if (timer_ticks >= timer->next_fire_tick) {
            /* Timer fired - call callback */
            if (timer->callback) {
                timer->callback(timer->context);
            }

            if (timer->periodic) {
                /* Reschedule periodic timer */
                uint64_t interval_ticks = hal_timer_ns_to_ticks(timer->interval_ns);
                timer->next_fire_tick = timer_ticks + interval_ticks;
            } else {
                /* One-shot timer - deactivate */
                timer->active = false;
            }
        }
    }
}

/* Get current tick count */
uint64_t hal_timer_get_ticks(void) {
    return timer_ticks;
}

/* Get timer frequency */
uint64_t hal_timer_get_frequency(void) {
    return timer_frequency;
}

/* Convert ticks to nanoseconds */
uint64_t hal_timer_ticks_to_ns(uint64_t ticks) {
    return (ticks * 1000000000ULL) / timer_frequency;
}

/* Convert nanoseconds to ticks */
uint64_t hal_timer_ns_to_ticks(uint64_t ns) {
    return (ns * timer_frequency) / 1000000000ULL;
}

/* Find free timer slot */
static int32_t find_free_timer(void) {
    for (uint32_t i = 0; i < MAX_TIMERS; i++) {
        if (!timers[i].active) {
            return (int32_t)i;
        }
    }
    return -1;
}

/* Setup one-shot timer */
status_t hal_timer_oneshot(uint64_t ns, timer_callback_t callback, void* context) {
    if (!callback) {
        return STATUS_INVALID;
    }

    int32_t slot = find_free_timer();
    if (slot < 0) {
        return STATUS_NOMEM;
    }

    timer_entry_t* timer = &timers[slot];
    timer->active = true;
    timer->periodic = false;
    timer->interval_ns = ns;
    timer->callback = callback;
    timer->context = context;

    /* Calculate when timer should fire */
    uint64_t interval_ticks = hal_timer_ns_to_ticks(ns);
    timer->next_fire_tick = timer_ticks + interval_ticks;

    return STATUS_OK;
}

/* Setup periodic timer */
status_t hal_timer_periodic(uint64_t ns, timer_callback_t callback, void* context) {
    if (!callback) {
        return STATUS_INVALID;
    }

    int32_t slot = find_free_timer();
    if (slot < 0) {
        return STATUS_NOMEM;
    }

    timer_entry_t* timer = &timers[slot];
    timer->active = true;
    timer->periodic = true;
    timer->interval_ns = ns;
    timer->callback = callback;
    timer->context = context;

    /* Calculate when timer should fire */
    uint64_t interval_ticks = hal_timer_ns_to_ticks(ns);
    timer->next_fire_tick = timer_ticks + interval_ticks;

    return STATUS_OK;
}

/* Sleep for nanoseconds (busy wait using TSC) */
void hal_timer_sleep_ns(uint64_t ns) {
    if (ns == 0) {
        return;
    }

    /* Use TSC for high-resolution timing */
    uint64_t start = read_tsc();

    /* Estimate TSC frequency (assumes 2.4 GHz CPU) */
    /* In production, would calibrate TSC frequency */
    uint64_t tsc_frequency = 2400000000ULL;  /* 2.4 GHz */

    uint64_t tsc_delta = (ns * tsc_frequency) / 1000000000ULL;
    uint64_t target = start + tsc_delta;

    while (read_tsc() < target) {
        __asm__ volatile("pause");  /* Hint to CPU that we're spinning */
    }
}

/* Sleep for microseconds */
void hal_timer_sleep_us(uint64_t us) {
    hal_timer_sleep_ns(us * 1000);
}

/* Sleep for milliseconds */
void hal_timer_sleep_ms(uint64_t ms) {
    hal_timer_sleep_ns(ms * 1000000);
}

/* Get high-resolution timestamp */
uint64_t hal_timer_get_timestamp_ns(void) {
    /* Use TSC for high-resolution timing */
    uint64_t tsc = read_tsc();

    /* Convert TSC to nanoseconds (assumes 2.4 GHz) */
    /* In production, would use calibrated TSC frequency */
    return (tsc * 1000) / 2400;
}

/* Cancel all timers */
void hal_timer_cancel_all(void) {
    for (uint32_t i = 0; i < MAX_TIMERS; i++) {
        timers[i].active = false;
    }
}
