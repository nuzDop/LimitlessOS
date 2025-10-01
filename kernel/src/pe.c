/*
 * PE (Portable Executable) Loader Implementation
 * Supports Windows PE32 and PE32+ (PE64) executables
 */

#include "kernel.h"
#include "microkernel.h"
#include "vmm.h"
#include "pe.h"

/* Global statistics */
static pe_stats_t g_pe_stats = {0};
static uint32_t g_pe_lock = 0;

/* Memory allocation helper */
static void* pe_alloc(size_t size) {
    size_t pages = (size + PAGE_SIZE - 1) / PAGE_SIZE;
    paddr_t phys = pmm_alloc_pages(pages);
    if (!phys) {
        return NULL;
    }
    return (void*)PHYS_TO_VIRT_DIRECT(phys);
}

/* Memory free helper */
static void pe_free(void* ptr, size_t size) {
    if (!ptr) return;
    size_t pages = (size + PAGE_SIZE - 1) / PAGE_SIZE;
    paddr_t phys = VIRT_TO_PHYS_DIRECT((uintptr_t)ptr);
    pmm_free_pages(phys, pages);
}

/* Initialize PE loader */
status_t pe_init(void) {
    KLOG_INFO("PE", "Initializing PE loader");

    for (size_t i = 0; i < sizeof(g_pe_stats); i++) {
        ((uint8_t*)&g_pe_stats)[i] = 0;
    }

    g_pe_lock = 0;

    return STATUS_OK;
}

/* Validate PE file */
status_t pe_validate(const uint8_t* data, size_t size) {
    if (!data || size < sizeof(pe_dos_header_t)) {
        return STATUS_INVALID;
    }

    /* Check DOS header */
    pe_dos_header_t* dos_header = (pe_dos_header_t*)data;
    if (dos_header->e_magic != PE_DOS_MAGIC) {
        return STATUS_INVALID;
    }

    /* Check PE header location */
    if (dos_header->e_lfanew < 0 || (size_t)dos_header->e_lfanew + sizeof(uint32_t) + sizeof(pe_coff_header_t) > size) {
        return STATUS_INVALID;
    }

    /* Check PE signature */
    uint32_t* pe_sig = (uint32_t*)(data + dos_header->e_lfanew);
    if (*pe_sig != PE_NT_MAGIC) {
        return STATUS_INVALID;
    }

    /* Check machine type */
    pe_coff_header_t* coff = (pe_coff_header_t*)(pe_sig + 1);
    if (coff->machine != PE_MACHINE_AMD64 && coff->machine != PE_MACHINE_I386) {
        KLOG_WARN("PE", "Unsupported machine type: 0x%x", coff->machine);
        return STATUS_NOSUPPORT;
    }

    /* Verify optional header */
    if (coff->optional_header_size < sizeof(uint16_t)) {
        return STATUS_INVALID;
    }

    uint16_t* opt_magic = (uint16_t*)(coff + 1);
    if (*opt_magic != PE_OPTIONAL_MAGIC_PE32 && *opt_magic != PE_OPTIONAL_MAGIC_PE32_PLUS) {
        return STATUS_INVALID;
    }

    return STATUS_OK;
}

/* Find section containing RVA */
status_t pe_section_by_rva(pe_context_t* ctx, uint32_t rva, pe_section_header_t** out_section) {
    if (!ctx || !out_section) {
        return STATUS_INVALID;
    }

    for (uint16_t i = 0; i < ctx->coff_header->num_sections; i++) {
        pe_section_header_t* section = &ctx->sections[i];
        uint32_t section_start = section->virtual_address;
        uint32_t section_end = section_start + section->virtual_size;

        if (rva >= section_start && rva < section_end) {
            *out_section = section;
            return STATUS_OK;
        }
    }

    return STATUS_NOTFOUND;
}

/* Convert RVA to virtual address */
uintptr_t pe_rva_to_va(pe_context_t* ctx, uint32_t rva) {
    if (!ctx) {
        return 0;
    }
    return ctx->image_base + rva;
}

/* Convert RVA to file offset */
const void* pe_rva_to_file_offset(pe_context_t* ctx, uint32_t rva) {
    if (!ctx) {
        return NULL;
    }

    pe_section_header_t* section = NULL;
    if (FAILED(pe_section_by_rva(ctx, rva, &section))) {
        return NULL;
    }

    uint32_t offset_in_section = rva - section->virtual_address;
    return ctx->data + section->raw_data_offset + offset_in_section;
}

/* Load PE file */
status_t pe_load(const uint8_t* data, size_t size, struct address_space* aspace, pe_context_t** out_ctx) {
    if (!data || !aspace || !out_ctx) {
        return STATUS_INVALID;
    }

    /* Validate first */
    status_t result = pe_validate(data, size);
    if (FAILED(result)) {
        return result;
    }

    /* Allocate context */
    pe_context_t* ctx = (pe_context_t*)pe_alloc(sizeof(pe_context_t));
    if (!ctx) {
        return STATUS_NOMEM;
    }

    /* Initialize context */
    for (size_t i = 0; i < sizeof(pe_context_t); i++) {
        ((uint8_t*)ctx)[i] = 0;
    }

    ctx->data = data;
    ctx->size = size;
    ctx->aspace = aspace;

    /* Parse DOS header */
    ctx->dos_header = (pe_dos_header_t*)data;

    /* Parse PE headers */
    uint32_t* pe_sig = (uint32_t*)(data + ctx->dos_header->e_lfanew);
    ctx->coff_header = (pe_coff_header_t*)(pe_sig + 1);

    /* Parse optional header */
    uint16_t* opt_magic = (uint16_t*)(ctx->coff_header + 1);
    if (*opt_magic == PE_OPTIONAL_MAGIC_PE32_PLUS) {
        ctx->is_pe32_plus = true;
        ctx->optional_header.opt64 = (pe_optional_header64_t*)opt_magic;
        ctx->image_base = ctx->optional_header.opt64->image_base;
        ctx->image_size = ctx->optional_header.opt64->image_size;
        ctx->entry_point = ctx->image_base + ctx->optional_header.opt64->entry_point;
        ctx->subsystem = ctx->optional_header.opt64->subsystem;
    } else {
        ctx->is_pe32_plus = false;
        ctx->optional_header.opt32 = (pe_optional_header32_t*)opt_magic;
        ctx->image_base = ctx->optional_header.opt32->image_base;
        ctx->image_size = ctx->optional_header.opt32->image_size;
        ctx->entry_point = ctx->image_base + ctx->optional_header.opt32->entry_point;
        ctx->subsystem = ctx->optional_header.opt32->subsystem;
    }

    /* Check if DLL */
    ctx->is_dll = (ctx->coff_header->characteristics & PE_CHAR_DLL) != 0;

    /* Parse section headers */
    ctx->sections = (pe_section_header_t*)((uint8_t*)(ctx->coff_header + 1) + ctx->coff_header->optional_header_size);

    /* Allocate image memory */
    size_t image_pages = (ctx->image_size + PAGE_SIZE - 1) / PAGE_SIZE;
    for (size_t i = 0; i < image_pages; i++) {
        paddr_t phys = pmm_alloc_page();
        if (!phys) {
            pe_free(ctx, sizeof(pe_context_t));
            return STATUS_NOMEM;
        }

        uintptr_t vaddr = ctx->image_base + (i * PAGE_SIZE);
        uint32_t flags = PTE_PRESENT | PTE_USER | PTE_WRITE;

        result = vmm_map_page(aspace, vaddr, phys, flags);
        if (FAILED(result)) {
            pmm_free_page(phys);
            pe_free(ctx, sizeof(pe_context_t));
            return result;
        }

        /* Zero the page */
        uint8_t* page_ptr = (uint8_t*)PHYS_TO_VIRT_DIRECT(phys);
        for (size_t j = 0; j < PAGE_SIZE; j++) {
            page_ptr[j] = 0;
        }
    }

    /* Update statistics */
    __sync_lock_test_and_set(&g_pe_lock, 1);
    g_pe_stats.pe_loaded++;
    if (ctx->is_pe32_plus) {
        g_pe_stats.pe32_plus_loaded++;
    } else {
        g_pe_stats.pe32_loaded++;
    }
    if (ctx->is_dll) {
        g_pe_stats.dlls_loaded++;
    }
    __sync_lock_release(&g_pe_lock);

    *out_ctx = ctx;
    return STATUS_OK;
}

/* Load sections into memory */
status_t pe_load_sections(pe_context_t* ctx) {
    if (!ctx) {
        return STATUS_INVALID;
    }

    KLOG_DEBUG("PE", "Loading %d sections", ctx->coff_header->num_sections);

    for (uint16_t i = 0; i < ctx->coff_header->num_sections; i++) {
        pe_section_header_t* section = &ctx->sections[i];

        /* Skip empty sections */
        if (section->raw_data_size == 0 || section->virtual_size == 0) {
            continue;
        }

        /* Calculate addresses */
        uintptr_t section_va = ctx->image_base + section->virtual_address;
        const uint8_t* section_data = ctx->data + section->raw_data_offset;
        size_t copy_size = section->raw_data_size;

        /* Don't copy more than virtual size */
        if (copy_size > section->virtual_size) {
            copy_size = section->virtual_size;
        }

        /* Copy section data */
        for (size_t j = 0; j < copy_size; j++) {
            paddr_t phys;
            status_t result = vmm_get_physical(ctx->aspace, section_va + j, &phys);
            if (FAILED(result)) {
                return result;
            }

            uint8_t* dest = (uint8_t*)PHYS_TO_VIRT_DIRECT(phys);
            dest[j % PAGE_SIZE] = section_data[j];
        }

        /* Update page permissions based on section characteristics */
        uint32_t flags = PTE_PRESENT | PTE_USER;

        if (section->characteristics & PE_SCN_MEM_WRITE) {
            flags |= PTE_WRITE;
        }

        if (!(section->characteristics & PE_SCN_MEM_EXECUTE)) {
            flags |= PTE_NX;
        }

        /* Apply permissions to all pages in section */
        size_t section_pages = (section->virtual_size + PAGE_SIZE - 1) / PAGE_SIZE;
        for (size_t j = 0; j < section_pages; j++) {
            uintptr_t page_va = section_va + (j * PAGE_SIZE);
            paddr_t phys;

            if (SUCCESS(vmm_get_physical(ctx->aspace, page_va, &phys))) {
                vmm_map_page(ctx->aspace, page_va, phys, flags);
            }
        }

        char section_name[PE_SECTION_NAME_SIZE + 1] = {0};
        for (int k = 0; k < PE_SECTION_NAME_SIZE && section->name[k]; k++) {
            section_name[k] = section->name[k];
        }

        KLOG_DEBUG("PE", "  Section %s: VA=0x%lx Size=0x%x", section_name, section_va, section->virtual_size);
    }

    return STATUS_OK;
}

/* Apply base relocations */
status_t pe_apply_relocations(pe_context_t* ctx, uintptr_t new_base) {
    if (!ctx) {
        return STATUS_INVALID;
    }

    /* Calculate delta */
    int64_t delta = (int64_t)new_base - (int64_t)ctx->image_base;
    if (delta == 0) {
        return STATUS_OK;  // No relocation needed
    }

    /* Get relocation directory */
    pe_data_directory_t* reloc_dir;
    if (ctx->is_pe32_plus) {
        if (PE_DIR_BASERELOC >= ctx->optional_header.opt64->num_data_directories) {
            return STATUS_NOTFOUND;
        }
        reloc_dir = &ctx->optional_header.opt64->data_directories[PE_DIR_BASERELOC];
    } else {
        if (PE_DIR_BASERELOC >= ctx->optional_header.opt32->num_data_directories) {
            return STATUS_NOTFOUND;
        }
        reloc_dir = &ctx->optional_header.opt32->data_directories[PE_DIR_BASERELOC];
    }

    if (reloc_dir->size == 0) {
        KLOG_WARN("PE", "No relocation directory, image may not work at new base");
        return STATUS_OK;
    }

    /* Process relocation blocks */
    const uint8_t* reloc_data = (const uint8_t*)pe_rva_to_file_offset(ctx, reloc_dir->virtual_address);
    if (!reloc_data) {
        return STATUS_INVALID;
    }

    uint32_t processed = 0;
    uint64_t reloc_count = 0;

    while (processed < reloc_dir->size) {
        pe_base_relocation_t* block = (pe_base_relocation_t*)(reloc_data + processed);

        if (block->size < sizeof(pe_base_relocation_t)) {
            break;
        }

        uint32_t num_entries = (block->size - sizeof(pe_base_relocation_t)) / sizeof(uint16_t);
        uint16_t* entries = (uint16_t*)(block + 1);

        for (uint32_t i = 0; i < num_entries; i++) {
            uint16_t entry = entries[i];
            uint8_t type = entry >> 12;
            uint16_t offset = entry & 0x0FFF;

            if (type == PE_REL_BASED_ABSOLUTE) {
                continue;  // Skip padding
            }

            uintptr_t target_va = pe_rva_to_va(ctx, block->virtual_address + offset);
            paddr_t target_phys;

            if (FAILED(vmm_get_physical(ctx->aspace, target_va, &target_phys))) {
                continue;
            }

            uint8_t* target = (uint8_t*)PHYS_TO_VIRT_DIRECT(target_phys);
            size_t page_offset = target_va % PAGE_SIZE;

            if (type == PE_REL_BASED_DIR64) {
                uint64_t* value = (uint64_t*)(target + page_offset);
                *value += delta;
                reloc_count++;
            } else if (type == PE_REL_BASED_HIGHLOW) {
                uint32_t* value = (uint32_t*)(target + page_offset);
                *value += (uint32_t)delta;
                reloc_count++;
            }
        }

        processed += block->size;
    }

    /* Update image base */
    ctx->image_base = new_base;
    ctx->entry_point = new_base + (ctx->is_pe32_plus ?
        ctx->optional_header.opt64->entry_point :
        ctx->optional_header.opt32->entry_point);

    /* Update statistics */
    __sync_lock_test_and_set(&g_pe_lock, 1);
    g_pe_stats.relocations_applied += reloc_count;
    __sync_lock_release(&g_pe_lock);

    KLOG_DEBUG("PE", "Applied %lu relocations (delta=0x%lx)", reloc_count, delta);
    return STATUS_OK;
}

/* Resolve imports (stub - requires DLL loading) */
status_t pe_resolve_imports(pe_context_t* ctx) {
    if (!ctx) {
        return STATUS_INVALID;
    }

    /* Get import directory */
    pe_data_directory_t* import_dir;
    if (ctx->is_pe32_plus) {
        if (PE_DIR_IMPORT >= ctx->optional_header.opt64->num_data_directories) {
            return STATUS_OK;  // No imports
        }
        import_dir = &ctx->optional_header.opt64->data_directories[PE_DIR_IMPORT];
    } else {
        if (PE_DIR_IMPORT >= ctx->optional_header.opt32->num_data_directories) {
            return STATUS_OK;  // No imports
        }
        import_dir = &ctx->optional_header.opt32->data_directories[PE_DIR_IMPORT];
    }

    if (import_dir->size == 0) {
        return STATUS_OK;  // No imports
    }

    /* Note: Full import resolution requires DLL loading and symbol lookup */
    /* This is a placeholder for the Win32 persona layer */

    KLOG_DEBUG("PE", "Import resolution deferred to Win32 persona");

    __sync_lock_test_and_set(&g_pe_lock, 1);
    g_pe_stats.imports_resolved++;
    __sync_lock_release(&g_pe_lock);

    return STATUS_OK;
}

/* Free PE context */
void pe_free_context(pe_context_t* ctx) {
    if (!ctx) {
        return;
    }

    pe_free(ctx, sizeof(pe_context_t));
}

/* Get statistics */
status_t pe_get_stats(pe_stats_t* out_stats) {
    if (!out_stats) {
        return STATUS_INVALID;
    }

    __sync_lock_test_and_set(&g_pe_lock, 1);

    out_stats->pe_loaded = g_pe_stats.pe_loaded;
    out_stats->pe32_loaded = g_pe_stats.pe32_loaded;
    out_stats->pe32_plus_loaded = g_pe_stats.pe32_plus_loaded;
    out_stats->dlls_loaded = g_pe_stats.dlls_loaded;
    out_stats->relocations_applied = g_pe_stats.relocations_applied;
    out_stats->imports_resolved = g_pe_stats.imports_resolved;

    __sync_lock_release(&g_pe_lock);

    return STATUS_OK;
}
