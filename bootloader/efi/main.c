/*
 * LimitlessOS UEFI Bootloader
 * Phase 1: Basic EFI loader
 */

#include "efi.h"

/* Forward declarations */
void efi_console_init(EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *con);
void efi_print(const CHAR16 *str);
void efi_print_ascii(const char *str);
void efi_clear_screen(void);

/* Boot info structure passed to kernel */
typedef struct {
    uint32_t magic;
    uint64_t mem_start;
    uint64_t mem_size;
    uint64_t kernel_start;
    uint64_t kernel_end;
    uint32_t framebuffer_width;
    uint32_t framebuffer_height;
    uint64_t framebuffer_addr;
} boot_info_t;

#define BOOT_MAGIC 0x4C494D49  // 'LIMI'

/* Required by compiler - memory functions */
void *memset(void *s, int c, UINTN n) {
    unsigned char *p = s;
    while (n--) {
        *p++ = (unsigned char)c;
    }
    return s;
}

void *memcpy(void *dest, const void *src, UINTN n) {
    unsigned char *d = dest;
    const unsigned char *s = src;
    while (n--) {
        *d++ = *s++;
    }
    return dest;
}

/* String utilities */
static UINTN strlen_ascii(const char *str) {
    UINTN len = 0;
    while (str[len]) len++;
    return len;
}

static void uint64_to_hex(uint64_t value, CHAR16 *buffer) {
    const CHAR16 hex[] = u"0123456789ABCDEF";
    buffer[0] = '0';
    buffer[1] = 'x';

    for (int i = 15; i >= 0; i--) {
        buffer[2 + (15 - i)] = hex[(value >> (i * 4)) & 0xF];
    }
    buffer[18] = 0;
}

/* Memory allocation */
static EFI_STATUS allocate_kernel_memory(EFI_BOOT_SERVICES *BS, uint64_t size, void **address) {
    UINTN pages = (size + 4095) / 4096;
    EFI_PHYSICAL_ADDRESS addr = 0x100000;  // Load at 1MB

    EFI_STATUS status = BS->AllocatePages(AllocateAddress, EfiLoaderCode, pages, &addr);
    if (status == EFI_SUCCESS) {
        *address = (void*)addr;
    }

    return status;
}

/* Get memory map */
static uint64_t get_total_memory(EFI_BOOT_SERVICES *BS) {
    UINTN map_size = 0;
    UINTN map_key = 0;
    UINTN descriptor_size = 0;
    uint32_t descriptor_version = 0;

    /* Get size */
    BS->GetMemoryMap(&map_size, NULL, &map_key, &descriptor_size, &descriptor_version);

    /* Allocate buffer */
    EFI_PHYSICAL_ADDRESS buffer = 0;
    UINTN pages = (map_size + 4095) / 4096;
    BS->AllocatePages(AllocateAnyPages, EfiLoaderData, pages, &buffer);

    /* Get map */
    BS->GetMemoryMap(&map_size, (void*)buffer, &map_key, &descriptor_size, &descriptor_version);

    /* Calculate total usable memory */
    uint64_t total = 0;
    uint8_t *ptr = (uint8_t*)buffer;
    UINTN entries = map_size / descriptor_size;

    for (UINTN i = 0; i < entries; i++) {
        EFI_MEMORY_DESCRIPTOR *desc = (EFI_MEMORY_DESCRIPTOR*)(ptr + i * descriptor_size);
        if (desc->Type == EfiConventionalMemory) {
            total += desc->NumberOfPages * 4096;
        }
    }

    BS->FreePages(buffer, pages);
    return total;
}

/* Simple kernel loader (stub - would load from disk in production) */
static EFI_STATUS load_kernel(EFI_BOOT_SERVICES *BS, void **kernel_entry, uint64_t *kernel_size) {
    /* For Phase 1, just allocate space
     * In production, would:
     * 1. Read kernel from EFI System Partition
     * 2. Parse ELF headers
     * 3. Load segments to correct addresses
     * 4. Return entry point
     */

    *kernel_size = 0x100000;  // 1MB for now
    EFI_STATUS status = allocate_kernel_memory(BS, *kernel_size, kernel_entry);

    return status;
}

/* Main EFI entry point */
EFI_STATUS EFIAPI efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable) {
    EFI_BOOT_SERVICES *BS = SystemTable->BootServices;
    EFI_STATUS status;

    /* Initialize console */
    efi_console_init(SystemTable->ConOut);
    efi_clear_screen();

    /* Print banner */
    efi_print(u"\r\n");
    efi_print(u"  ========================================\r\n");
    efi_print(u"  LimitlessOS UEFI Bootloader v0.1.0\r\n");
    efi_print(u"  Phase 1 - Basic Boot Support\r\n");
    efi_print(u"  ========================================\r\n");
    efi_print(u"\r\n");

    /* Get memory information */
    efi_print(u"  [*] Detecting memory...\r\n");
    uint64_t total_memory = get_total_memory(BS);
    CHAR16 mem_str[32];
    uint64_to_hex(total_memory, mem_str);
    efi_print(u"      Total usable memory: ");
    efi_print(mem_str);
    efi_print(u"\r\n");

    /* Load kernel */
    efi_print(u"  [*] Loading kernel...\r\n");
    void *kernel_entry = NULL;
    uint64_t kernel_size = 0;
    status = load_kernel(BS, &kernel_entry, &kernel_size);

    if (status != EFI_SUCCESS) {
        efi_print(u"      ERROR: Failed to allocate kernel memory\r\n");
        while (1) {
            BS->Stall(1000000);
        }
    }

    efi_print(u"      Kernel allocated at: ");
    uint64_to_hex((uint64_t)kernel_entry, mem_str);
    efi_print(mem_str);
    efi_print(u"\r\n");

    /* Prepare boot info */
    boot_info_t boot_info = {0};
    boot_info.magic = BOOT_MAGIC;
    boot_info.mem_start = 0x100000;
    boot_info.mem_size = total_memory;
    boot_info.kernel_start = (uint64_t)kernel_entry;
    boot_info.kernel_end = (uint64_t)kernel_entry + kernel_size;
    boot_info.framebuffer_width = 0;
    boot_info.framebuffer_height = 0;
    boot_info.framebuffer_addr = 0;

    /* Exit boot services */
    efi_print(u"  [*] Exiting boot services...\r\n");
    UINTN map_size = 0;
    UINTN map_key = 0;
    UINTN descriptor_size = 0;
    uint32_t descriptor_version = 0;

    BS->GetMemoryMap(&map_size, NULL, &map_key, &descriptor_size, &descriptor_version);

    EFI_PHYSICAL_ADDRESS map_buffer = 0;
    UINTN pages = (map_size + 4095) / 4096;
    BS->AllocatePages(AllocateAnyPages, EfiLoaderData, pages, &map_buffer);

    BS->GetMemoryMap(&map_size, (void*)map_buffer, &map_key, &descriptor_size, &descriptor_version);

    status = BS->ExitBootServices(ImageHandle, map_key);
    if (status != EFI_SUCCESS) {
        /* Retry once */
        BS->GetMemoryMap(&map_size, (void*)map_buffer, &map_key, &descriptor_size, &descriptor_version);
        status = BS->ExitBootServices(ImageHandle, map_key);
    }

    /* Jump to kernel */
    if (status == EFI_SUCCESS && kernel_entry) {
        /* In production, would jump to kernel_entry with boot_info */
        /* For Phase 1, just halt since we don't have actual kernel binary loaded */
        __asm__ volatile("cli; hlt");
    }

    /* Should never reach here */
    while (1) {
        __asm__ volatile("hlt");
    }

    return EFI_SUCCESS;
}
