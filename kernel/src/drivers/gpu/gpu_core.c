/*
 * GPU Core Driver Framework - Phase 1 Stub
 * Unified GPU driver interface for Intel, AMD, and NVIDIA GPUs
 * 
 * Supports:
 * - Mode setting (resolution, refresh rate)
 * - Framebuffer management
 * - Hardware acceleration (2D/3D)
 * - Multi-monitor support
 * - Hot-plug detection
 */

#include "kernel.h"

/* GPU Vendor IDs */
#define GPU_VENDOR_INTEL    0x8086
#define GPU_VENDOR_AMD      0x1002
#define GPU_VENDOR_NVIDIA   0x10DE

/* GPU Types */
typedef enum {
    GPU_TYPE_INTEGRATED,    // Integrated GPU (Intel HD, AMD APU)
    GPU_TYPE_DISCRETE,      // Discrete GPU (dedicated card)
    GPU_TYPE_VIRTUAL        // Virtual GPU (QEMU, VirtualBox)
} gpu_type_t;

/* Display Mode */
typedef struct gpu_mode {
    uint32_t width;
    uint32_t height;
    uint32_t refresh_rate;      // Hz
    uint32_t bits_per_pixel;
    uint32_t stride;            // Bytes per scanline
} gpu_mode_t;

/* Framebuffer Info */
typedef struct gpu_framebuffer {
    paddr_t physical_address;
    vaddr_t virtual_address;
    uint32_t size;              // Bytes
    gpu_mode_t mode;
} gpu_framebuffer_t;

/* GPU Device */
typedef struct gpu_device {
    uint32_t id;
    uint16_t vendor_id;
    uint16_t device_id;
    gpu_type_t type;
    char name[64];
    
    /* Framebuffer */
    gpu_framebuffer_t framebuffer;
    
    /* Supported modes */
    gpu_mode_t* modes;
    uint32_t mode_count;
    
    /* Current state */
    bool enabled;
    gpu_mode_t current_mode;
    
    /* Hardware info */
    uint64_t vram_size;         // VRAM size in bytes
    uint32_t pci_bus;
    uint32_t pci_device;
    uint32_t pci_function;
    
    /* Driver callbacks */
    status_t (*init)(struct gpu_device*);
    status_t (*set_mode)(struct gpu_device*, gpu_mode_t*);
    status_t (*blit)(struct gpu_device*, void* src, uint32_t x, uint32_t y,
                     uint32_t w, uint32_t h);
    status_t (*fill)(struct gpu_device*, uint32_t color, uint32_t x, uint32_t y,
                     uint32_t w, uint32_t h);
} gpu_device_t;

/* Global GPU state */
static struct {
    bool initialized;
    gpu_device_t devices[4];    // Support up to 4 GPUs
    uint32_t device_count;
    gpu_device_t* primary;      // Primary display GPU
    uint32_t lock;
} gpu_state = {0};

/* Initialize GPU subsystem */
status_t gpu_init(void) {
    if (gpu_state.initialized) {
        return STATUS_EXISTS;
    }
    
    gpu_state.device_count = 0;
    gpu_state.primary = NULL;
    gpu_state.lock = 0;
    gpu_state.initialized = true;
    
    KLOG_INFO("GPU", "GPU subsystem initialized");
    
    /* TODO Phase 1: Enumerate GPUs */
    /* - Scan PCI for VGA controllers (class 0x03) */
    /* - Detect vendor (Intel, AMD, NVIDIA) */
    /* - Load appropriate driver */
    /* - Set up framebuffer */
    
    return STATUS_OK;
}

/* Register GPU device */
status_t gpu_register_device(gpu_device_t* device) {
    if (!gpu_state.initialized || !device) {
        return STATUS_INVALID;
    }
    
    if (gpu_state.device_count >= 4) {
        return STATUS_NOMEM;
    }
    
    gpu_state.devices[gpu_state.device_count] = *device;
    gpu_state.device_count++;
    
    /* Set as primary if first device */
    if (!gpu_state.primary) {
        gpu_state.primary = &gpu_state.devices[0];
    }
    
    KLOG_INFO("GPU", "Registered GPU: %s (vendor=0x%04x device=0x%04x)",
              device->name, device->vendor_id, device->device_id);
    
    return STATUS_OK;
}

/* Set display mode */
status_t gpu_set_mode(gpu_device_t* device, uint32_t width, uint32_t height,
                      uint32_t refresh_rate) {
    if (!gpu_state.initialized || !device) {
        return STATUS_INVALID;
    }
    
    /* Find matching mode */
    gpu_mode_t target_mode = {
        .width = width,
        .height = height,
        .refresh_rate = refresh_rate,
        .bits_per_pixel = 32
    };
    
    /* Call driver set_mode if available */
    if (device->set_mode) {
        return device->set_mode(device, &target_mode);
    }
    
    KLOG_WARN("GPU", "set_mode not implemented for device %s", device->name);
    return STATUS_NOSUPPORT;
}

/* Blit (copy) image to framebuffer */
status_t gpu_blit(gpu_device_t* device, void* src, uint32_t x, uint32_t y,
                  uint32_t width, uint32_t height) {
    if (!gpu_state.initialized || !device || !src) {
        return STATUS_INVALID;
    }
    
    if (device->blit) {
        return device->blit(device, src, x, y, width, height);
    }
    
    /* Fallback: Software blit */
    uint32_t* framebuffer = (uint32_t*)device->framebuffer.virtual_address;
    uint32_t* source = (uint32_t*)src;
    
    for (uint32_t j = 0; j < height; j++) {
        for (uint32_t i = 0; i < width; i++) {
            uint32_t fb_offset = (y + j) * device->current_mode.width + (x + i);
            uint32_t src_offset = j * width + i;
            framebuffer[fb_offset] = source[src_offset];
        }
    }
    
    return STATUS_OK;
}

/* Fill rectangle with color */
status_t gpu_fill(gpu_device_t* device, uint32_t color, uint32_t x, uint32_t y,
                  uint32_t width, uint32_t height) {
    if (!gpu_state.initialized || !device) {
        return STATUS_INVALID;
    }
    
    if (device->fill) {
        return device->fill(device, color, x, y, width, height);
    }
    
    /* Fallback: Software fill */
    uint32_t* framebuffer = (uint32_t*)device->framebuffer.virtual_address;
    
    for (uint32_t j = 0; j < height; j++) {
        for (uint32_t i = 0; i < width; i++) {
            uint32_t offset = (y + j) * device->current_mode.width + (x + i);
            framebuffer[offset] = color;
        }
    }
    
    return STATUS_OK;
}

/* Get primary GPU */
gpu_device_t* gpu_get_primary(void) {
    if (!gpu_state.initialized) {
        return NULL;
    }
    return gpu_state.primary;
}

/* Get GPU by index */
gpu_device_t* gpu_get_device(uint32_t index) {
    if (!gpu_state.initialized || index >= gpu_state.device_count) {
        return NULL;
    }
    return &gpu_state.devices[index];
}

/* Get GPU count */
uint32_t gpu_get_count(void) {
    if (!gpu_state.initialized) {
        return 0;
    }
    return gpu_state.device_count;
}

/* TODO Phase 1: Vendor-specific driver initialization stubs */

/* Intel GPU initialization */
status_t intel_gpu_init(gpu_device_t* device) {
    KLOG_INFO("GPU", "Intel GPU detected: 0x%04x", device->device_id);
    /* TODO: Initialize Intel HD Graphics / Iris / Xe */
    return STATUS_OK;
}

/* AMD GPU initialization */
status_t amd_gpu_init(gpu_device_t* device) {
    KLOG_INFO("GPU", "AMD GPU detected: 0x%04x", device->device_id);
    /* TODO: Initialize AMDGPU (GCN/RDNA) */
    return STATUS_OK;
}

/* NVIDIA GPU initialization */
status_t nvidia_gpu_init(gpu_device_t* device) {
    KLOG_INFO("GPU", "NVIDIA GPU detected: 0x%04x", device->device_id);
    /* TODO: Initialize Nouveau (open-source) or proprietary driver */
    return STATUS_OK;
}
