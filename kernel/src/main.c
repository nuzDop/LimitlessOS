/*
 * LimitlessOS Microkernel - Main Entry Point
 * Phase 1: Core microkernel with basic services
 */

#include "kernel.h"
#include "microkernel.h"
#include "hal.h"
#include "vmm.h"
#include "vfs.h"
#include "process.h"

/* Forward declarations for subsystem initialization */
extern status_t vmm_init(void);
extern status_t ipc_init(void);
extern status_t vfs_init(void);
extern status_t ramdisk_register(void);
extern status_t process_init(void);
extern status_t ata_init(void);
extern status_t net_init(void);
extern status_t e1000_init(void);
extern status_t pe_init(void);
extern status_t macho_init(void);

/* Early boot console */
extern void console_init(void);
extern void console_write(const char* str);
extern void console_putchar(char c);

/* Boot information structure (from bootloader) */
typedef struct boot_info {
    uint32_t magic;
    paddr_t mem_start;
    size_t mem_size;
    paddr_t kernel_start;
    paddr_t kernel_end;
    uint32_t framebuffer_width;
    uint32_t framebuffer_height;
    paddr_t framebuffer_addr;
} boot_info_t;

#define BOOT_MAGIC 0x4C494D49  // 'LIMI'

/* Kernel statistics */
static struct {
    uint64_t boot_time;
    size_t total_memory;
    size_t free_memory;
    uint32_t cpu_count;
    bool initialized;
} kernel_stats = {0};

/* Kernel panic */
void kernel_panic(const char* message) {
    console_write("\n\n*** KERNEL PANIC ***\n");
    console_write(message);
    console_write("\n\nSystem halted.\n");

    /* Halt CPU */
    __asm__ volatile("cli; hlt");
    while (1) {
        __asm__ volatile("hlt");
    }
}

/* Kernel assertion */
void kernel_assert(bool condition, const char* message) {
    if (!condition) {
        kernel_panic(message);
    }
}

/* Simple integer to string */
static void itoa(uint64_t value, char* buffer, int base) {
    char temp[32];
    int i = 0;

    if (value == 0) {
        buffer[0] = '0';
        buffer[1] = '\0';
        return;
    }

    while (value > 0) {
        int digit = value % base;
        temp[i++] = (digit < 10) ? ('0' + digit) : ('a' + digit - 10);
        value /= base;
    }

    int j = 0;
    while (i > 0) {
        buffer[j++] = temp[--i];
    }
    buffer[j] = '\0';
}

/* Kernel logging implementation */
void kernel_log(log_level_t level, const char* subsystem, const char* format, ...) {
    const char* level_str[] = {
        "[DEBUG]",
        "[INFO] ",
        "[WARN] ",
        "[ERROR]",
        "[FATAL]"
    };

    console_write(level_str[level]);
    console_write(" ");
    console_write(subsystem);
    console_write(": ");
    console_write(format);
    console_write("\n");
}

/* Initialize subsystems */
static void init_subsystems(boot_info_t* boot_info) {
    status_t status;

    /* Initialize physical memory manager */
    KLOG_INFO("MM", "Initializing physical memory manager");
    status = pmm_init(boot_info->mem_start, boot_info->mem_size);
    KASSERT(SUCCESS(status));

    kernel_stats.total_memory = boot_info->mem_size;
    kernel_stats.free_memory = pmm_get_free_memory();

    char mem_str[32];
    itoa(boot_info->mem_size / MB(1), mem_str, 10);
    console_write("  Total memory: ");
    console_write(mem_str);
    console_write(" MB\n");

    /* Initialize hardware abstraction layer */
    KLOG_INFO("HAL", "Initializing Hardware Abstraction Layer");
    status = hal_init();
    KASSERT(SUCCESS(status));

    /* Detect CPUs */
    kernel_stats.cpu_count = hal_cpu_count();
    console_write("  CPUs detected: ");
    itoa(kernel_stats.cpu_count, mem_str, 10);
    console_write(mem_str);
    console_write("\n");

    /* Initialize scheduler */
    KLOG_INFO("SCHED", "Initializing scheduler");
    sched_config_t sched_config = {
        .policy = SCHED_POLICY_PRIORITY,
        .time_slice_ns = 10000000,  // 10ms
        .preemptive = true,
        .smp_enabled = (kernel_stats.cpu_count > 1),
    };
    status = sched_init(&sched_config);
    KASSERT(SUCCESS(status));

    /* Initialize virtual memory manager */
    KLOG_INFO("VMM", "Initializing virtual memory manager");
    status = vmm_init();
    KASSERT(SUCCESS(status));

    /* Initialize IPC subsystem */
    KLOG_INFO("IPC", "Initializing IPC subsystem");
    status = ipc_init();
    KASSERT(SUCCESS(status));

    /* Initialize Virtual File System */
    KLOG_INFO("VFS", "Initializing virtual file system");
    status = vfs_init();
    KASSERT(SUCCESS(status));

    /* Register ramdisk filesystem */
    KLOG_INFO("VFS", "Registering ramdisk filesystem");
    status = ramdisk_register();
    KASSERT(SUCCESS(status));

    /* Mount ramdisk as root */
    KLOG_INFO("VFS", "Mounting ramdisk at /");
    status = vfs_mount(NULL, "/", "ramdisk", 0);
    KASSERT(SUCCESS(status));

    /* Initialize process manager */
    KLOG_INFO("PROCESS", "Initializing process manager");
    status = process_init();
    KASSERT(SUCCESS(status));

    /* Initialize ATA storage driver */
    KLOG_INFO("STORAGE", "Initializing ATA/AHCI driver");
    status = ata_init();
    if (FAILED(status)) {
        KLOG_WARN("STORAGE", "No ATA devices found");
    }

    /* Initialize network stack */
    KLOG_INFO("NET", "Initializing network stack");
    status = net_init();
    KASSERT(SUCCESS(status));

    /* Initialize e1000 network driver */
    KLOG_INFO("NET", "Initializing e1000 driver");
    status = e1000_init();
    if (FAILED(status)) {
        KLOG_WARN("NET", "No e1000 devices found");
    }

    /* Initialize PE loader (Windows Persona) */
    KLOG_INFO("PE", "Initializing PE loader");
    status = pe_init();
    KASSERT(SUCCESS(status));

    /* Initialize Mach-O loader (macOS Persona) */
    KLOG_INFO("MACHO", "Initializing Mach-O loader");
    status = macho_init();
    KASSERT(SUCCESS(status));

    /* Initialize capability system */
    KLOG_INFO("CAP", "Initializing capability system");
    // Capability system initialization will be implemented
}

/* Print banner */
static void print_banner(void) {
    console_write("\n");
    console_write("  ██╗     ██╗███╗   ███╗██╗████████╗██╗     ███████╗███████╗███████╗\n");
    console_write("  ██║     ██║████╗ ████║██║╚══██╔══╝██║     ██╔════╝██╔════╝██╔════╝\n");
    console_write("  ██║     ██║██╔████╔██║██║   ██║   ██║     █████╗  ███████╗███████╗\n");
    console_write("  ██║     ██║██║╚██╔╝██║██║   ██║   ██║     ██╔══╝  ╚════██║╚════██║\n");
    console_write("  ███████╗██║██║ ╚═╝ ██║██║   ██║   ███████╗███████╗███████║███████║\n");
    console_write("  ╚══════╝╚═╝╚═╝     ╚═╝╚═╝   ╚═╝   ╚══════╝╚══════╝╚══════╝╚══════╝\n");
    console_write("\n");
    console_write("  LimitlessOS ");
    console_write(LIMITLESS_VERSION_STRING);
    console_write(" [");
    console_write(ARCH_NAME);
    console_write("]\n");
    console_write("  The future of operating systems.\n");
    console_write("\n");
}

/* Kernel main entry point */
void kernel_main(void* boot_info_ptr) {
    boot_info_t* boot_info = (boot_info_t*)boot_info_ptr;

    /* Initialize early console */
    console_init();

    /* Print banner */
    print_banner();

    /* Validate boot info */
    KLOG_INFO("BOOT", "Validating boot information");
    if (boot_info && boot_info->magic == BOOT_MAGIC) {
        console_write("  Boot magic: OK\n");
    } else {
        PANIC("Invalid boot information");
    }

    /* Initialize core subsystems */
    KLOG_INFO("KERNEL", "Initializing core subsystems");
    init_subsystems(boot_info);

    /* Mark kernel as initialized */
    kernel_stats.initialized = true;

    KLOG_INFO("KERNEL", "Boot complete - entering idle loop");
    console_write("\n");
    console_write("==================================================\n");
    console_write("  System ready. Phase 1 microkernel operational.\n");
    console_write("==================================================\n");
    console_write("\n");

    /* Kernel idle loop */
    while (1) {
        /* In a real system, this would:
         * - Wait for interrupts
         * - Schedule threads
         * - Handle IPC messages
         * - Manage power states
         */
        __asm__ volatile("hlt");
    }
}

/* Global constructors/destructors (for C++ support later) */
extern void (*__init_array_start[])(void);
extern void (*__init_array_end[])(void);

void _init(void) {
    size_t count = __init_array_end - __init_array_start;
    for (size_t i = 0; i < count; i++) {
        __init_array_start[i]();
    }
}

void _fini(void) {
    /* Nothing to do for now */
}
