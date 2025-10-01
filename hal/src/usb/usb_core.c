/*
 * USB Core Subsystem - Phase 1 Stub
 * Universal Serial Bus stack for LimitlessOS
 * 
 * Supports:
 * - USB 1.1 (UHCI/OHCI)
 * - USB 2.0 (EHCI)
 * - USB 3.0/3.1/3.2 (XHCI)
 * - USB-C with Power Delivery
 * - Hot-plug detection
 * - Device enumeration
 */

#include "hal.h"

/* USB Speeds */
typedef enum {
    USB_SPEED_LOW = 0,      // 1.5 Mbps (USB 1.0)
    USB_SPEED_FULL = 1,     // 12 Mbps (USB 1.1)
    USB_SPEED_HIGH = 2,     // 480 Mbps (USB 2.0)
    USB_SPEED_SUPER = 3,    // 5 Gbps (USB 3.0)
    USB_SPEED_SUPER_PLUS = 4, // 10 Gbps (USB 3.1)
    USB_SPEED_SUPER_PLUS_GEN2 = 5 // 20 Gbps (USB 3.2)
} usb_speed_t;

/* USB Device States */
typedef enum {
    USB_STATE_DETACHED = 0,
    USB_STATE_ATTACHED,
    USB_STATE_POWERED,
    USB_STATE_DEFAULT,
    USB_STATE_ADDRESS,
    USB_STATE_CONFIGURED,
    USB_STATE_SUSPENDED
} usb_device_state_t;

/* USB Transfer Types */
typedef enum {
    USB_TRANSFER_CONTROL = 0,
    USB_TRANSFER_ISOCHRONOUS = 1,
    USB_TRANSFER_BULK = 2,
    USB_TRANSFER_INTERRUPT = 3
} usb_transfer_type_t;

/* USB Device Descriptor */
typedef struct usb_device_descriptor {
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint16_t bcdUSB;
    uint8_t bDeviceClass;
    uint8_t bDeviceSubClass;
    uint8_t bDeviceProtocol;
    uint8_t bMaxPacketSize0;
    uint16_t idVendor;
    uint16_t idProduct;
    uint16_t bcdDevice;
    uint8_t iManufacturer;
    uint8_t iProduct;
    uint8_t iSerialNumber;
    uint8_t bNumConfigurations;
} __attribute__((packed)) usb_device_descriptor_t;

/* USB Device */
typedef struct usb_device {
    uint32_t id;                    // Device ID
    uint8_t address;                // USB address (1-127)
    usb_speed_t speed;              // Device speed
    usb_device_state_t state;       // Current state
    usb_device_descriptor_t descriptor; // Device descriptor
    void* controller;               // Controller handle
    uint8_t port;                   // Hub port number
    struct usb_device* parent;      // Parent hub (NULL for root)
    bool attached;
} usb_device_t;

/* USB Request Block (URB) */
typedef struct usb_urb {
    usb_device_t* device;
    usb_transfer_type_t transfer_type;
    uint8_t endpoint;
    void* buffer;
    uint32_t length;
    uint32_t actual_length;
    int status;
    void (*callback)(struct usb_urb*);
    void* context;
} usb_urb_t;

/* Global USB state */
static struct {
    bool initialized;
    uint32_t device_count;
    usb_device_t devices[128];  // Max 128 USB devices
    uint32_t lock;
} usb_state = {0};

/* Initialize USB subsystem */
status_t usb_init(void) {
    if (usb_state.initialized) {
        return STATUS_EXISTS;
    }
    
    usb_state.device_count = 0;
    usb_state.lock = 0;
    usb_state.initialized = true;
    
    hal_log(HAL_LOG_INFO, "USB", "USB subsystem initialized (stub)");
    
    /* TODO Phase 1: Initialize USB controllers */
    /* - Detect XHCI/EHCI/UHCI controllers via PCI */
    /* - Initialize controller hardware */
    /* - Set up root hub */
    
    return STATUS_OK;
}

/* Enumerate USB device */
status_t usb_enumerate_device(usb_device_t* device) {
    if (!usb_state.initialized || !device) {
        return STATUS_INVALID;
    }
    
    /* TODO Phase 1: Device enumeration */
    /* 1. Get device descriptor */
    /* 2. Assign USB address */
    /* 3. Get configuration descriptors */
    /* 4. Select configuration */
    /* 5. Load appropriate class driver */
    
    hal_log(HAL_LOG_INFO, "USB", "Device enumerated: VID=0x%04x PID=0x%04x",
            device->descriptor.idVendor, device->descriptor.idProduct);
    
    return STATUS_OK;
}

/* Submit USB request */
status_t usb_submit_urb(usb_urb_t* urb) {
    if (!usb_state.initialized || !urb) {
        return STATUS_INVALID;
    }
    
    /* TODO Phase 1: URB submission */
    /* - Validate URB */
    /* - Queue to appropriate controller */
    /* - Schedule transfer */
    
    return STATUS_OK;
}

/* Cancel USB request */
status_t usb_cancel_urb(usb_urb_t* urb) {
    if (!usb_state.initialized || !urb) {
        return STATUS_INVALID;
    }
    
    /* TODO Phase 1: URB cancellation */
    
    return STATUS_OK;
}

/* USB hot-plug detection */
void usb_handle_hotplug(uint8_t port, bool attached) {
    if (!usb_state.initialized) {
        return;
    }
    
    if (attached) {
        hal_log(HAL_LOG_INFO, "USB", "Device attached on port %d", port);
        
        /* TODO Phase 1: Handle device attachment */
        /* - Reset port */
        /* - Enumerate device */
        /* - Load driver */
    } else {
        hal_log(HAL_LOG_INFO, "USB", "Device detached from port %d", port);
        
        /* TODO Phase 1: Handle device detachment */
        /* - Notify driver */
        /* - Clean up resources */
        /* - Remove from device list */
    }
}

/* Get USB device by address */
usb_device_t* usb_get_device(uint8_t address) {
    if (!usb_state.initialized) {
        return NULL;
    }
    
    for (uint32_t i = 0; i < usb_state.device_count; i++) {
        if (usb_state.devices[i].address == address) {
            return &usb_state.devices[i];
        }
    }
    
    return NULL;
}

/* USB class driver interface stubs */

/* HID (Human Interface Device) class */
status_t usb_hid_init(usb_device_t* device) {
    hal_log(HAL_LOG_INFO, "USB", "HID device detected");
    /* TODO Phase 1: Initialize HID device (keyboard, mouse, gamepad) */
    return STATUS_OK;
}

/* Mass Storage class */
status_t usb_storage_init(usb_device_t* device) {
    hal_log(HAL_LOG_INFO, "USB", "Mass storage device detected");
    /* TODO Phase 1: Initialize USB storage (flash drives, external HDDs) */
    return STATUS_OK;
}

/* Audio class */
status_t usb_audio_init(usb_device_t* device) {
    hal_log(HAL_LOG_INFO, "USB", "Audio device detected");
    /* TODO Phase 1: Initialize USB audio device */
    return STATUS_OK;
}

/* CDC (Communications Device Class) - for USB networking */
status_t usb_cdc_init(usb_device_t* device) {
    hal_log(HAL_LOG_INFO, "USB", "CDC device detected");
    /* TODO Phase 1: Initialize USB network adapter */
    return STATUS_OK;
}
