/*
 * ATA/AHCI Storage Driver Implementation
 * PIO mode with LBA28/LBA48 support
 */

#include "kernel.h"
#include "microkernel.h"
#include "ata.h"
#include "hal.h"

/* Global ATA driver state */
static ata_driver_t ata_driver = {0};

/* Statistics */
static ata_stats_t ata_stats = {0};

/* I/O port operations (will use HAL) */
static inline uint8_t ata_inb(uint16_t port) {
    uint8_t val;
    __asm__ volatile("inb %1, %0" : "=a"(val) : "Nd"(port));
    return val;
}

static inline void ata_outb(uint16_t port, uint8_t val) {
    __asm__ volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint16_t ata_inw(uint16_t port) {
    uint16_t val;
    __asm__ volatile("inw %1, %0" : "=a"(val) : "Nd"(port));
    return val;
}

static inline void ata_outw(uint16_t port, uint16_t val) {
    __asm__ volatile("outw %0, %1" : : "a"(val), "Nd"(port));
}

/* String utilities */
static void ata_string_copy(char* dest, const char* src, size_t len) {
    for (size_t i = 0; i < len; i++) {
        dest[i] = src[i];
    }
}

static void ata_swap_string(char* str, size_t len) {
    for (size_t i = 0; i < len; i += 2) {
        char tmp = str[i];
        str[i] = str[i + 1];
        str[i + 1] = tmp;
    }
}

static void ata_trim_string(char* str, size_t len) {
    /* Remove trailing spaces */
    for (int i = len - 1; i >= 0; i--) {
        if (str[i] == ' ') {
            str[i] = '\0';
        } else {
            break;
        }
    }
}

/* 400ns delay (read alternate status register 4 times) */
void ata_delay_400ns(ata_device_t* dev) {
    for (int i = 0; i < 4; i++) {
        ata_inb(dev->ctrl_base);
    }
}

/* Read status register */
uint8_t ata_read_status(ata_device_t* dev) {
    return ata_inb(dev->io_base + ATA_REG_STATUS);
}

/* Wait for device ready */
status_t ata_wait_ready(ata_device_t* dev, uint32_t timeout_ms) {
    uint32_t timeout = timeout_ms * 1000;  // Convert to iterations

    while (timeout--) {
        uint8_t status = ata_read_status(dev);

        if (!(status & ATA_SR_BSY) && (status & ATA_SR_DRDY)) {
            return STATUS_OK;
        }

        if (status & ATA_SR_ERR) {
            return STATUS_ERROR;
        }

        /* Small delay */
        for (volatile int i = 0; i < 100; i++);
    }

    return STATUS_TIMEOUT;
}

/* Wait for DRQ (Data Request) */
status_t ata_wait_drq(ata_device_t* dev, uint32_t timeout_ms) {
    uint32_t timeout = timeout_ms * 1000;

    while (timeout--) {
        uint8_t status = ata_read_status(dev);

        if (status & ATA_SR_DRQ) {
            return STATUS_OK;
        }

        if (status & ATA_SR_ERR) {
            return STATUS_ERROR;
        }

        /* Small delay */
        for (volatile int i = 0; i < 100; i++);
    }

    return STATUS_TIMEOUT;
}

/* Identify ATA device */
status_t ata_identify_device(ata_device_t* dev) {
    /* Select drive */
    ata_outb(dev->io_base + ATA_REG_HDDEVSEL, 0xA0 | (dev->drive << 4));
    ata_delay_400ns(dev);

    /* Send IDENTIFY command */
    ata_outb(dev->io_base + ATA_REG_COMMAND, ATA_CMD_IDENTIFY);
    ata_delay_400ns(dev);

    /* Check if device exists */
    uint8_t status = ata_read_status(dev);
    if (status == 0) {
        return STATUS_NOTFOUND;
    }

    /* Wait for BSY to clear */
    status_t result = ata_wait_ready(dev, 1000);
    if (FAILED(result)) {
        return result;
    }

    /* Check for ATAPI device */
    uint8_t cl = ata_inb(dev->io_base + ATA_REG_LBA1);
    uint8_t ch = ata_inb(dev->io_base + ATA_REG_LBA2);

    if (cl == 0x14 && ch == 0xEB) {
        dev->type = ATA_DEV_ATAPI;
        return STATUS_NOSUPPORT;  // ATAPI not supported yet
    } else if (cl == 0x69 && ch == 0x96) {
        dev->type = ATA_DEV_ATAPI;
        return STATUS_NOSUPPORT;
    } else if (cl != 0 || ch != 0) {
        return STATUS_INVALID;
    }

    dev->type = ATA_DEV_ATA;

    /* Wait for DRQ */
    result = ata_wait_drq(dev, 1000);
    if (FAILED(result)) {
        return result;
    }

    /* Read identify data */
    uint16_t* data = (uint16_t*)&dev->identify;
    for (int i = 0; i < 256; i++) {
        data[i] = ata_inw(dev->io_base + ATA_REG_DATA);
    }

    /* Extract device information */
    ata_string_copy(dev->model, (char*)dev->identify.model, 40);
    ata_swap_string(dev->model, 40);
    ata_trim_string(dev->model, 40);
    dev->model[40] = '\0';

    ata_string_copy(dev->serial, (char*)dev->identify.serial, 20);
    ata_swap_string(dev->serial, 20);
    ata_trim_string(dev->serial, 20);
    dev->serial[20] = '\0';

    ata_string_copy(dev->firmware, (char*)dev->identify.firmware, 8);
    ata_swap_string(dev->firmware, 8);
    ata_trim_string(dev->firmware, 8);
    dev->firmware[8] = '\0';

    /* Determine sector count */
    if (dev->identify.lba48_sectors > 0) {
        dev->sectors = dev->identify.lba48_sectors;
        dev->supports_lba48 = true;
    } else {
        dev->sectors = dev->identify.lba28_sectors;
        dev->supports_lba48 = false;
    }

    dev->size_bytes = dev->sectors * ATA_SECTOR_SIZE;
    dev->supports_dma = (dev->identify.capabilities & BIT(8)) != 0;
    dev->present = true;

    KLOG_INFO("ATA", "Device %d: %s (%llu sectors, %llu MB)",
              dev->device_id, dev->model,
              dev->sectors, dev->size_bytes / (1024 * 1024));

    return STATUS_OK;
}

/* PIO Read (LBA28) */
status_t ata_pio_read28(ata_device_t* dev, uint32_t lba, uint8_t count, void* buffer) {
    if (!dev || !buffer || lba > 0x0FFFFFFF) {
        return STATUS_INVALID;
    }

    __sync_lock_test_and_set(&dev->lock, 1);

    /* Wait for ready */
    status_t result = ata_wait_ready(dev, 1000);
    if (FAILED(result)) {
        __sync_lock_release(&dev->lock);
        return result;
    }

    /* Select drive and set LBA mode */
    ata_outb(dev->io_base + ATA_REG_HDDEVSEL, 0xE0 | (dev->drive << 4) | ((lba >> 24) & 0x0F));
    ata_delay_400ns(dev);

    /* Set sector count and LBA */
    ata_outb(dev->io_base + ATA_REG_SECCOUNT0, count);
    ata_outb(dev->io_base + ATA_REG_LBA0, (uint8_t)lba);
    ata_outb(dev->io_base + ATA_REG_LBA1, (uint8_t)(lba >> 8));
    ata_outb(dev->io_base + ATA_REG_LBA2, (uint8_t)(lba >> 16));

    /* Send read command */
    ata_outb(dev->io_base + ATA_REG_COMMAND, ATA_CMD_READ_PIO);
    ata_delay_400ns(dev);

    /* Read sectors */
    uint16_t* buf = (uint16_t*)buffer;
    for (uint32_t sector = 0; sector < count; sector++) {
        result = ata_wait_drq(dev, 1000);
        if (FAILED(result)) {
            __sync_lock_release(&dev->lock);
            ata_stats.read_errors++;
            return result;
        }

        /* Read 256 words (512 bytes) */
        for (int i = 0; i < 256; i++) {
            buf[sector * 256 + i] = ata_inw(dev->io_base + ATA_REG_DATA);
        }

        ata_delay_400ns(dev);
    }

    __sync_lock_release(&dev->lock);

    ata_stats.total_reads++;
    ata_stats.bytes_read += count * ATA_SECTOR_SIZE;

    return STATUS_OK;
}

/* PIO Read (LBA48) */
status_t ata_pio_read48(ata_device_t* dev, uint64_t lba, uint32_t count, void* buffer) {
    if (!dev || !buffer || !dev->supports_lba48) {
        return STATUS_INVALID;
    }

    __sync_lock_test_and_set(&dev->lock, 1);

    /* Wait for ready */
    status_t result = ata_wait_ready(dev, 1000);
    if (FAILED(result)) {
        __sync_lock_release(&dev->lock);
        return result;
    }

    /* Select drive */
    ata_outb(dev->io_base + ATA_REG_HDDEVSEL, 0x40 | (dev->drive << 4));
    ata_delay_400ns(dev);

    /* Set sector count (high byte, then low byte) */
    ata_outb(dev->io_base + ATA_REG_SECCOUNT0, (uint8_t)(count >> 8));
    ata_outb(dev->io_base + ATA_REG_SECCOUNT0, (uint8_t)count);

    /* Set LBA (high bytes, then low bytes) */
    ata_outb(dev->io_base + ATA_REG_LBA0, (uint8_t)(lba >> 24));
    ata_outb(dev->io_base + ATA_REG_LBA1, (uint8_t)(lba >> 32));
    ata_outb(dev->io_base + ATA_REG_LBA2, (uint8_t)(lba >> 40));
    ata_outb(dev->io_base + ATA_REG_LBA0, (uint8_t)lba);
    ata_outb(dev->io_base + ATA_REG_LBA1, (uint8_t)(lba >> 8));
    ata_outb(dev->io_base + ATA_REG_LBA2, (uint8_t)(lba >> 16));

    /* Send read command */
    ata_outb(dev->io_base + ATA_REG_COMMAND, ATA_CMD_READ_PIO_EXT);
    ata_delay_400ns(dev);

    /* Read sectors */
    uint16_t* buf = (uint16_t*)buffer;
    for (uint32_t sector = 0; sector < count; sector++) {
        result = ata_wait_drq(dev, 1000);
        if (FAILED(result)) {
            __sync_lock_release(&dev->lock);
            ata_stats.read_errors++;
            return result;
        }

        /* Read 256 words (512 bytes) */
        for (int i = 0; i < 256; i++) {
            buf[sector * 256 + i] = ata_inw(dev->io_base + ATA_REG_DATA);
        }

        ata_delay_400ns(dev);
    }

    __sync_lock_release(&dev->lock);

    ata_stats.total_reads++;
    ata_stats.bytes_read += count * ATA_SECTOR_SIZE;

    return STATUS_OK;
}

/* PIO Write (LBA28) */
status_t ata_pio_write28(ata_device_t* dev, uint32_t lba, uint8_t count, const void* buffer) {
    if (!dev || !buffer || lba > 0x0FFFFFFF) {
        return STATUS_INVALID;
    }

    __sync_lock_test_and_set(&dev->lock, 1);

    /* Wait for ready */
    status_t result = ata_wait_ready(dev, 1000);
    if (FAILED(result)) {
        __sync_lock_release(&dev->lock);
        return result;
    }

    /* Select drive and set LBA mode */
    ata_outb(dev->io_base + ATA_REG_HDDEVSEL, 0xE0 | (dev->drive << 4) | ((lba >> 24) & 0x0F));
    ata_delay_400ns(dev);

    /* Set sector count and LBA */
    ata_outb(dev->io_base + ATA_REG_SECCOUNT0, count);
    ata_outb(dev->io_base + ATA_REG_LBA0, (uint8_t)lba);
    ata_outb(dev->io_base + ATA_REG_LBA1, (uint8_t)(lba >> 8));
    ata_outb(dev->io_base + ATA_REG_LBA2, (uint8_t)(lba >> 16));

    /* Send write command */
    ata_outb(dev->io_base + ATA_REG_COMMAND, ATA_CMD_WRITE_PIO);
    ata_delay_400ns(dev);

    /* Write sectors */
    const uint16_t* buf = (const uint16_t*)buffer;
    for (uint32_t sector = 0; sector < count; sector++) {
        result = ata_wait_drq(dev, 1000);
        if (FAILED(result)) {
            __sync_lock_release(&dev->lock);
            ata_stats.write_errors++;
            return result;
        }

        /* Write 256 words (512 bytes) */
        for (int i = 0; i < 256; i++) {
            ata_outw(dev->io_base + ATA_REG_DATA, buf[sector * 256 + i]);
        }

        ata_delay_400ns(dev);
    }

    /* Flush cache */
    ata_outb(dev->io_base + ATA_REG_COMMAND, ATA_CMD_CACHE_FLUSH);
    result = ata_wait_ready(dev, 1000);

    __sync_lock_release(&dev->lock);

    ata_stats.total_writes++;
    ata_stats.bytes_written += count * ATA_SECTOR_SIZE;

    return result;
}

/* PIO Write (LBA48) */
status_t ata_pio_write48(ata_device_t* dev, uint64_t lba, uint32_t count, const void* buffer) {
    if (!dev || !buffer || !dev->supports_lba48) {
        return STATUS_INVALID;
    }

    __sync_lock_test_and_set(&dev->lock, 1);

    /* Wait for ready */
    status_t result = ata_wait_ready(dev, 1000);
    if (FAILED(result)) {
        __sync_lock_release(&dev->lock);
        return result;
    }

    /* Select drive */
    ata_outb(dev->io_base + ATA_REG_HDDEVSEL, 0x40 | (dev->drive << 4));
    ata_delay_400ns(dev);

    /* Set sector count (high byte, then low byte) */
    ata_outb(dev->io_base + ATA_REG_SECCOUNT0, (uint8_t)(count >> 8));
    ata_outb(dev->io_base + ATA_REG_SECCOUNT0, (uint8_t)count);

    /* Set LBA (high bytes, then low bytes) */
    ata_outb(dev->io_base + ATA_REG_LBA0, (uint8_t)(lba >> 24));
    ata_outb(dev->io_base + ATA_REG_LBA1, (uint8_t)(lba >> 32));
    ata_outb(dev->io_base + ATA_REG_LBA2, (uint8_t)(lba >> 40));
    ata_outb(dev->io_base + ATA_REG_LBA0, (uint8_t)lba);
    ata_outb(dev->io_base + ATA_REG_LBA1, (uint8_t)(lba >> 8));
    ata_outb(dev->io_base + ATA_REG_LBA2, (uint8_t)(lba >> 16));

    /* Send write command */
    ata_outb(dev->io_base + ATA_REG_COMMAND, ATA_CMD_WRITE_PIO_EXT);
    ata_delay_400ns(dev);

    /* Write sectors */
    const uint16_t* buf = (const uint16_t*)buffer;
    for (uint32_t sector = 0; sector < count; sector++) {
        result = ata_wait_drq(dev, 1000);
        if (FAILED(result)) {
            __sync_lock_release(&dev->lock);
            ata_stats.write_errors++;
            return result;
        }

        /* Write 256 words (512 bytes) */
        for (int i = 0; i < 256; i++) {
            ata_outw(dev->io_base + ATA_REG_DATA, buf[sector * 256 + i]);
        }

        ata_delay_400ns(dev);
    }

    /* Flush cache */
    ata_outb(dev->io_base + ATA_REG_COMMAND, ATA_CMD_CACHE_FLUSH_EXT);
    result = ata_wait_ready(dev, 1000);

    __sync_lock_release(&dev->lock);

    ata_stats.total_writes++;
    ata_stats.bytes_written += count * ATA_SECTOR_SIZE;

    return result;
}

/* High-level read sectors */
status_t ata_read_sectors(ata_device_t* dev, uint64_t lba, uint32_t count, void* buffer) {
    if (!dev || !dev->present || !buffer || count == 0) {
        return STATUS_INVALID;
    }

    if (lba + count > dev->sectors) {
        return STATUS_INVALID;
    }

    /* Use LBA48 if needed */
    if (lba > 0x0FFFFFFF || count > 255 || dev->supports_lba48) {
        return ata_pio_read48(dev, lba, count, buffer);
    } else {
        return ata_pio_read28(dev, (uint32_t)lba, (uint8_t)count, buffer);
    }
}

/* High-level write sectors */
status_t ata_write_sectors(ata_device_t* dev, uint64_t lba, uint32_t count, const void* buffer) {
    if (!dev || !dev->present || !buffer || count == 0) {
        return STATUS_INVALID;
    }

    if (lba + count > dev->sectors) {
        return STATUS_INVALID;
    }

    /* Use LBA48 if needed */
    if (lba > 0x0FFFFFFF || count > 255 || dev->supports_lba48) {
        return ata_pio_write48(dev, lba, count, buffer);
    } else {
        return ata_pio_write28(dev, (uint32_t)lba, (uint8_t)count, buffer);
    }
}

/* Flush cache */
status_t ata_flush_cache(ata_device_t* dev) {
    if (!dev || !dev->present) {
        return STATUS_INVALID;
    }

    __sync_lock_test_and_set(&dev->lock, 1);

    status_t result = ata_wait_ready(dev, 1000);
    if (FAILED(result)) {
        __sync_lock_release(&dev->lock);
        return result;
    }

    if (dev->supports_lba48) {
        ata_outb(dev->io_base + ATA_REG_COMMAND, ATA_CMD_CACHE_FLUSH_EXT);
    } else {
        ata_outb(dev->io_base + ATA_REG_COMMAND, ATA_CMD_CACHE_FLUSH);
    }

    result = ata_wait_ready(dev, 3000);

    __sync_lock_release(&dev->lock);

    return result;
}

/* Detect all ATA devices */
status_t ata_detect_devices(void) {
    uint8_t device_id = 0;

    /* Check primary and secondary channels */
    for (uint8_t channel = 0; channel < 2; channel++) {
        uint16_t io_base = (channel == 0) ? ATA_PRIMARY_IO : ATA_SECONDARY_IO;
        uint16_t ctrl_base = (channel == 0) ? ATA_PRIMARY_CTRL : ATA_SECONDARY_CTRL;

        /* Check master and slave */
        for (uint8_t drive = 0; drive < 2; drive++) {
            if (device_id >= ATA_MAX_DEVICES) {
                break;
            }

            ata_device_t* dev = &ata_driver.devices[device_id];
            dev->device_id = device_id;
            dev->channel = channel;
            dev->drive = drive;
            dev->io_base = io_base;
            dev->ctrl_base = ctrl_base;
            dev->interface_type = ATA_TYPE_PIO;
            dev->present = false;
            dev->lock = 0;

            /* Try to identify device */
            status_t result = ata_identify_device(dev);
            if (SUCCESS(result)) {
                ata_driver.device_count++;
            }

            device_id++;
        }
    }

    KLOG_INFO("ATA", "Detected %d device(s)", ata_driver.device_count);
    return STATUS_OK;
}

/* Get device by ID */
ata_device_t* ata_get_device(uint8_t device_id) {
    if (device_id >= ATA_MAX_DEVICES) {
        return NULL;
    }

    ata_device_t* dev = &ata_driver.devices[device_id];
    return dev->present ? dev : NULL;
}

/* Get device count */
uint32_t ata_get_device_count(void) {
    return ata_driver.device_count;
}

/* Get statistics */
status_t ata_get_stats(ata_stats_t* stats) {
    if (!stats) {
        return STATUS_INVALID;
    }

    *stats = ata_stats;
    return STATUS_OK;
}

/* Initialize ATA driver */
status_t ata_init(void) {
    if (ata_driver.initialized) {
        return STATUS_EXISTS;
    }

    KLOG_INFO("ATA", "Initializing ATA/AHCI driver");

    /* Initialize driver state */
    for (uint32_t i = 0; i < ATA_MAX_DEVICES; i++) {
        ata_driver.devices[i].present = false;
    }

    ata_driver.device_count = 0;
    ata_driver.lock = 0;
    ata_driver.initialized = true;

    /* Detect devices */
    status_t result = ata_detect_devices();

    if (ata_driver.device_count == 0) {
        KLOG_WARN("ATA", "No ATA devices detected");
    }

    return result;
}

/* Shutdown ATA driver */
void ata_shutdown(void) {
    if (!ata_driver.initialized) {
        return;
    }

    /* Flush all device caches */
    for (uint32_t i = 0; i < ATA_MAX_DEVICES; i++) {
        if (ata_driver.devices[i].present) {
            ata_flush_cache(&ata_driver.devices[i]);
        }
    }

    ata_driver.initialized = false;
    KLOG_INFO("ATA", "ATA driver shutdown");
}
