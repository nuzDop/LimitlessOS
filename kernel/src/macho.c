/*
 * Mach-O (Mach Object) Loader Implementation
 * Supports macOS executables and dylibs (32-bit and 64-bit)
 */

#include "kernel.h"
#include "microkernel.h"
#include "vmm.h"
#include "macho.h"

/* Global statistics */
static macho_stats_t g_macho_stats = {0};
static uint32_t g_macho_lock = 0;

/* Memory allocation helper */
static void* macho_alloc(size_t size) {
    size_t pages = (size + PAGE_SIZE - 1) / PAGE_SIZE;
    paddr_t phys = pmm_alloc_pages(pages);
    if (!phys) {
        return NULL;
    }
    return (void*)PHYS_TO_VIRT_DIRECT(phys);
}

/* Memory free helper */
static void macho_free(void* ptr, size_t size) {
    if (!ptr) return;
    size_t pages = (size + PAGE_SIZE - 1) / PAGE_SIZE;
    paddr_t phys = VIRT_TO_PHYS_DIRECT((uintptr_t)ptr);
    pmm_free_pages(phys, pages);
}

/* Initialize Mach-O loader */
status_t macho_init(void) {
    KLOG_INFO("MACHO", "Initializing Mach-O loader");

    for (size_t i = 0; i < sizeof(g_macho_stats); i++) {
        ((uint8_t*)&g_macho_stats)[i] = 0;
    }

    g_macho_lock = 0;

    return STATUS_OK;
}

/* Validate Mach-O file */
status_t macho_validate(const uint8_t* data, size_t size) {
    if (!data || size < sizeof(macho_header_32_t)) {
        return STATUS_INVALID;
    }

    uint32_t magic = *(uint32_t*)data;

    /* Check for valid Mach-O magic */
    if (magic != MACHO_MAGIC_32 && magic != MACHO_MAGIC_64 &&
        magic != MACHO_CIGAM_32 && magic != MACHO_CIGAM_64 &&
        magic != MACHO_FAT_MAGIC && magic != MACHO_FAT_CIGAM) {
        return STATUS_INVALID;
    }

    /* Fat binaries not fully supported yet */
    if (magic == MACHO_FAT_MAGIC || magic == MACHO_FAT_CIGAM) {
        KLOG_WARN("MACHO", "Fat/Universal binaries not fully supported");
        return STATUS_NOSUPPORT;
    }

    return STATUS_OK;
}

/* Get load command by type */
load_command_t* macho_get_load_command(macho_context_t* ctx, uint32_t cmd_type) {
    if (!ctx || !ctx->load_commands) {
        return NULL;
    }

    uint8_t* cmd_ptr = ctx->load_commands;
    for (uint32_t i = 0; i < ctx->num_commands; i++) {
        load_command_t* lc = (load_command_t*)cmd_ptr;
        uint32_t cmd = lc->cmd;
        uint32_t cmdsize = lc->cmdsize;

        if (ctx->swap_bytes) {
            cmd = macho_swap32(cmd);
            cmdsize = macho_swap32(cmdsize);
        }

        if (cmd == cmd_type) {
            return lc;
        }

        cmd_ptr += cmdsize;
    }

    return NULL;
}

/* Get dylib name from dylib command */
const char* macho_get_dylib_name(macho_context_t* ctx, dylib_command_t* dylib_cmd) {
    if (!ctx || !dylib_cmd) {
        return NULL;
    }

    uint32_t name_offset = dylib_cmd->name_offset;
    if (ctx->swap_bytes) {
        name_offset = macho_swap32(name_offset);
    }

    return (const char*)((uint8_t*)dylib_cmd + name_offset);
}

/* Get dylinker name from dylinker command */
const char* macho_get_dylinker_name(macho_context_t* ctx, dylinker_command_t* dylinker_cmd) {
    if (!ctx || !dylinker_cmd) {
        return NULL;
    }

    uint32_t name_offset = dylinker_cmd->name_offset;
    if (ctx->swap_bytes) {
        name_offset = macho_swap32(name_offset);
    }

    return (const char*)((uint8_t*)dylinker_cmd + name_offset);
}

/* Load Mach-O file */
status_t macho_load(const uint8_t* data, size_t size, struct address_space* aspace, macho_context_t** out_ctx) {
    if (!data || !aspace || !out_ctx) {
        return STATUS_INVALID;
    }

    /* Validate first */
    status_t result = macho_validate(data, size);
    if (FAILED(result)) {
        return result;
    }

    /* Allocate context */
    macho_context_t* ctx = (macho_context_t*)macho_alloc(sizeof(macho_context_t));
    if (!ctx) {
        return STATUS_NOMEM;
    }

    /* Initialize context */
    for (size_t i = 0; i < sizeof(macho_context_t); i++) {
        ((uint8_t*)ctx)[i] = 0;
    }

    ctx->data = data;
    ctx->size = size;
    ctx->aspace = aspace;

    /* Check magic and determine format */
    uint32_t magic = *(uint32_t*)data;

    if (magic == MACHO_MAGIC_64 || magic == MACHO_CIGAM_64) {
        ctx->is_64bit = true;
        ctx->swap_bytes = (magic == MACHO_CIGAM_64);
        ctx->header64 = (macho_header_64_t*)data;

        macho_header_64_t* hdr = ctx->header64;
        ctx->num_commands = ctx->swap_bytes ? macho_swap32(hdr->ncmds) : hdr->ncmds;
        ctx->file_type = ctx->swap_bytes ? macho_swap32(hdr->filetype) : hdr->filetype;

        /* Load commands start after header */
        ctx->load_commands = (uint8_t*)(data + sizeof(macho_header_64_t));

    } else {
        ctx->is_64bit = false;
        ctx->swap_bytes = (magic == MACHO_CIGAM_32);
        ctx->header32 = (macho_header_32_t*)data;

        macho_header_32_t* hdr = ctx->header32;
        ctx->num_commands = ctx->swap_bytes ? macho_swap32(hdr->ncmds) : hdr->ncmds;
        ctx->file_type = ctx->swap_bytes ? macho_swap32(hdr->filetype) : hdr->filetype;

        /* Load commands start after header */
        ctx->load_commands = (uint8_t*)(data + sizeof(macho_header_32_t));
    }

    /* Check if dylib */
    ctx->is_dylib = (ctx->file_type == MACHO_TYPE_DYLIB);

    /* Find entry point from LC_MAIN or LC_UNIXTHREAD */
    load_command_t* main_cmd = macho_get_load_command(ctx, LC_MAIN);
    if (main_cmd) {
        entry_point_command_t* ep = (entry_point_command_t*)main_cmd;
        uint64_t entryoff = ep->entryoff;
        if (ctx->swap_bytes) {
            entryoff = macho_swap64(entryoff);
        }
        ctx->entry_point = ctx->base_address + entryoff;
    }

    /* Update statistics */
    __sync_lock_test_and_set(&g_macho_lock, 1);
    g_macho_stats.macho_loaded++;
    if (ctx->is_64bit) {
        g_macho_stats.macho64_loaded++;
    } else {
        g_macho_stats.macho32_loaded++;
    }
    if (ctx->is_dylib) {
        g_macho_stats.dylibs_loaded++;
    }
    __sync_lock_release(&g_macho_lock);

    *out_ctx = ctx;
    return STATUS_OK;
}

/* Load segments into memory */
status_t macho_load_segments(macho_context_t* ctx) {
    if (!ctx) {
        return STATUS_INVALID;
    }

    KLOG_DEBUG("MACHO", "Loading segments (%d load commands)", ctx->num_commands);

    uint8_t* cmd_ptr = ctx->load_commands;
    uintptr_t min_addr = (uintptr_t)-1;
    uintptr_t max_addr = 0;

    /* First pass: determine address range */
    for (uint32_t i = 0; i < ctx->num_commands; i++) {
        load_command_t* lc = (load_command_t*)cmd_ptr;
        uint32_t cmd = lc->cmd;
        uint32_t cmdsize = lc->cmdsize;

        if (ctx->swap_bytes) {
            cmd = macho_swap32(cmd);
            cmdsize = macho_swap32(cmdsize);
        }

        if (ctx->is_64bit && cmd == LC_SEGMENT_64) {
            segment_command_64_t* seg = (segment_command_64_t*)lc;
            uint64_t vmaddr = seg->vmaddr;
            uint64_t vmsize = seg->vmsize;

            if (ctx->swap_bytes) {
                vmaddr = macho_swap64(vmaddr);
                vmsize = macho_swap64(vmsize);
            }

            if (vmsize > 0) {
                if (vmaddr < min_addr) min_addr = vmaddr;
                if (vmaddr + vmsize > max_addr) max_addr = vmaddr + vmsize;
            }

        } else if (!ctx->is_64bit && cmd == LC_SEGMENT) {
            segment_command_32_t* seg = (segment_command_32_t*)lc;
            uint32_t vmaddr = seg->vmaddr;
            uint32_t vmsize = seg->vmsize;

            if (ctx->swap_bytes) {
                vmaddr = macho_swap32(vmaddr);
                vmsize = macho_swap32(vmsize);
            }

            if (vmsize > 0) {
                if (vmaddr < min_addr) min_addr = vmaddr;
                if (vmaddr + vmsize > max_addr) max_addr = vmaddr + vmsize;
            }
        }

        cmd_ptr += cmdsize;
    }

    ctx->base_address = min_addr;
    ctx->total_size = max_addr - min_addr;

    /* Allocate memory for entire image */
    size_t total_pages = (ctx->total_size + PAGE_SIZE - 1) / PAGE_SIZE;
    for (size_t i = 0; i < total_pages; i++) {
        paddr_t phys = pmm_alloc_page();
        if (!phys) {
            return STATUS_NOMEM;
        }

        uintptr_t vaddr = ctx->base_address + (i * PAGE_SIZE);
        uint32_t flags = PTE_PRESENT | PTE_USER | PTE_WRITE;

        status_t result = vmm_map_page(ctx->aspace, vaddr, phys, flags);
        if (FAILED(result)) {
            pmm_free_page(phys);
            return result;
        }

        /* Zero the page */
        uint8_t* page_ptr = (uint8_t*)PHYS_TO_VIRT_DIRECT(phys);
        for (size_t j = 0; j < PAGE_SIZE; j++) {
            page_ptr[j] = 0;
        }
    }

    /* Second pass: load segment data */
    cmd_ptr = ctx->load_commands;
    uint32_t seg_count = 0;

    for (uint32_t i = 0; i < ctx->num_commands; i++) {
        load_command_t* lc = (load_command_t*)cmd_ptr;
        uint32_t cmd = lc->cmd;
        uint32_t cmdsize = lc->cmdsize;

        if (ctx->swap_bytes) {
            cmd = macho_swap32(cmd);
            cmdsize = macho_swap32(cmdsize);
        }

        if (ctx->is_64bit && cmd == LC_SEGMENT_64) {
            segment_command_64_t* seg = (segment_command_64_t*)lc;

            uint64_t vmaddr = seg->vmaddr;
            uint64_t vmsize = seg->vmsize;
            uint64_t fileoff = seg->fileoff;
            uint64_t filesize = seg->filesize;
            uint32_t initprot = seg->initprot;

            if (ctx->swap_bytes) {
                vmaddr = macho_swap64(vmaddr);
                vmsize = macho_swap64(vmsize);
                fileoff = macho_swap64(fileoff);
                filesize = macho_swap64(filesize);
                initprot = macho_swap32(initprot);
            }

            /* Copy segment data from file */
            if (filesize > 0) {
                const uint8_t* src = ctx->data + fileoff;
                for (size_t j = 0; j < filesize; j++) {
                    paddr_t phys;
                    if (FAILED(vmm_get_physical(ctx->aspace, vmaddr + j, &phys))) {
                        continue;
                    }

                    uint8_t* dest = (uint8_t*)PHYS_TO_VIRT_DIRECT(phys);
                    dest[j % PAGE_SIZE] = src[j];
                }
            }

            /* Update page permissions */
            uint32_t flags = PTE_PRESENT | PTE_USER;
            if (initprot & VM_PROT_WRITE) flags |= PTE_WRITE;
            if (!(initprot & VM_PROT_EXECUTE)) flags |= PTE_NX;

            size_t seg_pages = (vmsize + PAGE_SIZE - 1) / PAGE_SIZE;
            for (size_t j = 0; j < seg_pages; j++) {
                uintptr_t page_va = vmaddr + (j * PAGE_SIZE);
                paddr_t phys;

                if (SUCCESS(vmm_get_physical(ctx->aspace, page_va, &phys))) {
                    vmm_map_page(ctx->aspace, page_va, phys, flags);
                }
            }

            seg_count++;
            KLOG_DEBUG("MACHO", "  Segment %.16s: VA=0x%lx Size=0x%lx", seg->segname, vmaddr, vmsize);

        } else if (!ctx->is_64bit && cmd == LC_SEGMENT) {
            segment_command_32_t* seg = (segment_command_32_t*)lc;

            uint32_t vmaddr = seg->vmaddr;
            uint32_t vmsize = seg->vmsize;
            uint32_t fileoff = seg->fileoff;
            uint32_t filesize = seg->filesize;
            uint32_t initprot = seg->initprot;

            if (ctx->swap_bytes) {
                vmaddr = macho_swap32(vmaddr);
                vmsize = macho_swap32(vmsize);
                fileoff = macho_swap32(fileoff);
                filesize = macho_swap32(filesize);
                initprot = macho_swap32(initprot);
            }

            /* Copy segment data */
            if (filesize > 0) {
                const uint8_t* src = ctx->data + fileoff;
                for (size_t j = 0; j < filesize; j++) {
                    paddr_t phys;
                    if (FAILED(vmm_get_physical(ctx->aspace, vmaddr + j, &phys))) {
                        continue;
                    }

                    uint8_t* dest = (uint8_t*)PHYS_TO_VIRT_DIRECT(phys);
                    dest[j % PAGE_SIZE] = src[j];
                }
            }

            /* Update permissions */
            uint32_t flags = PTE_PRESENT | PTE_USER;
            if (initprot & VM_PROT_WRITE) flags |= PTE_WRITE;
            if (!(initprot & VM_PROT_EXECUTE)) flags |= PTE_NX;

            size_t seg_pages = (vmsize + PAGE_SIZE - 1) / PAGE_SIZE;
            for (size_t j = 0; j < seg_pages; j++) {
                uintptr_t page_va = vmaddr + (j * PAGE_SIZE);
                paddr_t phys;

                if (SUCCESS(vmm_get_physical(ctx->aspace, page_va, &phys))) {
                    vmm_map_page(ctx->aspace, page_va, phys, flags);
                }
            }

            seg_count++;
            KLOG_DEBUG("MACHO", "  Segment %.16s: VA=0x%x Size=0x%x", seg->segname, vmaddr, vmsize);
        }

        cmd_ptr += cmdsize;
    }

    __sync_lock_test_and_set(&g_macho_lock, 1);
    g_macho_stats.segments_loaded += seg_count;
    __sync_lock_release(&g_macho_lock);

    return STATUS_OK;
}

/* Resolve symbols (stub - requires dyld) */
status_t macho_resolve_symbols(macho_context_t* ctx) {
    if (!ctx) {
        return STATUS_INVALID;
    }

    /* Symbol resolution requires dynamic linker (dyld) */
    /* This is a placeholder for the macOS persona layer */

    KLOG_DEBUG("MACHO", "Symbol resolution deferred to dyld");

    __sync_lock_test_and_set(&g_macho_lock, 1);
    g_macho_stats.symbols_resolved++;
    __sync_lock_release(&g_macho_lock);

    return STATUS_OK;
}

/* Free Mach-O context */
void macho_free_context(macho_context_t* ctx) {
    if (!ctx) {
        return;
    }

    macho_free(ctx, sizeof(macho_context_t));
}

/* Get statistics */
status_t macho_get_stats(macho_stats_t* out_stats) {
    if (!out_stats) {
        return STATUS_INVALID;
    }

    __sync_lock_test_and_set(&g_macho_lock, 1);

    out_stats->macho_loaded = g_macho_stats.macho_loaded;
    out_stats->macho32_loaded = g_macho_stats.macho32_loaded;
    out_stats->macho64_loaded = g_macho_stats.macho64_loaded;
    out_stats->dylibs_loaded = g_macho_stats.dylibs_loaded;
    out_stats->segments_loaded = g_macho_stats.segments_loaded;
    out_stats->symbols_resolved = g_macho_stats.symbols_resolved;

    __sync_lock_release(&g_macho_lock);

    return STATUS_OK;
}
