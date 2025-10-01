#ifndef LIMITLESS_E1000_H
#define LIMITLESS_E1000_H

/*
 * Intel e1000 Ethernet Driver
 * Support for 82540EM/82545EM (QEMU default)
 */

#include "kernel.h"
#include "net.h"

/* PCI vendor/device IDs */
#define E1000_VENDOR_ID 0x8086
#define E1000_DEVICE_82540EM 0x100E
#define E1000_DEVICE_82545EM 0x100F

/* Register offsets */
#define E1000_REG_CTRL     0x0000  // Control
#define E1000_REG_STATUS   0x0008  // Device Status
#define E1000_REG_EECD     0x0010  // EEPROM Control
#define E1000_REG_EERD     0x0014  // EEPROM Read
#define E1000_REG_CTRL_EXT 0x0018  // Extended Control
#define E1000_REG_ICR      0x00C0  // Interrupt Cause Read
#define E1000_REG_ITR      0x00C4  // Interrupt Throttling
#define E1000_REG_ICS      0x00C8  // Interrupt Cause Set
#define E1000_REG_IMS      0x00D0  // Interrupt Mask Set
#define E1000_REG_IMC      0x00D8  // Interrupt Mask Clear
#define E1000_REG_RCTL     0x0100  // Receive Control
#define E1000_REG_TCTL     0x0400  // Transmit Control
#define E1000_REG_RDBAL    0x2800  // RX Descriptor Base Low
#define E1000_REG_RDBAH    0x2804  // RX Descriptor Base High
#define E1000_REG_RDLEN    0x2808  // RX Descriptor Length
#define E1000_REG_RDH      0x2810  // RX Descriptor Head
#define E1000_REG_RDT      0x2818  // RX Descriptor Tail
#define E1000_REG_TDBAL    0x3800  // TX Descriptor Base Low
#define E1000_REG_TDBAH    0x3804  // TX Descriptor Base High
#define E1000_REG_TDLEN    0x3808  // TX Descriptor Length
#define E1000_REG_TDH      0x3810  // TX Descriptor Head
#define E1000_REG_TDT      0x3818  // TX Descriptor Tail
#define E1000_REG_MTA      0x5200  // Multicast Table Array
#define E1000_REG_RAL      0x5400  // Receive Address Low
#define E1000_REG_RAH      0x5404  // Receive Address High

/* Control Register bits */
#define E1000_CTRL_FD      BIT(0)   // Full Duplex
#define E1000_CTRL_LRST    BIT(3)   // Link Reset
#define E1000_CTRL_ASDE    BIT(5)   // Auto Speed Detection Enable
#define E1000_CTRL_SLU     BIT(6)   // Set Link Up
#define E1000_CTRL_RST     BIT(26)  // Device Reset
#define E1000_CTRL_VME     BIT(30)  // VLAN Mode Enable
#define E1000_CTRL_PHY_RST BIT(31)  // PHY Reset

/* Receive Control bits */
#define E1000_RCTL_EN      BIT(1)   // Receiver Enable
#define E1000_RCTL_SBP     BIT(2)   // Store Bad Packets
#define E1000_RCTL_UPE     BIT(3)   // Unicast Promiscuous Enable
#define E1000_RCTL_MPE     BIT(4)   // Multicast Promiscuous Enable
#define E1000_RCTL_LPE     BIT(5)   // Long Packet Enable
#define E1000_RCTL_LBM     (BIT(6)|BIT(7))  // Loopback Mode
#define E1000_RCTL_BAM     BIT(15)  // Broadcast Accept Mode
#define E1000_RCTL_BSIZE_2048 0     // Buffer Size 2048
#define E1000_RCTL_BSEX    BIT(25)  // Buffer Size Extension
#define E1000_RCTL_SECRC   BIT(26)  // Strip Ethernet CRC

/* Transmit Control bits */
#define E1000_TCTL_EN      BIT(1)   // Transmit Enable
#define E1000_TCTL_PSP     BIT(3)   // Pad Short Packets
#define E1000_TCTL_CT      (0xFF << 4)  // Collision Threshold
#define E1000_TCTL_COLD    (0x3FF << 12) // Collision Distance

/* Descriptor counts */
#define E1000_NUM_RX_DESC 256
#define E1000_NUM_TX_DESC 256

/* Receive Descriptor */
typedef struct e1000_rx_desc {
    uint64_t buffer_addr;
    uint16_t length;
    uint16_t checksum;
    uint8_t status;
    uint8_t errors;
    uint16_t special;
} PACKED e1000_rx_desc_t;

/* RX Descriptor status bits */
#define E1000_RXD_STAT_DD  BIT(0)  // Descriptor Done
#define E1000_RXD_STAT_EOP BIT(1)  // End of Packet

/* Transmit Descriptor */
typedef struct e1000_tx_desc {
    uint64_t buffer_addr;
    uint16_t length;
    uint8_t cso;
    uint8_t cmd;
    uint8_t status;
    uint8_t css;
    uint16_t special;
} PACKED e1000_tx_desc_t;

/* TX Descriptor command bits */
#define E1000_TXD_CMD_EOP  BIT(0)  // End of Packet
#define E1000_TXD_CMD_RS   BIT(3)  // Report Status

/* TX Descriptor status bits */
#define E1000_TXD_STAT_DD  BIT(0)  // Descriptor Done

/* e1000 device structure */
typedef struct e1000_device {
    /* PCI information */
    uint8_t bus;
    uint8_t device;
    uint8_t function;
    uint16_t device_id;

    /* Memory-mapped registers */
    uintptr_t mmio_base;
    size_t mmio_size;

    /* MAC address */
    mac_addr_t mac;

    /* Descriptor rings */
    e1000_rx_desc_t* rx_descs;
    paddr_t rx_descs_phys;
    uint8_t** rx_buffers;
    uint16_t rx_tail;

    e1000_tx_desc_t* tx_descs;
    paddr_t tx_descs_phys;
    uint8_t** tx_buffers;
    uint16_t tx_tail;

    /* Network interface */
    net_interface_t* iface;

    /* State */
    bool initialized;
    uint32_t lock;
} e1000_device_t;

/* e1000 driver API */
status_t e1000_init(void);
void e1000_shutdown(void);

/* Device management */
status_t e1000_detect_devices(void);
e1000_device_t* e1000_get_device(uint8_t index);

/* I/O operations */
status_t e1000_send_packet(net_interface_t* iface, const void* data, size_t length);
void e1000_receive_packets(e1000_device_t* dev);

/* Register access */
void e1000_write_reg(e1000_device_t* dev, uint32_t reg, uint32_t value);
uint32_t e1000_read_reg(e1000_device_t* dev, uint32_t reg);

/* Initialization helpers */
status_t e1000_read_mac(e1000_device_t* dev);
status_t e1000_reset(e1000_device_t* dev);
status_t e1000_init_rx(e1000_device_t* dev);
status_t e1000_init_tx(e1000_device_t* dev);
status_t e1000_init_device(uintptr_t mmio_base, size_t mmio_size);

#endif /* LIMITLESS_E1000_H */
