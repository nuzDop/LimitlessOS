/*
 * x86_64 CPU Management
 */

#include "hal.h"
#include <stdint.h>
#include <stdbool.h>

static uint32_t detected_cpu_count = 1;
static cpu_info_t cpu_info_cache = {0};

/* CPUID instruction wrapper */
static inline void cpuid(uint32_t leaf, uint32_t* eax, uint32_t* ebx, uint32_t* ecx, uint32_t* edx) {
    __asm__ volatile("cpuid"
                     : "=a"(*eax), "=b"(*ebx), "=c"(*ecx), "=d"(*edx)
                     : "a"(leaf), "c"(0));
}

/* Read timestamp counter */
static inline uint64_t rdtsc(void) {
    uint32_t low, high;
    __asm__ volatile("rdtsc" : "=a"(low), "=d"(high));
    return ((uint64_t)high << 32) | low;
}

/* Detect CPU features */
static void detect_cpu_features(cpu_info_t* info) {
    uint32_t eax, ebx, ecx, edx;

    /* Get vendor string */
    cpuid(0, &eax, &ebx, &ecx, &edx);
    *(uint32_t*)(info->vendor + 0) = ebx;
    *(uint32_t*)(info->vendor + 4) = edx;
    *(uint32_t*)(info->vendor + 8) = ecx;
    info->vendor[12] = '\0';

    /* Get processor info and features */
    cpuid(1, &eax, &ebx, &ecx, &edx);

    info->stepping = eax & 0xF;
    info->model_id = (eax >> 4) & 0xF;
    info->family = (eax >> 8) & 0xF;

    /* Extended model and family */
    if (info->family == 0xF) {
        info->family += (eax >> 20) & 0xFF;
    }
    if (info->family == 0x6 || info->family == 0xF) {
        info->model_id += ((eax >> 16) & 0xF) << 4;
    }

    /* Feature flags */
    info->has_sse = (edx & (1 << 25)) != 0;
    info->has_sse2 = (edx & (1 << 26)) != 0;
    info->has_avx = (ecx & (1 << 28)) != 0;

    /* Check extended features */
    cpuid(7, &eax, &ebx, &ecx, &edx);
    info->has_avx2 = (ebx & (1 << 5)) != 0;
    info->has_avx512 = (ebx & (1 << 16)) != 0;

    /* Get processor brand string */
    cpuid(0x80000000, &eax, &ebx, &ecx, &edx);
    if (eax >= 0x80000004) {
        uint32_t* brand = (uint32_t*)info->model;
        cpuid(0x80000002, &brand[0], &brand[1], &brand[2], &brand[3]);
        cpuid(0x80000003, &brand[4], &brand[5], &brand[6], &brand[7]);
        cpuid(0x80000004, &brand[8], &brand[9], &brand[10], &brand[11]);
        info->model[63] = '\0';

        /* Trim leading spaces */
        char* p = info->model;
        while (*p == ' ') p++;
        if (p != info->model) {
            int i = 0;
            while (p[i]) {
                info->model[i] = p[i];
                i++;
            }
            info->model[i] = '\0';
        }
    }

    /* CPU topology - simplified for now */
    info->core_count = 1;
    info->thread_count = 1;

    /* Estimate frequency (simplified - read from MSR in production) */
    info->frequency_mhz = 2400;  // Default estimate
}

/* Initialize CPU subsystem */
status_t hal_cpu_init(void) {
    /* Detect primary CPU */
    detect_cpu_features(&cpu_info_cache);
    cpu_info_cache.id = 0;

    /* Detect number of CPUs (simplified - use ACPI/MP tables in production) */
    detected_cpu_count = 1;

    return STATUS_OK;
}

/* Get number of CPUs */
uint32_t hal_cpu_count(void) {
    return detected_cpu_count;
}

/* Get CPU information */
status_t hal_cpu_info(uint32_t cpu_id, cpu_info_t* info) {
    if (!info || cpu_id >= detected_cpu_count) {
        return STATUS_INVALID;
    }

    *info = cpu_info_cache;
    info->id = cpu_id;

    return STATUS_OK;
}

/* Enable interrupts */
void hal_cpu_enable_interrupts(void) {
    __asm__ volatile("sti");
}

/* Disable interrupts */
void hal_cpu_disable_interrupts(void) {
    __asm__ volatile("cli");
}

/* Halt CPU */
void hal_cpu_halt(void) {
    __asm__ volatile("hlt");
}

/* Read timestamp counter */
uint64_t hal_cpu_read_timestamp(void) {
    return rdtsc();
}
