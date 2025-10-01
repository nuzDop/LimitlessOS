# LimitlessOS Driver Framework

## Overview

LimitlessOS uses a **microkernel architecture** with **user-space drivers**. This document describes the driver framework, including the Hardware Abstraction Layer (HAL), driver loading, and device management.

## Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                      Applications                                │
└──────────────────────────┬──────────────────────────────────────┘
                           │ Standard APIs (POSIX, Win32, Cocoa)
┌──────────────────────────┴──────────────────────────────────────┐
│                     User-Space Drivers                           │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────┐        │
│  │  USB     │  │  GPU     │  │  WiFi    │  │  Storage │        │
│  │  Driver  │  │  Driver  │  │  Driver  │  │  Driver  │        │
│  └────┬─────┘  └────┬─────┘  └────┬─────┘  └────┬─────┘        │
└───────┼─────────────┼─────────────┼─────────────┼───────────────┘
        │             │             │             │
        │             IPC (Message Passing)       │
        │             │             │             │
┌───────┴─────────────┴─────────────┴─────────────┴───────────────┐
│            Hardware Abstraction Layer (HAL)                      │
│  ┌─────────────────────────────────────────────────────────┐    │
│  │  Unified Interface (CPU, Storage, Network, Display...)  │    │
│  └─────────────────────────────────────────────────────────┘    │
└──────────────────────────┬──────────────────────────────────────┘
        │             │             │             │
┌───────┴─────────────┴─────────────┴─────────────┴───────────────┐
│                    Microkernel                                   │
│  • IPC          • Scheduling     • Memory Management            │
│  • Capabilities • Interrupts     • Basic syscalls               │
└──────────────────────────┬──────────────────────────────────────┘
                           │
┌──────────────────────────┴──────────────────────────────────────┐
│                         Hardware                                 │
│  • x86_64   • ARM64   • RISC-V  (future)                        │
└─────────────────────────────────────────────────────────────────┘
```

## Design Principles

1. **Microkernel First**: All drivers run in user space (except essential boot drivers)
2. **Capability-Based**: Fine-grained permissions for hardware access
3. **Modular**: Drivers are loadable/unloadable at runtime
4. **Hot-Plug**: Dynamic device discovery and removal
5. **AI-Friendly**: Hooks for AI-driven optimization
6. **Cross-Platform**: HAL abstracts architecture differences

## Hardware Abstraction Layer (HAL)

### HAL Interface

The HAL provides unified contracts for hardware access:

```c
/* CPU Management */
uint32_t hal_cpu_count(void);
status_t hal_cpu_info(uint32_t cpu_id, cpu_info_t* info);
void hal_cpu_enable_interrupts(void);
void hal_cpu_disable_interrupts(void);

/* Storage */
status_t hal_storage_read(device_t device, uint64_t lba, 
                          void* buffer, uint32_t count);
status_t hal_storage_write(device_t device, uint64_t lba, 
                           const void* buffer, uint32_t count);

/* Network */
status_t hal_network_send(device_t device, const void* packet, size_t size);
status_t hal_network_receive(device_t device, void* buffer, size_t* size);

/* Display */
status_t hal_display_set_mode(device_t device, display_mode_t* mode);
status_t hal_display_blit(device_t device, const void* data, 
                          uint32_t x, uint32_t y, uint32_t w, uint32_t h);

/* Timer */
uint64_t hal_timer_get_ticks(void);
status_t hal_timer_oneshot(uint64_t ns, void (*callback)(void*), void* context);
```

### HAL Logging

```c
void hal_log(hal_log_level_t level, const char* subsystem, 
             const char* format, ...);

// Levels: HAL_LOG_DEBUG, HAL_LOG_INFO, HAL_LOG_WARN, HAL_LOG_ERROR
```

## Driver Model

### Driver Structure

Every driver implements a standard interface:

```c
typedef struct driver {
    char name[64];              // Driver name
    char version[16];           // Version string
    uint32_t type;              // Driver type (DRIVER_TYPE_USB, etc.)
    
    /* Lifecycle */
    status_t (*init)(struct driver*);
    status_t (*shutdown)(struct driver*);
    
    /* Device operations */
    status_t (*probe)(struct driver*, device_t device);
    status_t (*attach)(struct driver*, device_t device);
    status_t (*detach)(struct driver*, device_t device);
    
    /* I/O operations */
    ssize_t (*read)(struct driver*, device_t, void*, size_t);
    ssize_t (*write)(struct driver*, device_t, const void*, size_t);
    status_t (*ioctl)(struct driver*, device_t, uint32_t cmd, void* arg);
    
    /* Interrupt handling */
    void (*interrupt)(struct driver*, uint32_t irq);
    
    /* Power management */
    status_t (*suspend)(struct driver*);
    status_t (*resume)(struct driver*);
    
    void* private_data;         // Driver-specific data
} driver_t;
```

### Driver Types

```c
typedef enum {
    DRIVER_TYPE_USB,
    DRIVER_TYPE_PCI,
    DRIVER_TYPE_SATA,
    DRIVER_TYPE_NVME,
    DRIVER_TYPE_GPU,
    DRIVER_TYPE_NETWORK,
    DRIVER_TYPE_AUDIO,
    DRIVER_TYPE_INPUT,
    DRIVER_TYPE_DISPLAY,
} driver_type_t;
```

### Driver Registration

```c
/* Register driver with kernel */
status_t driver_register(driver_t* driver);

/* Unregister driver */
status_t driver_unregister(driver_t* driver);

/* Load driver from file */
status_t driver_load(const char* path);
```

## USB Driver Framework

### USB Architecture

```
Application Layer
    ↓
USB Class Drivers (HID, Mass Storage, Audio, CDC)
    ↓
USB Core (Device Enumeration, URB Management)
    ↓
USB Host Controller Drivers (XHCI, EHCI, UHCI)
    ↓
Hardware
```

### USB Device Enumeration

1. **Device Attachment**: Port status change interrupt
2. **Port Reset**: Reset port and determine speed
3. **Get Device Descriptor**: Request descriptor at address 0
4. **Assign Address**: Set device address (1-127)
5. **Get Full Descriptor**: Request full device descriptor
6. **Get Configuration**: Request configuration descriptor
7. **Set Configuration**: Activate configuration
8. **Load Class Driver**: Load appropriate class driver (HID, storage, etc.)

### USB Transfer Types

- **Control**: Configuration, commands (bidirectional, reliable)
- **Bulk**: Large data transfers (unidirectional, reliable)
- **Interrupt**: Periodic data (keyboards, mice)
- **Isochronous**: Time-sensitive data (audio, video, no retries)

### USB Class Drivers

#### HID (Human Interface Device)

```c
/* Keyboards, mice, gamepads, joysticks */
status_t usb_hid_init(usb_device_t* device);
status_t usb_hid_read_report(usb_device_t* device, void* report, size_t size);
```

#### Mass Storage

```c
/* Flash drives, external HDDs */
status_t usb_storage_init(usb_device_t* device);
status_t usb_storage_read(usb_device_t* device, uint64_t lba, 
                          void* buffer, uint32_t count);
status_t usb_storage_write(usb_device_t* device, uint64_t lba, 
                           const void* buffer, uint32_t count);
```

#### Audio

```c
/* USB headsets, speakers, microphones */
status_t usb_audio_init(usb_device_t* device);
status_t usb_audio_play(usb_device_t* device, const void* samples, size_t count);
status_t usb_audio_record(usb_device_t* device, void* samples, size_t count);
```

#### CDC (Communications Device Class)

```c
/* USB network adapters, modems */
status_t usb_cdc_init(usb_device_t* device);
status_t usb_cdc_send(usb_device_t* device, const void* packet, size_t size);
status_t usb_cdc_receive(usb_device_t* device, void* packet, size_t* size);
```

### XHCI (USB 3.x) Controller

XHCI supports:
- **SuperSpeed** (USB 3.0): 5 Gbps
- **SuperSpeed+** (USB 3.1): 10 Gbps
- **SuperSpeed+ Gen 2** (USB 3.2): 20 Gbps

Key components:
- **Command Ring**: Send commands to controller
- **Event Ring**: Receive events (completions, errors)
- **Transfer Rings**: Queue data transfers
- **Device Context**: Per-device state
- **Doorbell Array**: Signal new work to controller

## GPU Driver Framework

### GPU Architecture

```
Application (OpenGL, Vulkan, Direct3D)
    ↓
Graphics API Translation Layer
    ↓
GPU Core Driver (Mode Setting, Command Submission)
    ↓
Vendor Driver (Intel, AMD, NVIDIA)
    ↓
Hardware
```

### GPU Device Management

```c
typedef struct gpu_device {
    uint32_t id;
    uint16_t vendor_id;         // GPU_VENDOR_INTEL, AMD, NVIDIA
    uint16_t device_id;
    gpu_type_t type;            // INTEGRATED or DISCRETE
    
    /* Framebuffer */
    gpu_framebuffer_t framebuffer;
    
    /* Capabilities */
    uint64_t vram_size;
    gpu_mode_t* modes;
    uint32_t mode_count;
    
    /* Operations */
    status_t (*set_mode)(gpu_device_t*, gpu_mode_t*);
    status_t (*blit)(gpu_device_t*, void* src, uint32_t x, uint32_t y, ...);
    status_t (*fill)(gpu_device_t*, uint32_t color, uint32_t x, ...);
} gpu_device_t;
```

### Display Modes

```c
typedef struct gpu_mode {
    uint32_t width;             // e.g., 1920
    uint32_t height;            // e.g., 1080
    uint32_t refresh_rate;      // e.g., 60 Hz
    uint32_t bits_per_pixel;    // e.g., 32 (RGBA)
    uint32_t stride;            // Bytes per scanline
} gpu_mode_t;
```

### Multi-Monitor Support

```c
/* Get GPU count */
uint32_t gpu_get_count(void);

/* Get GPU by index */
gpu_device_t* gpu_get_device(uint32_t index);

/* Get primary GPU */
gpu_device_t* gpu_get_primary(void);
```

### Vendor-Specific Drivers

#### Intel GPU

- **Intel HD Graphics** (Gen 7-9)
- **Intel Iris** (Gen 9+)
- **Intel Xe** (Gen 12+)

Features: Integrated, power-efficient, good for general use

#### AMD GPU

- **GCN** (Graphics Core Next)
- **RDNA** (1st/2nd gen)
- **RDNA 3** (latest)

Features: Good price/performance, open-source friendly

#### NVIDIA GPU

- **Pascal** (GTX 10xx)
- **Turing** (RTX 20xx)
- **Ampere** (RTX 30xx)
- **Ada Lovelace** (RTX 40xx)

Features: Best gaming performance, CUDA support

## WiFi Driver Framework

### WiFi Architecture

```
Network Stack (TCP/IP)
    ↓
WiFi Core (Scanning, Connection Management)
    ↓
WiFi Driver (Chipset-Specific)
    ↓
Hardware
```

### WiFi Device Management

```c
typedef struct wifi_device {
    uint32_t id;
    uint16_t vendor_id;
    uint16_t device_id;
    uint8_t mac_address[6];
    
    /* Capabilities */
    wifi_standard_t max_standard;   // 802.11ax (WiFi 6)
    bool dual_band;                 // 2.4 + 5 GHz
    bool tri_band;                  // 2.4 + 5 + 6 GHz
    
    /* State */
    wifi_state_t state;
    wifi_network_t* connected_network;
    wifi_network_t* scan_results;
    uint32_t scan_count;
    
    /* Operations */
    status_t (*scan)(wifi_device_t*);
    status_t (*connect)(wifi_device_t*, wifi_network_t*, const char* password);
    status_t (*disconnect)(wifi_device_t*);
    status_t (*send)(wifi_device_t*, void* packet, uint32_t length);
} wifi_device_t;
```

### WiFi Standards

- **802.11a**: 5 GHz, 54 Mbps
- **802.11b**: 2.4 GHz, 11 Mbps
- **802.11g**: 2.4 GHz, 54 Mbps
- **802.11n** (WiFi 4): 2.4/5 GHz, 600 Mbps
- **802.11ac** (WiFi 5): 5 GHz, 3.5 Gbps
- **802.11ax** (WiFi 6/6E): 2.4/5/6 GHz, 9.6 Gbps

### WiFi Security

```c
typedef enum {
    WIFI_SEC_NONE,              // Open network
    WIFI_SEC_WEP,               // Deprecated
    WIFI_SEC_WPA_PSK,           // WPA with pre-shared key
    WIFI_SEC_WPA2_PSK,          // WPA2 with PSK
    WIFI_SEC_WPA3_PSK,          // WPA3 with PSK (latest)
    WIFI_SEC_WPA2_ENTERPRISE,   // 802.1X authentication
    WIFI_SEC_WPA3_ENTERPRISE,
} wifi_security_t;
```

### WiFi Scanning

```c
/* Scan for networks */
status_t wifi_scan(wifi_device_t* device);

/* Results available in device->scan_results */
for (uint32_t i = 0; i < device->scan_count; i++) {
    wifi_network_t* network = &device->scan_results[i];
    printf("SSID: %s, RSSI: %d dBm, Security: %d\n",
           network->ssid, network->rssi, network->security);
}
```

### WiFi Connection

```c
/* Connect to network */
status_t wifi_connect(wifi_device_t* device, 
                      const char* ssid, 
                      const char* password);

/* Disconnect */
status_t wifi_disconnect(wifi_device_t* device);
```

### Chipset Support

#### Intel WiFi (iwlwifi)

- AX200, AX201, AX210, AX211
- WiFi 6/6E support
- Best Linux support

#### Realtek WiFi (rtw88/rtw89)

- RTL8821C, RTL8852AE, RTL8852BE
- Budget-friendly
- Good compatibility

#### Broadcom WiFi (brcmfmac)

- BCM4350, BCM4356, BCM4378
- Common in laptops
- Requires firmware

## Device Discovery and Hot-Plug

### PCI Enumeration

```c
/* Enumerate PCI devices */
status_t pci_enumerate(void);

/* Scan for device class */
status_t pci_scan_class(uint8_t class, uint8_t subclass, 
                        pci_device_t** devices, uint32_t* count);

/* Examples */
pci_scan_class(0x03, 0x00, ...);  // VGA controllers (GPUs)
pci_scan_class(0x02, 0x80, ...);  // Network controllers (WiFi)
pci_scan_class(0x0C, 0x03, ...);  // USB controllers
```

### Hot-Plug Detection

```c
/* USB hot-plug */
void usb_handle_hotplug(uint8_t port, bool attached);

/* Display hot-plug (DisplayPort, HDMI) */
void gpu_handle_hotplug(uint32_t connector_id, bool attached);

/* Thunderbolt hot-plug */
void thunderbolt_handle_hotplug(uint32_t port, bool attached);
```

## Interrupt Handling

### Registering IRQ Handler

```c
/* Register handler */
status_t irq_register(irq_t irq, irq_handler_t handler, void* context);

/* Handler signature */
typedef void (*irq_handler_t)(irq_t irq, void* context);

/* Example */
void usb_interrupt_handler(irq_t irq, void* context) {
    xhci_controller_t* ctrl = (xhci_controller_t*)context;
    xhci_interrupt_handler(ctrl);
}

irq_register(IRQ_USB, usb_interrupt_handler, controller);
```

### Message-Signaled Interrupts (MSI/MSI-X)

Modern devices use MSI/MSI-X for better performance:

```c
/* Enable MSI-X */
status_t pci_enable_msix(pci_device_t* device, uint32_t vector_count);

/* Get MSI-X vector */
uint32_t pci_get_msix_vector(pci_device_t* device, uint32_t index);
```

## Driver Development Guide

### Creating a New Driver

1. **Define driver structure**:
```c
static driver_t my_driver = {
    .name = "My Device Driver",
    .version = "1.0.0",
    .type = DRIVER_TYPE_USB,
    .init = my_driver_init,
    .shutdown = my_driver_shutdown,
    .probe = my_driver_probe,
    ...
};
```

2. **Implement lifecycle functions**:
```c
status_t my_driver_init(driver_t* driver) {
    // Initialize driver
    return STATUS_OK;
}

status_t my_driver_shutdown(driver_t* driver) {
    // Clean up resources
    return STATUS_OK;
}
```

3. **Implement device operations**:
```c
status_t my_driver_probe(driver_t* driver, device_t device) {
    // Check if we support this device
    return STATUS_OK;
}

ssize_t my_driver_read(driver_t* driver, device_t device, 
                       void* buffer, size_t size) {
    // Read from device
    return bytes_read;
}
```

4. **Register driver**:
```c
status_t driver_module_init(void) {
    return driver_register(&my_driver);
}
```

### Best Practices

1. **Use HAL abstractions**: Don't access hardware directly
2. **Handle errors gracefully**: Check return values
3. **Log appropriately**: Use hal_log for debugging
4. **Be hot-plug aware**: Handle device removal
5. **Implement power management**: Support suspend/resume
6. **AI integration**: Add AI hooks for optimization
7. **Test thoroughly**: Use QEMU for testing

## Future Enhancements

### Phase 2-5 Roadmap

1. **Complete USB Stack**:
   - Full XHCI/EHCI/UHCI implementation
   - All USB class drivers
   - USB-C and Power Delivery

2. **Complete GPU Drivers**:
   - Full Intel HD/Iris/Xe support
   - AMDGPU with GCN/RDNA
   - Nouveau with basic 3D

3. **Complete WiFi Drivers**:
   - Intel iwlwifi (AX200, AX210)
   - Realtek rtw88/rtw89
   - Broadcom brcmfmac

4. **Additional Drivers**:
   - Bluetooth (USB and PCIe)
   - Thunderbolt 3/4
   - NVMe over Fabrics
   - RAID controllers
   - Sound cards (Intel HDA, USB Audio)

5. **Driver Hot-Reload**:
   - Update drivers without reboot
   - Live patching
   - A/B driver versions

## Conclusion

LimitlessOS's driver framework combines the best of microkernel architecture (user-space drivers, isolation) with practical engineering (HAL abstraction, hot-plug, AI integration). The modular design allows rapid driver development while maintaining system stability and security.

---

*For implementation details, see:*
- *USB: `hal/src/usb/usb_core.c`, `hal/src/usb/xhci.c`*
- *GPU: `kernel/src/drivers/gpu/gpu_core.c`*
- *WiFi: `kernel/src/drivers/wifi/wifi_core.c`*
