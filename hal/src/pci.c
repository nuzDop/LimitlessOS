/*
 * PCI Bus Enumeration
 * Full implementation with device discovery and configuration
 */

#include "hal.h"

/* PCI Configuration Space Access */
#define PCI_CONFIG_ADDRESS 0xCF8
#define PCI_CONFIG_DATA    0xCFC

/* PCI Configuration Space Registers */
#define PCI_VENDOR_ID      0x00
#define PCI_DEVICE_ID      0x02
#define PCI_COMMAND        0x04
#define PCI_STATUS         0x06
#define PCI_REVISION_ID    0x08
#define PCI_CLASS_CODE     0x09
#define PCI_SUBCLASS       0x0A
#define PCI_PROG_IF        0x0B
#define PCI_HEADER_TYPE    0x0E
#define PCI_BAR0           0x10
#define PCI_BAR1           0x14
#define PCI_BAR2           0x18
#define PCI_BAR3           0x1C
#define PCI_BAR4           0x20
#define PCI_BAR5           0x24
#define PCI_INTERRUPT_LINE 0x3C
#define PCI_INTERRUPT_PIN  0x3D

/* Maximum devices */
#define MAX_PCI_DEVICES 256

/* PCI device database */
static pci_device_t pci_devices[MAX_PCI_DEVICES];
static uint32_t pci_device_count = 0;
static bool pci_initialized = false;

/* I/O port access - use HAL functions from header */

/* Read PCI configuration space */
static uint32_t pci_read_config(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    uint32_t address = (uint32_t)(
        (1 << 31) |
        ((uint32_t)bus << 16) |
        ((uint32_t)slot << 11) |
        ((uint32_t)func << 8) |
        (offset & 0xFC)
    );

    outl(PCI_CONFIG_ADDRESS, address);
    return inl(PCI_CONFIG_DATA);
}

/* Write PCI configuration space */
static void pci_write_config(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint32_t value) {
    uint32_t address = (uint32_t)(
        (1 << 31) |
        ((uint32_t)bus << 16) |
        ((uint32_t)slot << 11) |
        ((uint32_t)func << 8) |
        (offset & 0xFC)
    );

    outl(PCI_CONFIG_ADDRESS, address);
    outl(PCI_CONFIG_DATA, value);
}

/* Read 16-bit value from config space */
static uint16_t pci_read_config16(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    uint32_t value = pci_read_config(bus, slot, func, offset & 0xFC);
    return (uint16_t)((value >> ((offset & 2) * 8)) & 0xFFFF);
}

/* Read 8-bit value from config space */
static uint8_t pci_read_config8(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    uint32_t value = pci_read_config(bus, slot, func, offset & 0xFC);
    return (uint8_t)((value >> ((offset & 3) * 8)) & 0xFF);
}

/* Check if device exists */
static bool pci_device_exists(uint8_t bus, uint8_t slot, uint8_t func) {
    uint16_t vendor = pci_read_config16(bus, slot, func, PCI_VENDOR_ID);
    return vendor != 0xFFFF;
}

/* Scan single function */
static void pci_scan_function(uint8_t bus, uint8_t slot, uint8_t func) {
    if (!pci_device_exists(bus, slot, func)) {
        return;
    }

    if (pci_device_count >= MAX_PCI_DEVICES) {
        return;
    }

    pci_device_t* dev = &pci_devices[pci_device_count++];

    /* Read device identification */
    dev->vendor_id = pci_read_config16(bus, slot, func, PCI_VENDOR_ID);
    dev->device_id = pci_read_config16(bus, slot, func, PCI_DEVICE_ID);
    dev->class_code = pci_read_config8(bus, slot, func, PCI_CLASS_CODE);
    dev->subclass = pci_read_config8(bus, slot, func, PCI_SUBCLASS);
    dev->prog_if = pci_read_config8(bus, slot, func, PCI_PROG_IF);
    dev->revision = pci_read_config8(bus, slot, func, PCI_REVISION_ID);
    dev->interrupt_line = pci_read_config8(bus, slot, func, PCI_INTERRUPT_LINE);

    /* Store bus/device/function */
    dev->bus = bus;
    dev->device = slot;
    dev->function = func;

    /* Read BARs */
    for (int i = 0; i < 6; i++) {
        dev->bar[i] = pci_read_config(bus, slot, func, PCI_BAR0 + (i * 4));
    }
}

/* Scan single slot */
static void pci_scan_slot(uint8_t bus, uint8_t slot) {
    if (!pci_device_exists(bus, slot, 0)) {
        return;
    }

    /* Scan function 0 */
    pci_scan_function(bus, slot, 0);

    /* Check if multi-function device */
    uint8_t header_type = pci_read_config8(bus, slot, 0, PCI_HEADER_TYPE);
    if (header_type & 0x80) {
        /* Multi-function device, scan functions 1-7 */
        for (uint8_t func = 1; func < 8; func++) {
            pci_scan_function(bus, slot, func);
        }
    }
}

/* Scan single bus */
static void pci_scan_bus(uint8_t bus) {
    for (uint8_t slot = 0; slot < 32; slot++) {
        pci_scan_slot(bus, slot);
    }
}

/* Initialize PCI subsystem */
status_t hal_pci_init(void) {
    if (pci_initialized) {
        return STATUS_EXISTS;
    }

    pci_device_count = 0;

    /* Scan all buses */
    for (uint16_t bus = 0; bus < 256; bus++) {
        pci_scan_bus((uint8_t)bus);
    }

    pci_initialized = true;

    /* Log discovered devices */
    for (uint32_t i = 0; i < pci_device_count; i++) {
        pci_device_t* dev = &pci_devices[i];
        (void)dev; /* In production, would log device info */
    }

    return STATUS_OK;
}

/* Get device count */
uint32_t hal_pci_get_device_count(void) {
    return pci_device_count;
}

/* Get device by index */
status_t hal_pci_get_device(uint32_t index, pci_device_t* device) {
    if (!device || index >= pci_device_count) {
        return STATUS_INVALID;
    }

    *device = pci_devices[index];
    return STATUS_OK;
}

/* Find device by vendor/device ID */
status_t hal_pci_find_device(uint16_t vendor_id, uint16_t device_id, pci_device_t* device) {
    if (!device) {
        return STATUS_INVALID;
    }

    for (uint32_t i = 0; i < pci_device_count; i++) {
        if (pci_devices[i].vendor_id == vendor_id &&
            pci_devices[i].device_id == device_id) {
            *device = pci_devices[i];
            return STATUS_OK;
        }
    }

    return STATUS_NOTFOUND;
}

/* Enable PCI device */
status_t hal_pci_enable_device(pci_device_t* device) {
    if (!device) {
        return STATUS_INVALID;
    }

    /* Read current command register */
    uint32_t command = pci_read_config(device->bus, device->device, device->function, PCI_COMMAND);

    /* Enable I/O space, memory space, and bus mastering */
    command |= 0x07;

    /* Write back */
    pci_write_config(device->bus, device->device, device->function, PCI_COMMAND, command);

    return STATUS_OK;
}

/* Read device BAR size */
uint64_t hal_pci_get_bar_size(pci_device_t* device, uint8_t bar_index) {
    if (!device || bar_index >= 6) {
        return 0;
    }

    uint8_t offset = PCI_BAR0 + (bar_index * 4);

    /* Save original value */
    uint32_t original = pci_read_config(device->bus, device->device, device->function, offset);

    /* Write all 1s */
    pci_write_config(device->bus, device->device, device->function, offset, 0xFFFFFFFF);

    /* Read back */
    uint32_t size_mask = pci_read_config(device->bus, device->device, device->function, offset);

    /* Restore original */
    pci_write_config(device->bus, device->device, device->function, offset, original);

    /* Calculate size */
    if (size_mask == 0) {
        return 0;
    }

    /* Check if memory or I/O BAR */
    if (original & 0x1) {
        /* I/O BAR */
        size_mask &= 0xFFFFFFFC;
    } else {
        /* Memory BAR */
        size_mask &= 0xFFFFFFF0;
    }

    return ~size_mask + 1;
}
