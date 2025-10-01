/*
 * Intel e1000 Ethernet Driver Implementation
 * Supports 82540EM/82545EM (QEMU default)
 */

#include "kernel.h"
#include "microkernel.h"
#include "vmm.h"
#include "e1000.h"
#include "net.h"
#include "hal.h"

/* Global e1000 device (supports single device for now) */
static e1000_device_t e1000_dev = {0};

/* Write to e1000 register */
void e1000_write_reg(e1000_device_t* dev, uint32_t reg, uint32_t value) {
    volatile uint32_t* addr = (volatile uint32_t*)(dev->mmio_base + reg);
    *addr = value;
}

/* Read from e1000 register */
uint32_t e1000_read_reg(e1000_device_t* dev, uint32_t reg) {
    volatile uint32_t* addr = (volatile uint32_t*)(dev->mmio_base + reg);
    return *addr;
}

/* Read MAC address from EEPROM */
status_t e1000_read_mac(e1000_device_t* dev) {
    /* Read from Receive Address Low/High registers */
    uint32_t ral = e1000_read_reg(dev, E1000_REG_RAL);
    uint32_t rah = e1000_read_reg(dev, E1000_REG_RAH);

    dev->mac.addr[0] = (ral >> 0) & 0xFF;
    dev->mac.addr[1] = (ral >> 8) & 0xFF;
    dev->mac.addr[2] = (ral >> 16) & 0xFF;
    dev->mac.addr[3] = (ral >> 24) & 0xFF;
    dev->mac.addr[4] = (rah >> 0) & 0xFF;
    dev->mac.addr[5] = (rah >> 8) & 0xFF;

    KLOG_INFO("E1000", "MAC address: %02x:%02x:%02x:%02x:%02x:%02x",
              dev->mac.addr[0], dev->mac.addr[1], dev->mac.addr[2],
              dev->mac.addr[3], dev->mac.addr[4], dev->mac.addr[5]);

    return STATUS_OK;
}

/* Reset e1000 device */
status_t e1000_reset(e1000_device_t* dev) {
    /* Disable interrupts */
    e1000_write_reg(dev, E1000_REG_IMC, 0xFFFFFFFF);

    /* Reset device */
    uint32_t ctrl = e1000_read_reg(dev, E1000_REG_CTRL);
    e1000_write_reg(dev, E1000_REG_CTRL, ctrl | E1000_CTRL_RST);

    /* Wait for reset to complete */
    for (volatile int i = 0; i < 100000; i++);

    /* Disable interrupts again */
    e1000_write_reg(dev, E1000_REG_IMC, 0xFFFFFFFF);

    /* Clear interrupt causes */
    e1000_read_reg(dev, E1000_REG_ICR);

    return STATUS_OK;
}

/* Initialize receive ring */
status_t e1000_init_rx(e1000_device_t* dev) {
    /* Allocate descriptor ring */
    size_t desc_size = E1000_NUM_RX_DESC * sizeof(e1000_rx_desc_t);
    size_t desc_pages = (desc_size + PAGE_SIZE - 1) / PAGE_SIZE;

    paddr_t desc_phys = pmm_alloc_pages(desc_pages);
    if (!desc_phys) {
        return STATUS_NOMEM;
    }

    dev->rx_descs = (e1000_rx_desc_t*)PHYS_TO_VIRT_DIRECT(desc_phys);
    dev->rx_descs_phys = desc_phys;

    /* Allocate buffers for each descriptor */
    dev->rx_buffers = (uint8_t**)pmm_alloc_page();
    if (!dev->rx_buffers) {
        pmm_free_pages(desc_phys, desc_pages);
        return STATUS_NOMEM;
    }

    for (int i = 0; i < E1000_NUM_RX_DESC; i++) {
        paddr_t buf_phys = pmm_alloc_page();
        if (!buf_phys) {
            return STATUS_NOMEM;
        }

        dev->rx_buffers[i] = (uint8_t*)PHYS_TO_VIRT_DIRECT(buf_phys);
        dev->rx_descs[i].buffer_addr = buf_phys;
        dev->rx_descs[i].status = 0;
    }

    /* Set descriptor ring registers */
    e1000_write_reg(dev, E1000_REG_RDBAL, (uint32_t)(desc_phys & 0xFFFFFFFF));
    e1000_write_reg(dev, E1000_REG_RDBAH, (uint32_t)(desc_phys >> 32));
    e1000_write_reg(dev, E1000_REG_RDLEN, E1000_NUM_RX_DESC * sizeof(e1000_rx_desc_t));

    /* Set head and tail */
    e1000_write_reg(dev, E1000_REG_RDH, 0);
    e1000_write_reg(dev, E1000_REG_RDT, E1000_NUM_RX_DESC - 1);
    dev->rx_tail = 0;

    /* Enable receiver */
    uint32_t rctl = E1000_RCTL_EN |        // Enable
                    E1000_RCTL_BAM |       // Broadcast Accept
                    E1000_RCTL_BSIZE_2048 | // 2KB buffer
                    E1000_RCTL_SECRC;      // Strip CRC

    e1000_write_reg(dev, E1000_REG_RCTL, rctl);

    return STATUS_OK;
}

/* Initialize transmit ring */
status_t e1000_init_tx(e1000_device_t* dev) {
    /* Allocate descriptor ring */
    size_t desc_size = E1000_NUM_TX_DESC * sizeof(e1000_tx_desc_t);
    size_t desc_pages = (desc_size + PAGE_SIZE - 1) / PAGE_SIZE;

    paddr_t desc_phys = pmm_alloc_pages(desc_pages);
    if (!desc_phys) {
        return STATUS_NOMEM;
    }

    dev->tx_descs = (e1000_tx_desc_t*)PHYS_TO_VIRT_DIRECT(desc_phys);
    dev->tx_descs_phys = desc_phys;

    /* Allocate buffers for each descriptor */
    dev->tx_buffers = (uint8_t**)pmm_alloc_page();
    if (!dev->tx_buffers) {
        pmm_free_pages(desc_phys, desc_pages);
        return STATUS_NOMEM;
    }

    for (int i = 0; i < E1000_NUM_TX_DESC; i++) {
        paddr_t buf_phys = pmm_alloc_page();
        if (!buf_phys) {
            return STATUS_NOMEM;
        }

        dev->tx_buffers[i] = (uint8_t*)PHYS_TO_VIRT_DIRECT(buf_phys);
        dev->tx_descs[i].buffer_addr = buf_phys;
        dev->tx_descs[i].status = E1000_TXD_STAT_DD;
        dev->tx_descs[i].cmd = 0;
    }

    /* Set descriptor ring registers */
    e1000_write_reg(dev, E1000_REG_TDBAL, (uint32_t)(desc_phys & 0xFFFFFFFF));
    e1000_write_reg(dev, E1000_REG_TDBAH, (uint32_t)(desc_phys >> 32));
    e1000_write_reg(dev, E1000_REG_TDLEN, E1000_NUM_TX_DESC * sizeof(e1000_tx_desc_t));

    /* Set head and tail */
    e1000_write_reg(dev, E1000_REG_TDH, 0);
    e1000_write_reg(dev, E1000_REG_TDT, 0);
    dev->tx_tail = 0;

    /* Enable transmitter */
    uint32_t tctl = E1000_TCTL_EN |     // Enable
                    E1000_TCTL_PSP |    // Pad Short Packets
                    (0x0F << 4) |       // Collision Threshold
                    (0x40 << 12);       // Collision Distance

    e1000_write_reg(dev, E1000_REG_TCTL, tctl);

    return STATUS_OK;
}

/* Send packet */
status_t e1000_send_packet(net_interface_t* iface, const void* data, size_t length) {
    e1000_device_t* dev = (e1000_device_t*)iface->driver_data;

    if (!dev || !dev->initialized || length > PAGE_SIZE) {
        return STATUS_INVALID;
    }

    __sync_lock_test_and_set(&dev->lock, 1);

    /* Get next TX descriptor */
    uint16_t tail = dev->tx_tail;
    e1000_tx_desc_t* desc = &dev->tx_descs[tail];

    /* Wait if descriptor is in use */
    while (!(desc->status & E1000_TXD_STAT_DD)) {
        /* Descriptor not ready, would need timeout */
    }

    /* Copy data to buffer */
    uint8_t* buffer = dev->tx_buffers[tail];
    for (size_t i = 0; i < length; i++) {
        buffer[i] = ((const uint8_t*)data)[i];
    }

    /* Setup descriptor */
    desc->length = length;
    desc->cmd = E1000_TXD_CMD_EOP | E1000_TXD_CMD_RS;
    desc->status = 0;

    /* Update tail */
    dev->tx_tail = (tail + 1) % E1000_NUM_TX_DESC;
    e1000_write_reg(dev, E1000_REG_TDT, dev->tx_tail);

    __sync_lock_release(&dev->lock);

    return STATUS_OK;
}

/* Receive packets */
void e1000_receive_packets(e1000_device_t* dev) {
    if (!dev || !dev->initialized) {
        return;
    }

    uint16_t tail = dev->rx_tail;

    while (1) {
        e1000_rx_desc_t* desc = &dev->rx_descs[tail];

        /* Check if descriptor has data */
        if (!(desc->status & E1000_RXD_STAT_DD)) {
            break;
        }

        /* Process packet */
        if (desc->status & E1000_RXD_STAT_EOP) {
            uint8_t* buffer = dev->rx_buffers[tail];
            size_t length = desc->length;

            /* Pass to network stack */
            net_receive_packet(dev->iface, buffer, length);
        }

        /* Reset descriptor */
        desc->status = 0;

        /* Update tail */
        tail = (tail + 1) % E1000_NUM_RX_DESC;
        e1000_write_reg(dev, E1000_REG_RDT, tail);
    }

    dev->rx_tail = tail;
}

/* Detect e1000 devices using HAL PCI */
status_t e1000_detect_devices(void) {
    /* Intel e1000 vendor/device IDs */
    const uint16_t E1000_VENDOR_INTEL = 0x8086;
    const uint16_t E1000_DEV_82540EM = 0x100E;
    const uint16_t E1000_DEV_82545EM = 0x100F;
    const uint16_t E1000_DEV_82574L = 0x10D3;

    /* Get PCI device count */
    uint32_t device_count = hal_pci_get_device_count();
    if (device_count == 0) {
        KLOG_WARN("E1000", "No PCI devices found");
        return STATUS_NOTFOUND;
    }

    /* Scan for e1000 devices */
    for (uint32_t i = 0; i < device_count; i++) {
        pci_device_t pci_dev;
        status_t result = hal_pci_get_device(i, &pci_dev);
        if (FAILED(result)) {
            continue;
        }

        /* Check if this is an Intel e1000 */
        if (pci_dev.vendor_id == E1000_VENDOR_INTEL &&
            (pci_dev.device_id == E1000_DEV_82540EM ||
             pci_dev.device_id == E1000_DEV_82545EM ||
             pci_dev.device_id == E1000_DEV_82574L)) {

            KLOG_INFO("E1000", "Found device %04x:%04x at %02x:%02x.%x",
                     pci_dev.vendor_id, pci_dev.device_id,
                     pci_dev.bus, pci_dev.device, pci_dev.function);

            /* Get MMIO base address from BAR0 */
            uint32_t bar0 = pci_dev.bar[0];
            if (!(bar0 & 0x1)) {  /* Memory BAR */
                uintptr_t mmio_base = (bar0 & ~0xF);
                size_t mmio_size = 128 * 1024;  /* 128KB standard for e1000 */

                /* Map MMIO region */
                mmio_base = (uintptr_t)PHYS_TO_VIRT_DIRECT(mmio_base);

                /* Enable PCI device */
                result = hal_pci_enable_device(&pci_dev);
                if (FAILED(result)) {
                    KLOG_ERROR("E1000", "Failed to enable PCI device");
                    continue;
                }

                /* Initialize the device */
                result = e1000_init_device(mmio_base, mmio_size);
                if (!FAILED(result)) {
                    KLOG_INFO("E1000", "Successfully initialized e1000 device");
                    return STATUS_OK;
                }
            }
        }
    }

    KLOG_WARN("E1000", "No compatible e1000 devices found");
    return STATUS_NOTFOUND;
}

/* Get e1000 device */
e1000_device_t* e1000_get_device(uint8_t index) {
    if (index == 0 && e1000_dev.initialized) {
        return &e1000_dev;
    }
    return NULL;
}

/* Initialize e1000 driver */
status_t e1000_init(void) {
    if (e1000_dev.initialized) {
        return STATUS_EXISTS;
    }

    KLOG_INFO("E1000", "Initializing Intel e1000 Ethernet driver");

    /* Detect devices */
    status_t result = e1000_detect_devices();
    if (FAILED(result)) {
        KLOG_WARN("E1000", "No e1000 devices found");
        return result;
    }

    /* For QEMU testing, we'd set up MMIO base from PCI config */
    /* This is a simplified version */

    e1000_dev.initialized = true;
    e1000_dev.lock = 0;

    KLOG_INFO("E1000", "Driver initialized");
    return STATUS_OK;
}

/* Initialize specific e1000 device (helper for when PCI is implemented) */
status_t e1000_init_device(uintptr_t mmio_base, size_t mmio_size) {
    e1000_device_t* dev = &e1000_dev;

    dev->mmio_base = mmio_base;
    dev->mmio_size = mmio_size;

    /* Reset device */
    status_t result = e1000_reset(dev);
    if (FAILED(result)) {
        return result;
    }

    /* Read MAC address */
    result = e1000_read_mac(dev);
    if (FAILED(result)) {
        return result;
    }

    /* Initialize RX */
    result = e1000_init_rx(dev);
    if (FAILED(result)) {
        return result;
    }

    /* Initialize TX */
    result = e1000_init_tx(dev);
    if (FAILED(result)) {
        return result;
    }

    /* Set link up */
    uint32_t ctrl = e1000_read_reg(dev, E1000_REG_CTRL);
    e1000_write_reg(dev, E1000_REG_CTRL, ctrl | E1000_CTRL_SLU);

    /* Register network interface */
    net_interface_t iface = {0};
    iface.id = 0;

    /* Copy name */
    const char* name = "eth0";
    for (int i = 0; i < 16 && name[i]; i++) {
        iface.name[i] = name[i];
    }

    mac_addr_copy(&iface.mac, &dev->mac);
    iface.send = e1000_send_packet;
    iface.driver_data = dev;
    iface.up = false;

    result = net_register_interface(&iface);
    if (FAILED(result)) {
        return result;
    }

    dev->iface = net_get_interface(0);
    dev->initialized = true;

    KLOG_INFO("E1000", "Device initialized and registered as eth0");
    return STATUS_OK;
}

/* Shutdown e1000 driver */
void e1000_shutdown(void) {
    if (!e1000_dev.initialized) {
        return;
    }

    /* Disable receiver and transmitter */
    if (e1000_dev.mmio_base) {
        e1000_write_reg(&e1000_dev, E1000_REG_RCTL, 0);
        e1000_write_reg(&e1000_dev, E1000_REG_TCTL, 0);
    }

    e1000_dev.initialized = false;
    KLOG_INFO("E1000", "Driver shutdown");
}
