/*
 * Hardware Abstraction Layer - Core
 */

#include "hal.h"

static bool hal_initialized = false;

/* Initialize HAL and all subsystems */
status_t hal_init(void) {
    if (hal_initialized) {
        return STATUS_EXISTS;
    }

    status_t status;

    /* Initialize CPU management */
    status = hal_cpu_init();
    if (FAILED(status)) {
        return status;
    }

    /* Initialize timer */
    status = hal_timer_init();
    if (FAILED(status)) {
        return status;
    }

    /* Initialize PCI bus */
    status = hal_pci_init();
    if (FAILED(status)) {
        return status;
    }

    /* Initialize storage */
    status = hal_storage_init();
    if (FAILED(status)) {
        /* Non-fatal - system can boot without storage */
    }

    /* Initialize network */
    status = hal_network_init();
    if (FAILED(status)) {
        /* Non-fatal - system can boot without network */
    }

    /* Initialize display */
    status = hal_display_init();
    if (FAILED(status)) {
        /* Non-fatal - we have console */
    }

    /* Initialize audio */
    status = hal_audio_init();
    if (FAILED(status)) {
        /* Non-fatal */
    }

    /* Initialize input */
    status = hal_input_init();
    if (FAILED(status)) {
        /* Non-fatal */
    }

    hal_initialized = true;
    return STATUS_OK;
}

/* Shutdown HAL */
status_t hal_shutdown(void) {
    if (!hal_initialized) {
        return STATUS_ERROR;
    }

    /* Shutdown all subsystems in reverse order */
    hal_initialized = false;
    return STATUS_OK;
}
