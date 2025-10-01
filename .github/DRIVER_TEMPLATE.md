# Driver Template for LimitlessOS

This document provides templates for creating user-space drivers in LimitlessOS.

## Generic Driver Template

```c
/**
 * @file driver_name.c
 * @brief [Brief description of the driver]
 * 
 * [Detailed description of what hardware this driver supports]
 * 
 * @author LimitlessOS Contributors
 * @date 2025
 */

#include "kernel.h"
#include "microkernel.h"
#include "hal.h"

/* Driver-specific constants */
#define DRIVER_NAME "driver_name"
#define DRIVER_VERSION "1.0.0"

/* Device-specific registers and structures */
typedef struct driver_device {
    /* Hardware identification */
    uint16_t vendor_id;
    uint16_t device_id;
    uint32_t class_code;
    
    /* Memory-mapped I/O */
    void* mmio_base;
    size_t mmio_size;
    
    /* DMA buffers */
    void* rx_buffer;
    void* tx_buffer;
    size_t buffer_size;
    
    /* State */
    bool initialized;
    bool enabled;
    
    /* Statistics (for AI telemetry) */
    uint64_t interrupts;
    uint64_t errors;
    uint64_t operations;
    
    /* Lock */
    uint32_t lock;
} driver_device_t;

/* Global driver state */
static struct {
    driver_device_t devices[16];
    uint32_t device_count;
    bool initialized;
} driver_state = {0};

/**
 * @brief Initialize the driver subsystem
 * 
 * @return STATUS_OK on success, error code otherwise
 */
status_t driver_init(void) {
    if (driver_state.initialized) {
        return STATUS_EXISTS;
    }
    
    KLOG_INFO(DRIVER_NAME, "Initializing %s driver v%s", DRIVER_NAME, DRIVER_VERSION);
    
    /* Initialize driver state */
    driver_state.device_count = 0;
    driver_state.initialized = true;
    
    KLOG_INFO(DRIVER_NAME, "Driver initialized successfully");
    return STATUS_OK;
}

/**
 * @brief Probe for devices supported by this driver
 * 
 * @param pci_dev PCI device to probe
 * @return STATUS_OK if device is supported
 */
status_t driver_probe(pci_device_t* pci_dev) {
    if (!pci_dev) {
        return STATUS_INVALID;
    }
    
    /* Check if device is supported */
    if (pci_dev->vendor_id != VENDOR_ID || pci_dev->device_id != DEVICE_ID) {
        return STATUS_NOTFOUND;
    }
    
    KLOG_INFO(DRIVER_NAME, "Found supported device: %04x:%04x", 
              pci_dev->vendor_id, pci_dev->device_id);
    
    /* Allocate device structure */
    if (driver_state.device_count >= 16) {
        return STATUS_NOMEM;
    }
    
    driver_device_t* dev = &driver_state.devices[driver_state.device_count++];
    dev->vendor_id = pci_dev->vendor_id;
    dev->device_id = pci_dev->device_id;
    dev->class_code = pci_dev->class_code;
    
    /* Map MMIO registers */
    dev->mmio_base = hal_map_mmio(pci_dev->bar[0], &dev->mmio_size);
    if (!dev->mmio_base) {
        KLOG_ERROR(DRIVER_NAME, "Failed to map MMIO region");
        return STATUS_ERROR;
    }
    
    /* Allocate DMA buffers */
    dev->buffer_size = 4096;
    dev->rx_buffer = pmm_alloc_pages(1);
    dev->tx_buffer = pmm_alloc_pages(1);
    
    if (!dev->rx_buffer || !dev->tx_buffer) {
        KLOG_ERROR(DRIVER_NAME, "Failed to allocate DMA buffers");
        return STATUS_NOMEM;
    }
    
    /* Reset device */
    status_t status = driver_reset_device(dev);
    if (!SUCCESS(status)) {
        KLOG_ERROR(DRIVER_NAME, "Failed to reset device");
        return status;
    }
    
    dev->initialized = true;
    
    KLOG_INFO(DRIVER_NAME, "Device initialized successfully");
    return STATUS_OK;
}

/**
 * @brief Reset the device to a known state
 * 
 * @param dev Device to reset
 * @return STATUS_OK on success
 */
static status_t driver_reset_device(driver_device_t* dev) {
    if (!dev || !dev->mmio_base) {
        return STATUS_INVALID;
    }
    
    /* Device-specific reset sequence */
    // 1. Disable device
    // 2. Wait for pending operations
    // 3. Clear status registers
    // 4. Re-enable device
    
    return STATUS_OK;
}

/**
 * @brief Enable the device
 * 
 * @param dev Device to enable
 * @return STATUS_OK on success
 */
status_t driver_enable(driver_device_t* dev) {
    if (!dev || !dev->initialized) {
        return STATUS_INVALID;
    }
    
    if (dev->enabled) {
        return STATUS_EXISTS;
    }
    
    /* Enable interrupts */
    // TODO: Enable device-specific interrupts
    
    dev->enabled = true;
    
    KLOG_INFO(DRIVER_NAME, "Device enabled");
    return STATUS_OK;
}

/**
 * @brief Disable the device
 * 
 * @param dev Device to disable
 * @return STATUS_OK on success
 */
status_t driver_disable(driver_device_t* dev) {
    if (!dev || !dev->initialized) {
        return STATUS_INVALID;
    }
    
    if (!dev->enabled) {
        return STATUS_OK;
    }
    
    /* Disable interrupts */
    // TODO: Disable device-specific interrupts
    
    dev->enabled = false;
    
    KLOG_INFO(DRIVER_NAME, "Device disabled");
    return STATUS_OK;
}

/**
 * @brief Interrupt handler for the device
 * 
 * @param dev Device that triggered the interrupt
 */
void driver_interrupt_handler(driver_device_t* dev) {
    if (!dev || !dev->enabled) {
        return;
    }
    
    __sync_fetch_and_add(&dev->interrupts, 1);
    
    /* Read interrupt status */
    // TODO: Read device-specific interrupt status
    
    /* Handle interrupt */
    // TODO: Handle device-specific interrupt
    
    /* Clear interrupt */
    // TODO: Clear device-specific interrupt
}

/**
 * @brief Get driver statistics (for AI telemetry)
 * 
 * @param dev Device to query
 * @param stats Output statistics structure
 * @return STATUS_OK on success
 */
status_t driver_get_stats(driver_device_t* dev, driver_stats_t* stats) {
    if (!dev || !stats) {
        return STATUS_INVALID;
    }
    
    stats->interrupts = dev->interrupts;
    stats->errors = dev->errors;
    stats->operations = dev->operations;
    
    return STATUS_OK;
}

/**
 * @brief Cleanup driver resources
 */
void driver_cleanup(void) {
    if (!driver_state.initialized) {
        return;
    }
    
    /* Cleanup all devices */
    for (uint32_t i = 0; i < driver_state.device_count; i++) {
        driver_device_t* dev = &driver_state.devices[i];
        
        if (dev->enabled) {
            driver_disable(dev);
        }
        
        /* Free DMA buffers */
        if (dev->rx_buffer) {
            pmm_free_pages((paddr_t)dev->rx_buffer, 1);
        }
        if (dev->tx_buffer) {
            pmm_free_pages((paddr_t)dev->tx_buffer, 1);
        }
        
        /* Unmap MMIO */
        if (dev->mmio_base) {
            hal_unmap_mmio(dev->mmio_base, dev->mmio_size);
        }
    }
    
    driver_state.initialized = false;
    
    KLOG_INFO(DRIVER_NAME, "Driver cleanup complete");
}
```

## USB Driver Template

```c
/**
 * @file usb_driver.c
 * @brief USB device driver template
 */

#include "kernel.h"
#include "microkernel.h"
#include "hal.h"
#include "usb.h"

/* USB device descriptor */
typedef struct usb_driver_device {
    usb_device_t* usb_dev;
    
    /* Endpoints */
    uint8_t in_endpoint;
    uint8_t out_endpoint;
    uint16_t max_packet_size;
    
    /* Transfer state */
    void* transfer_buffer;
    size_t transfer_size;
    
    /* Statistics */
    uint64_t transfers;
    uint64_t errors;
} usb_driver_device_t;

/**
 * @brief Probe USB device
 */
status_t usb_driver_probe(usb_device_t* usb_dev) {
    if (!usb_dev) {
        return STATUS_INVALID;
    }
    
    /* Check device class/subclass/protocol */
    if (usb_dev->class != USB_CLASS_VENDOR_SPECIFIC) {
        return STATUS_NOTFOUND;
    }
    
    /* Get device descriptor */
    usb_device_descriptor_t desc;
    status_t status = usb_get_descriptor(usb_dev, USB_DESC_DEVICE, 0, &desc, sizeof(desc));
    if (!SUCCESS(status)) {
        return status;
    }
    
    /* Verify vendor/product ID */
    if (desc.vendor_id != VENDOR_ID || desc.product_id != PRODUCT_ID) {
        return STATUS_NOTFOUND;
    }
    
    /* Get configuration descriptor */
    usb_config_descriptor_t config;
    status = usb_get_descriptor(usb_dev, USB_DESC_CONFIG, 0, &config, sizeof(config));
    if (!SUCCESS(status)) {
        return status;
    }
    
    /* Set configuration */
    status = usb_set_configuration(usb_dev, config.configuration_value);
    if (!SUCCESS(status)) {
        return status;
    }
    
    /* Find endpoints */
    // TODO: Parse interface descriptors and find IN/OUT endpoints
    
    KLOG_INFO("USB_DRIVER", "USB device initialized");
    return STATUS_OK;
}

/**
 * @brief Send data to USB device
 */
status_t usb_driver_send(usb_driver_device_t* dev, const void* data, size_t size) {
    if (!dev || !data || size == 0) {
        return STATUS_INVALID;
    }
    
    /* Create transfer */
    usb_transfer_t transfer = {
        .endpoint = dev->out_endpoint,
        .type = USB_TRANSFER_BULK,
        .direction = USB_DIR_OUT,
        .buffer = (void*)data,
        .size = size,
        .timeout_ms = 1000,
    };
    
    /* Submit transfer */
    status_t status = usb_submit_transfer(dev->usb_dev, &transfer);
    if (!SUCCESS(status)) {
        __sync_fetch_and_add(&dev->errors, 1);
        return status;
    }
    
    __sync_fetch_and_add(&dev->transfers, 1);
    return STATUS_OK;
}

/**
 * @brief Receive data from USB device
 */
status_t usb_driver_receive(usb_driver_device_t* dev, void* buffer, size_t* size) {
    if (!dev || !buffer || !size) {
        return STATUS_INVALID;
    }
    
    /* Create transfer */
    usb_transfer_t transfer = {
        .endpoint = dev->in_endpoint,
        .type = USB_TRANSFER_BULK,
        .direction = USB_DIR_IN,
        .buffer = buffer,
        .size = *size,
        .timeout_ms = 1000,
    };
    
    /* Submit transfer */
    status_t status = usb_submit_transfer(dev->usb_dev, &transfer);
    if (!SUCCESS(status)) {
        __sync_fetch_and_add(&dev->errors, 1);
        return status;
    }
    
    *size = transfer.actual_size;
    __sync_fetch_and_add(&dev->transfers, 1);
    return STATUS_OK;
}
```

## Storage Driver Template

```c
/**
 * @file storage_driver.c
 * @brief Block device driver template
 */

#include "kernel.h"
#include "microkernel.h"
#include "hal.h"

/* Block device structure */
typedef struct storage_device {
    const char* name;
    uint64_t capacity_bytes;
    uint32_t block_size;
    bool read_only;
    
    /* Operations */
    status_t (*read_blocks)(struct storage_device* dev, uint64_t lba, void* buffer, size_t count);
    status_t (*write_blocks)(struct storage_device* dev, uint64_t lba, const void* buffer, size_t count);
    status_t (*flush)(struct storage_device* dev);
    
    /* Statistics */
    uint64_t reads;
    uint64_t writes;
    uint64_t errors;
    
    void* private_data;
} storage_device_t;

/**
 * @brief Read blocks from storage device
 */
status_t storage_read_blocks(storage_device_t* dev, uint64_t lba, void* buffer, size_t count) {
    if (!dev || !buffer || count == 0) {
        return STATUS_INVALID;
    }
    
    if (lba + count > (dev->capacity_bytes / dev->block_size)) {
        return STATUS_INVALID;
    }
    
    /* Device-specific read implementation */
    status_t status = dev->read_blocks(dev, lba, buffer, count);
    
    if (SUCCESS(status)) {
        __sync_fetch_and_add(&dev->reads, count);
    } else {
        __sync_fetch_and_add(&dev->errors, 1);
    }
    
    return status;
}

/**
 * @brief Write blocks to storage device
 */
status_t storage_write_blocks(storage_device_t* dev, uint64_t lba, const void* buffer, size_t count) {
    if (!dev || !buffer || count == 0) {
        return STATUS_INVALID;
    }
    
    if (dev->read_only) {
        return STATUS_ERROR;
    }
    
    if (lba + count > (dev->capacity_bytes / dev->block_size)) {
        return STATUS_INVALID;
    }
    
    /* Device-specific write implementation */
    status_t status = dev->write_blocks(dev, lba, buffer, count);
    
    if (SUCCESS(status)) {
        __sync_fetch_and_add(&dev->writes, count);
    } else {
        __sync_fetch_and_add(&dev->errors, 1);
    }
    
    return status;
}

/**
 * @brief Flush pending writes
 */
status_t storage_flush(storage_device_t* dev) {
    if (!dev) {
        return STATUS_INVALID;
    }
    
    if (dev->flush) {
        return dev->flush(dev);
    }
    
    return STATUS_OK;
}
```

## Network Driver Template

```c
/**
 * @file network_driver.c
 * @brief Network interface driver template
 */

#include "kernel.h"
#include "microkernel.h"
#include "hal.h"
#include "net.h"

/* Network device structure */
typedef struct network_device {
    const char* name;
    uint8_t mac_address[6];
    bool link_up;
    uint32_t link_speed;  // Mbps
    
    /* Ring buffers */
    void* rx_ring;
    void* tx_ring;
    uint32_t rx_head;
    uint32_t rx_tail;
    uint32_t tx_head;
    uint32_t tx_tail;
    
    /* Operations */
    status_t (*send_packet)(struct network_device* dev, const void* data, size_t size);
    status_t (*receive_packet)(struct network_device* dev, void* buffer, size_t* size);
    
    /* Statistics */
    uint64_t rx_packets;
    uint64_t tx_packets;
    uint64_t rx_bytes;
    uint64_t tx_bytes;
    uint64_t rx_errors;
    uint64_t tx_errors;
    uint64_t rx_dropped;
    uint64_t tx_dropped;
    
    void* private_data;
} network_device_t;

/**
 * @brief Send network packet
 */
status_t network_send_packet(network_device_t* dev, const void* data, size_t size) {
    if (!dev || !data || size == 0 || size > 1514) {
        return STATUS_INVALID;
    }
    
    if (!dev->link_up) {
        __sync_fetch_and_add(&dev->tx_dropped, 1);
        return STATUS_ERROR;
    }
    
    /* Device-specific send implementation */
    status_t status = dev->send_packet(dev, data, size);
    
    if (SUCCESS(status)) {
        __sync_fetch_and_add(&dev->tx_packets, 1);
        __sync_fetch_and_add(&dev->tx_bytes, size);
    } else {
        __sync_fetch_and_add(&dev->tx_errors, 1);
    }
    
    return status;
}

/**
 * @brief Receive network packet
 */
status_t network_receive_packet(network_device_t* dev, void* buffer, size_t* size) {
    if (!dev || !buffer || !size) {
        return STATUS_INVALID;
    }
    
    if (!dev->link_up) {
        return STATUS_ERROR;
    }
    
    /* Device-specific receive implementation */
    status_t status = dev->receive_packet(dev, buffer, size);
    
    if (SUCCESS(status)) {
        __sync_fetch_and_add(&dev->rx_packets, 1);
        __sync_fetch_and_add(&dev->rx_bytes, *size);
    } else {
        __sync_fetch_and_add(&dev->rx_errors, 1);
    }
    
    return status;
}

/**
 * @brief Check link status
 */
bool network_is_link_up(network_device_t* dev) {
    return dev && dev->link_up;
}
```

## Makefile Template for Drivers

```makefile
# Driver Makefile Template

DRIVER_NAME := my_driver
CC := clang
CFLAGS := -Wall -Wextra -Werror -std=c17 -O2 -g -fPIC
CFLAGS += -I../../kernel/include -I../../hal/include

SOURCES := $(wildcard *.c)
OBJECTS := $(SOURCES:.c=.o)
TARGET := lib$(DRIVER_NAME).so

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) -shared -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJECTS) $(TARGET)

install: $(TARGET)
	cp $(TARGET) /usr/lib/limitlessos/drivers/

.PHONY: all clean install
```

## Testing Template

```c
/**
 * @file test_driver.c
 * @brief Unit tests for driver
 */

#include "kernel.h"
#include "driver.h"

void test_driver_init(void) {
    status_t status = driver_init();
    assert(status == STATUS_OK);
    
    /* Test double init */
    status = driver_init();
    assert(status == STATUS_EXISTS);
}

void test_driver_probe(void) {
    pci_device_t pci_dev = {
        .vendor_id = VENDOR_ID,
        .device_id = DEVICE_ID,
    };
    
    status_t status = driver_probe(&pci_dev);
    assert(status == STATUS_OK);
}

void test_driver_operation(void) {
    /* Test driver-specific operations */
    // TODO: Add device-specific tests
}

int main(void) {
    test_driver_init();
    test_driver_probe();
    test_driver_operation();
    
    printf("All tests passed!\n");
    return 0;
}
```

---

## Usage

1. Copy the appropriate template
2. Replace placeholders with actual values
3. Implement device-specific operations
4. Add proper error handling
5. Add telemetry hooks for AI integration
6. Write tests
7. Add documentation

## Best Practices

- Always validate input parameters
- Use proper locking for shared resources
- Track statistics for AI telemetry
- Handle errors gracefully
- Clean up resources on failure
- Test thoroughly with real hardware
