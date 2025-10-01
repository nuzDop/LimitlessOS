#ifndef LIMITLESS_HAL_H
#define LIMITLESS_HAL_H

#include "kernel.h"

/*
 * Hardware Abstraction Layer (HAL)
 *
 * Provides unified interface for:
 * - Storage devices
 * - Network interfaces
 * - Display/GPU
 * - Audio
 * - Input devices
 * - CPU management
 * - Timers and clocks
 */

/* ============================================================================
 * HAL Initialization
 * ============================================================================ */

status_t hal_init(void);
status_t hal_shutdown(void);

/* ============================================================================
 * CPU Management
 * ============================================================================ */

typedef struct cpu_info {
    uint32_t id;
    char vendor[32];
    char model[64];
    uint32_t family;
    uint32_t model_id;
    uint32_t stepping;
    uint64_t frequency_mhz;
    uint32_t core_count;
    uint32_t thread_count;
    bool has_sse;
    bool has_sse2;
    bool has_avx;
    bool has_avx2;
    bool has_avx512;
} cpu_info_t;

typedef struct cpu_context {
    uint64_t rax, rbx, rcx, rdx;
    uint64_t rsi, rdi, rbp, rsp;
    uint64_t r8, r9, r10, r11;
    uint64_t r12, r13, r14, r15;
    uint64_t rip;
    uint64_t rflags;
    uint64_t cr3;
} ALIGNED(16) cpu_context_t;

status_t hal_cpu_init(void);
uint32_t hal_cpu_count(void);
status_t hal_cpu_info(uint32_t cpu_id, cpu_info_t* info);
void hal_cpu_enable_interrupts(void);
void hal_cpu_disable_interrupts(void);
void hal_cpu_halt(void);
uint64_t hal_cpu_read_timestamp(void);

/* ============================================================================
 * Timer and Clock
 * ============================================================================ */

typedef struct timer_info {
    const char* name;
    uint64_t frequency_hz;
    bool is_periodic;
    bool is_oneshot;
} timer_info_t;

typedef void (*timer_callback_t)(void* context);

status_t hal_timer_init(void);
uint64_t hal_timer_get_ticks(void);
uint64_t hal_timer_get_frequency(void);
uint64_t hal_timer_ticks_to_ns(uint64_t ticks);
uint64_t hal_timer_ns_to_ticks(uint64_t ns);
status_t hal_timer_oneshot(uint64_t ns, timer_callback_t callback, void* context);
status_t hal_timer_periodic(uint64_t ns, timer_callback_t callback, void* context);

/* ============================================================================
 * Storage
 * ============================================================================ */

typedef uint64_t storage_device_t;

#define STORAGE_DEVICE_INVALID 0

typedef enum {
    STORAGE_TYPE_HDD,
    STORAGE_TYPE_SSD,
    STORAGE_TYPE_NVME,
    STORAGE_TYPE_USB,
    STORAGE_TYPE_CDROM,
    STORAGE_TYPE_FLOPPY,
} storage_type_t;

typedef struct storage_info {
    storage_device_t id;
    storage_type_t type;
    char model[64];
    char serial[32];
    uint64_t size_bytes;
    uint32_t block_size;
    bool removable;
    bool read_only;
} storage_info_t;

status_t hal_storage_init(void);
uint32_t hal_storage_get_device_count(void);
status_t hal_storage_get_info(storage_device_t device, storage_info_t* info);
status_t hal_storage_read(storage_device_t device, uint64_t lba, void* buffer, size_t count);
status_t hal_storage_write(storage_device_t device, uint64_t lba, const void* buffer, size_t count);
status_t hal_storage_flush(storage_device_t device);

/* ============================================================================
 * Network
 * ============================================================================ */

typedef uint64_t network_device_t;

#define NETWORK_DEVICE_INVALID 0

typedef struct mac_address {
    uint8_t addr[6];
} PACKED mac_address_t;

typedef enum {
    NETWORK_TYPE_ETHERNET,
    NETWORK_TYPE_WIFI,
    NETWORK_TYPE_LOOPBACK,
} network_type_t;

typedef struct network_info {
    network_device_t id;
    network_type_t type;
    char name[32];
    mac_address_t mac;
    uint32_t mtu;
    uint64_t link_speed_mbps;
    bool is_up;
} network_info_t;

status_t hal_network_init(void);
uint32_t hal_network_get_device_count(void);
status_t hal_network_get_info(network_device_t device, network_info_t* info);
status_t hal_network_send(network_device_t device, const void* packet, size_t size);
status_t hal_network_receive(network_device_t device, void* buffer, size_t* size);

/* ============================================================================
 * Display/GPU
 * ============================================================================ */

typedef uint64_t display_device_t;

#define DISPLAY_DEVICE_INVALID 0

typedef struct display_mode {
    uint32_t width;
    uint32_t height;
    uint32_t bpp;
    uint32_t refresh_rate;
} display_mode_t;

typedef struct display_info {
    display_device_t id;
    char name[64];
    uint32_t width;
    uint32_t height;
    uint32_t bpp;
    paddr_t framebuffer_addr;
    size_t framebuffer_size;
    uint32_t pitch;
} display_info_t;

status_t hal_display_init(void);
uint32_t hal_display_get_device_count(void);
status_t hal_display_get_info(display_device_t device, display_info_t* info);
status_t hal_display_set_mode(display_device_t device, display_mode_t* mode);
status_t hal_display_blit(display_device_t device, const void* data, uint32_t x, uint32_t y, uint32_t w, uint32_t h);

/* ============================================================================
 * Audio
 * ============================================================================ */

typedef uint64_t audio_device_t;

#define AUDIO_DEVICE_INVALID 0

typedef struct audio_info {
    audio_device_t id;
    char name[64];
    uint32_t sample_rate;
    uint32_t channels;
    uint32_t bits_per_sample;
} audio_info_t;

status_t hal_audio_init(void);
uint32_t hal_audio_get_device_count(void);
status_t hal_audio_get_info(audio_device_t device, audio_info_t* info);
status_t hal_audio_play(audio_device_t device, const void* data, size_t size);
status_t hal_audio_record(audio_device_t device, void* buffer, size_t size);

/* ============================================================================
 * Input Devices
 * ============================================================================ */

typedef enum {
    INPUT_EVENT_KEY_PRESS,
    INPUT_EVENT_KEY_RELEASE,
    INPUT_EVENT_MOUSE_MOVE,
    INPUT_EVENT_MOUSE_BUTTON,
    INPUT_EVENT_MOUSE_SCROLL,
} input_event_type_t;

typedef struct input_event {
    input_event_type_t type;
    uint64_t timestamp;
    union {
        struct {
            uint32_t keycode;
            uint32_t modifiers;
        } key;
        struct {
            int32_t x, y;
            int32_t dx, dy;
        } mouse_move;
        struct {
            uint8_t button;
            bool pressed;
        } mouse_button;
        struct {
            int32_t delta;
        } mouse_scroll;
    };
} input_event_t;

typedef void (*input_callback_t)(input_event_t* event, void* context);

status_t hal_input_init(void);
status_t hal_input_register_callback(input_callback_t callback, void* context);
status_t hal_input_poll(input_event_t* event);

/* ============================================================================
 * I/O Ports (x86_64 specific)
 * ============================================================================ */

#ifdef ARCH_X86_64

void outb(uint16_t port, uint8_t value);
uint8_t inb(uint16_t port);
void outw(uint16_t port, uint16_t value);
uint16_t inw(uint16_t port);
void outl(uint16_t port, uint32_t value);
uint32_t inl(uint16_t port);

#endif

/* ============================================================================
 * PCI Bus
 * ============================================================================ */

typedef struct pci_device {
    uint16_t vendor_id;
    uint16_t device_id;
    uint8_t bus;
    uint8_t device;
    uint8_t function;
    uint8_t class_code;
    uint8_t subclass;
    uint8_t prog_if;
    uint8_t revision;
    uint32_t bar[6];
    uint8_t interrupt_line;
} pci_device_t;

status_t hal_pci_init(void);
uint32_t hal_pci_get_device_count(void);
status_t hal_pci_get_device(uint32_t index, pci_device_t* device);
status_t hal_pci_find_device(uint16_t vendor_id, uint16_t device_id, pci_device_t* device);
status_t hal_pci_enable_device(pci_device_t* device);
uint64_t hal_pci_get_bar_size(pci_device_t* device, uint8_t bar_index);

#endif /* LIMITLESS_HAL_H */
