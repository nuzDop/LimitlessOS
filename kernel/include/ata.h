#ifndef LIMITLESS_ATA_H
#define LIMITLESS_ATA_H

/*
 * ATA/AHCI Storage Driver
 * Support for SATA and legacy ATA devices
 */

#include "kernel.h"

/* ATA commands */
#define ATA_CMD_READ_PIO        0x20
#define ATA_CMD_READ_PIO_EXT    0x24
#define ATA_CMD_READ_DMA        0xC8
#define ATA_CMD_READ_DMA_EXT    0x25
#define ATA_CMD_WRITE_PIO       0x30
#define ATA_CMD_WRITE_PIO_EXT   0x34
#define ATA_CMD_WRITE_DMA       0xCA
#define ATA_CMD_WRITE_DMA_EXT   0x35
#define ATA_CMD_CACHE_FLUSH     0xE7
#define ATA_CMD_CACHE_FLUSH_EXT 0xEA
#define ATA_CMD_PACKET          0xA0
#define ATA_CMD_IDENTIFY_PACKET 0xA1
#define ATA_CMD_IDENTIFY        0xEC

/* ATA registers */
#define ATA_REG_DATA       0x00
#define ATA_REG_ERROR      0x01
#define ATA_REG_FEATURES   0x01
#define ATA_REG_SECCOUNT0  0x02
#define ATA_REG_LBA0       0x03
#define ATA_REG_LBA1       0x04
#define ATA_REG_LBA2       0x05
#define ATA_REG_HDDEVSEL   0x06
#define ATA_REG_COMMAND    0x07
#define ATA_REG_STATUS     0x07
#define ATA_REG_SECCOUNT1  0x08
#define ATA_REG_LBA3       0x09
#define ATA_REG_LBA4       0x0A
#define ATA_REG_LBA5       0x0B
#define ATA_REG_CONTROL    0x0C
#define ATA_REG_ALTSTATUS  0x0C
#define ATA_REG_DEVADDRESS 0x0D

/* ATA status bits */
#define ATA_SR_BSY  0x80  // Busy
#define ATA_SR_DRDY 0x40  // Drive ready
#define ATA_SR_DF   0x20  // Drive write fault
#define ATA_SR_DSC  0x10  // Drive seek complete
#define ATA_SR_DRQ  0x08  // Data request ready
#define ATA_SR_CORR 0x04  // Corrected data
#define ATA_SR_IDX  0x02  // Index
#define ATA_SR_ERR  0x01  // Error

/* ATA error bits */
#define ATA_ER_BBK   0x80  // Bad block
#define ATA_ER_UNC   0x40  // Uncorrectable data
#define ATA_ER_MC    0x20  // Media changed
#define ATA_ER_IDNF  0x10  // ID not found
#define ATA_ER_MCR   0x08  // Media change request
#define ATA_ER_ABRT  0x04  // Command aborted
#define ATA_ER_TK0NF 0x02  // Track 0 not found
#define ATA_ER_AMNF  0x01  // Address mark not found

/* ATA device types */
#define ATA_DEV_UNKNOWN 0
#define ATA_DEV_ATA     1
#define ATA_DEV_ATAPI   2

/* ATA interface types */
#define ATA_TYPE_PIO  0
#define ATA_TYPE_DMA  1
#define ATA_TYPE_AHCI 2

/* Standard ATA I/O ports */
#define ATA_PRIMARY_IO      0x1F0
#define ATA_PRIMARY_CTRL    0x3F6
#define ATA_SECONDARY_IO    0x170
#define ATA_SECONDARY_CTRL  0x376

/* Maximum devices */
#define ATA_MAX_DEVICES 8

/* Sector size */
#define ATA_SECTOR_SIZE 512

/* ATA device information */
typedef struct ata_identify {
    uint16_t config;           // General configuration
    uint16_t cylinders;        // Number of cylinders
    uint16_t reserved1;
    uint16_t heads;            // Number of heads
    uint16_t reserved2[2];
    uint16_t sectors;          // Sectors per track
    uint16_t reserved3[3];
    char serial[20];           // Serial number (ASCII)
    uint16_t reserved4[3];
    char firmware[8];          // Firmware revision
    char model[40];            // Model number
    uint16_t reserved5[2];
    uint32_t capabilities;     // Capabilities
    uint16_t reserved6[9];
    uint32_t lba28_sectors;    // Total sectors (LBA28)
    uint16_t reserved7[38];
    uint64_t lba48_sectors;    // Total sectors (LBA48)
    uint16_t reserved8[152];
} PACKED ata_identify_t;

/* ATA device structure */
typedef struct ata_device {
    uint8_t device_id;         // Device ID (0-7)
    uint8_t channel;           // 0=Primary, 1=Secondary
    uint8_t drive;             // 0=Master, 1=Slave
    uint8_t type;              // ATA_DEV_*
    uint8_t interface_type;    // ATA_TYPE_*

    uint16_t io_base;          // I/O base port
    uint16_t ctrl_base;        // Control base port

    bool present;              // Device detected
    bool supports_lba48;       // LBA48 support
    bool supports_dma;         // DMA support

    uint64_t sectors;          // Total sectors
    uint64_t size_bytes;       // Total size in bytes

    char model[41];            // Model string (null-terminated)
    char serial[21];           // Serial number (null-terminated)
    char firmware[9];          // Firmware revision (null-terminated)

    ata_identify_t identify;   // IDENTIFY data

    uint32_t lock;             // Device lock
} ata_device_t;

/* ATA driver state */
typedef struct ata_driver {
    ata_device_t devices[ATA_MAX_DEVICES];
    uint32_t device_count;
    bool initialized;
    uint32_t lock;
} ata_driver_t;

/* ATA driver API */
status_t ata_init(void);
void ata_shutdown(void);

/* Device management */
status_t ata_detect_devices(void);
ata_device_t* ata_get_device(uint8_t device_id);
uint32_t ata_get_device_count(void);

/* I/O operations */
status_t ata_read_sectors(ata_device_t* dev, uint64_t lba, uint32_t count, void* buffer);
status_t ata_write_sectors(ata_device_t* dev, uint64_t lba, uint32_t count, const void* buffer);
status_t ata_flush_cache(ata_device_t* dev);

/* PIO operations (internal) */
status_t ata_pio_read28(ata_device_t* dev, uint32_t lba, uint8_t count, void* buffer);
status_t ata_pio_read48(ata_device_t* dev, uint64_t lba, uint32_t count, void* buffer);
status_t ata_pio_write28(ata_device_t* dev, uint32_t lba, uint8_t count, const void* buffer);
status_t ata_pio_write48(ata_device_t* dev, uint64_t lba, uint32_t count, const void* buffer);

/* Utility functions */
status_t ata_identify_device(ata_device_t* dev);
status_t ata_wait_ready(ata_device_t* dev, uint32_t timeout_ms);
status_t ata_wait_drq(ata_device_t* dev, uint32_t timeout_ms);
uint8_t ata_read_status(ata_device_t* dev);
void ata_delay_400ns(ata_device_t* dev);

/* Statistics */
typedef struct ata_stats {
    uint64_t total_reads;
    uint64_t total_writes;
    uint64_t bytes_read;
    uint64_t bytes_written;
    uint64_t read_errors;
    uint64_t write_errors;
} ata_stats_t;

status_t ata_get_stats(ata_stats_t* stats);

#endif /* LIMITLESS_ATA_H */
