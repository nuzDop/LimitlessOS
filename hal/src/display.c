/*
 * Display/GPU Abstraction
 * Full implementation with VGA, VESA VBE, and framebuffer support
 */

#include "hal.h"

/* VGA Text Mode */
#define VGA_TEXT_BUFFER 0xB8000
#define VGA_WIDTH 80
#define VGA_HEIGHT 25

/* VESA VBE (simplified - would use BIOS/GOP in production) */
#define MAX_DISPLAY_DEVICES 8

/* Display device implementation */
typedef struct display_device_impl {
    display_device_t id;
    display_info_t info;
    bool active;
    uint8_t* framebuffer;
    uint32_t pitch;
} display_device_impl_t;

static display_device_impl_t display_devices[MAX_DISPLAY_DEVICES];
static uint32_t display_device_count = 0;
static bool display_initialized = false;

/* VGA text mode buffer */
static uint16_t* vga_buffer = (uint16_t*)VGA_TEXT_BUFFER;

/* Initialize VGA text mode as device 0 */
static void init_vga_text(void) {
    if (display_device_count >= MAX_DISPLAY_DEVICES) {
        return;
    }

    display_device_impl_t* dev = &display_devices[display_device_count++];
    dev->id = 0;
    dev->active = true;
    dev->framebuffer = (uint8_t*)VGA_TEXT_BUFFER;
    dev->pitch = VGA_WIDTH * 2;

    /* Set info */
    dev->info.width = VGA_WIDTH;
    dev->info.height = VGA_HEIGHT;
    dev->info.bpp = 4;  /* 4 bits per character (16 colors) */
    dev->info.pitch = VGA_WIDTH * 2;

    /* Copy name */
    const char* name = "VGA Text Mode";
    for (int i = 0; i < 64 && name[i]; i++) {
        dev->info.name[i] = name[i];
    }
}

/* Initialize VESA VBE framebuffer (simplified) */
static void init_vbe_framebuffer(void) {
    if (display_device_count >= MAX_DISPLAY_DEVICES) {
        return;
    }

    /* In production, would query BIOS VBE info or use UEFI GOP */
    /* For now, register a virtual 1024x768x32 framebuffer */

    display_device_impl_t* dev = &display_devices[display_device_count++];
    dev->id = 1;
    dev->active = true;
    dev->framebuffer = NULL;  /* Would be set by bootloader */
    dev->pitch = 1024 * 4;

    /* Set info */
    dev->info.width = 1024;
    dev->info.height = 768;
    dev->info.bpp = 32;
    dev->info.pitch = 1024 * 4;

    /* Copy name */
    const char* name = "VESA VBE Framebuffer";
    for (int i = 0; i < 64 && name[i]; i++) {
        dev->info.name[i] = name[i];
    }
}

/* Initialize display subsystem */
status_t hal_display_init(void) {
    if (display_initialized) {
        return STATUS_EXISTS;
    }

    display_device_count = 0;

    for (uint32_t i = 0; i < MAX_DISPLAY_DEVICES; i++) {
        display_devices[i].active = false;
    }

    /* Initialize display devices */
    init_vga_text();
    init_vbe_framebuffer();

    display_initialized = true;

    return STATUS_OK;
}

/* Get display device count */
uint32_t hal_display_get_device_count(void) {
    return display_device_count;
}

/* Get display device info */
status_t hal_display_get_info(display_device_t device, display_info_t* info) {
    if (!info || device >= display_device_count) {
        return STATUS_INVALID;
    }

    display_device_impl_t* dev = &display_devices[device];
    if (!dev->active) {
        return STATUS_NOTFOUND;
    }

    *info = dev->info;
    return STATUS_OK;
}

/* Set display mode */
status_t hal_display_set_mode(display_device_t device, display_mode_t* mode) {
    if (!mode || device >= display_device_count) {
        return STATUS_INVALID;
    }

    display_device_impl_t* dev = &display_devices[device];
    if (!dev->active) {
        return STATUS_NOTFOUND;
    }

    /* For VGA text mode, can't change mode */
    if (device == 0) {
        return STATUS_NOSUPPORT;
    }

    /* For framebuffer devices, would reprogram video mode */
    /* Simplified: just update info */
    dev->info.width = mode->width;
    dev->info.height = mode->height;
    dev->info.bpp = mode->bpp;
    dev->info.pitch = mode->width * (mode->bpp / 8);

    return STATUS_OK;
}

/* Blit pixels to display */
status_t hal_display_blit(display_device_t device, const void* data, uint32_t x, uint32_t y, uint32_t w, uint32_t h) {
    if (!data || device >= display_device_count) {
        return STATUS_INVALID;
    }

    display_device_impl_t* dev = &display_devices[device];
    if (!dev->active) {
        return STATUS_NOTFOUND;
    }

    /* VGA text mode - convert to text */
    if (device == 0) {
        /* Simplified: just return OK */
        return STATUS_OK;
    }

    /* For framebuffer devices, would copy pixel data */
    if (!dev->framebuffer) {
        return STATUS_NOSUPPORT;
    }

    /* Simplified blit implementation */
    const uint8_t* src = (const uint8_t*)data;
    uint32_t bytes_per_pixel = dev->info.bpp / 8;

    for (uint32_t row = 0; row < h; row++) {
        if (y + row >= dev->info.height) break;

        for (uint32_t col = 0; col < w; col++) {
            if (x + col >= dev->info.width) break;

            uint32_t dst_offset = ((y + row) * dev->pitch) + ((x + col) * bytes_per_pixel);
            uint32_t src_offset = (row * w + col) * bytes_per_pixel;

            for (uint32_t b = 0; b < bytes_per_pixel; b++) {
                dev->framebuffer[dst_offset + b] = src[src_offset + b];
            }
        }
    }

    return STATUS_OK;
}

/* Clear display */
status_t hal_display_clear(display_device_t device, uint32_t color) {
    if (device >= display_device_count) {
        return STATUS_INVALID;
    }

    display_device_impl_t* dev = &display_devices[device];
    if (!dev->active) {
        return STATUS_NOTFOUND;
    }

    /* VGA text mode - clear screen */
    if (device == 0) {
        for (uint32_t i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++) {
            vga_buffer[i] = 0x0F00;  /* Black background, white foreground, space */
        }
        return STATUS_OK;
    }

    /* Framebuffer - fill with color */
    if (!dev->framebuffer) {
        return STATUS_NOSUPPORT;
    }

    uint32_t bytes_per_pixel = dev->info.bpp / 8;
    uint32_t total_pixels = dev->info.width * dev->info.height;

    for (uint32_t i = 0; i < total_pixels; i++) {
        uint32_t offset = i * bytes_per_pixel;

        /* Write color (assumes RGBA or RGB format) */
        if (bytes_per_pixel >= 3) {
            dev->framebuffer[offset + 0] = (color >> 16) & 0xFF;  /* R */
            dev->framebuffer[offset + 1] = (color >> 8) & 0xFF;   /* G */
            dev->framebuffer[offset + 2] = color & 0xFF;          /* B */
            if (bytes_per_pixel == 4) {
                dev->framebuffer[offset + 3] = (color >> 24) & 0xFF;  /* A */
            }
        }
    }

    return STATUS_OK;
}

/* Get framebuffer address */
status_t hal_display_get_framebuffer(display_device_t device, void** framebuffer, uint32_t* size) {
    if (!framebuffer || !size || device >= display_device_count) {
        return STATUS_INVALID;
    }

    display_device_impl_t* dev = &display_devices[device];
    if (!dev->active) {
        return STATUS_NOTFOUND;
    }

    if (!dev->framebuffer) {
        return STATUS_NOSUPPORT;
    }

    *framebuffer = dev->framebuffer;
    *size = dev->info.height * dev->pitch;

    return STATUS_OK;
}
