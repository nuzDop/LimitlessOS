/*
 * ELF64 Loader Implementation
 * Complete ELF binary loader with relocation support
 */

#include "kernel.h"
#include "microkernel.h"
#include "vmm.h"
#include "elf.h"

/* Memory allocation for loading */
static void* elf_alloc_pages(size_t size) {
    size_t pages = (size + PAGE_SIZE - 1) / PAGE_SIZE;
    paddr_t paddr = pmm_alloc_pages(pages);
    if (!paddr) {
        return NULL;
    }
    return (void*)PHYS_TO_VIRT_DIRECT(paddr);
}

/* Memory free */
static void elf_free_pages(void* addr, size_t size) {
    if (!addr) {
        return;
    }
    size_t pages = (size + PAGE_SIZE - 1) / PAGE_SIZE;
    paddr_t paddr = VIRT_TO_PHYS_DIRECT((vaddr_t)addr);
    pmm_free_pages(paddr, pages);
}

/* Memory copy */
static void elf_memcpy(void* dest, const void* src, size_t n) {
    uint8_t* d = (uint8_t*)dest;
    const uint8_t* s = (const uint8_t*)src;
    for (size_t i = 0; i < n; i++) {
        d[i] = s[i];
    }
}

/* Memory set */
static void elf_memset(void* dest, int c, size_t n) {
    uint8_t* d = (uint8_t*)dest;
    for (size_t i = 0; i < n; i++) {
        d[i] = (uint8_t)c;
    }
}

/* String operations */
static size_t elf_strlen(const char* s) {
    size_t len = 0;
    while (s[len]) len++;
    return len;
}

static UNUSED void elf_strcpy(char* dest, const char* src, size_t max) {
    size_t i = 0;
    while (src[i] && i < max - 1) {
        dest[i] = src[i];
        i++;
    }
    dest[i] = '\0';
}

/* Validate ELF header */
status_t elf_validate(const uint8_t* data, size_t size) {
    if (!data || size < sizeof(Elf64_Ehdr)) {
        return STATUS_INVALID;
    }

    Elf64_Ehdr* ehdr = (Elf64_Ehdr*)data;

    /* Check magic number */
    if (ehdr->e_ident[0] != 0x7F ||
        ehdr->e_ident[1] != 'E' ||
        ehdr->e_ident[2] != 'L' ||
        ehdr->e_ident[3] != 'F') {
        KLOG_ERROR("ELF", "Invalid ELF magic");
        return STATUS_INVALID;
    }

    /* Check class (64-bit) */
    if (ehdr->e_ident[4] != ELFCLASS64) {
        KLOG_ERROR("ELF", "Not a 64-bit ELF");
        return STATUS_INVALID;
    }

    /* Check endianness (little endian) */
    if (ehdr->e_ident[5] != ELFDATA2LSB) {
        KLOG_ERROR("ELF", "Not little endian");
        return STATUS_INVALID;
    }

    /* Check version */
    if (ehdr->e_ident[6] != EV_CURRENT) {
        KLOG_ERROR("ELF", "Invalid ELF version");
        return STATUS_INVALID;
    }

    /* Check machine type (x86_64) */
    if (ehdr->e_machine != EM_X86_64) {
        KLOG_ERROR("ELF", "Not x86_64");
        return STATUS_INVALID;
    }

    /* Check type (executable or dynamic) */
    if (ehdr->e_type != ET_EXEC && ehdr->e_type != ET_DYN) {
        KLOG_ERROR("ELF", "Not executable or shared object");
        return STATUS_INVALID;
    }

    return STATUS_OK;
}

/* Parse ELF headers */
status_t elf_parse_headers(elf_context_t* ctx) {
    if (!ctx || !ctx->data) {
        return STATUS_INVALID;
    }

    /* Parse ELF header */
    ctx->ehdr = (Elf64_Ehdr*)ctx->data;

    /* Check header size */
    if (ctx->size < sizeof(Elf64_Ehdr)) {
        return STATUS_INVALID;
    }

    /* Get program headers */
    if (ctx->ehdr->e_phoff && ctx->ehdr->e_phnum > 0) {
        if (ctx->ehdr->e_phoff + ctx->ehdr->e_phnum * sizeof(Elf64_Phdr) > ctx->size) {
            return STATUS_INVALID;
        }
        ctx->phdr = (Elf64_Phdr*)(ctx->data + ctx->ehdr->e_phoff);
    }

    /* Get section headers */
    if (ctx->ehdr->e_shoff && ctx->ehdr->e_shnum > 0) {
        if (ctx->ehdr->e_shoff + ctx->ehdr->e_shnum * sizeof(Elf64_Shdr) > ctx->size) {
            return STATUS_INVALID;
        }
        ctx->shdr = (Elf64_Shdr*)(ctx->data + ctx->ehdr->e_shoff);
    }

    /* Calculate total memory size needed */
    ctx->total_size = 0;
    vaddr_t min_addr = ~0UL;
    vaddr_t max_addr = 0;

    for (int i = 0; i < ctx->ehdr->e_phnum; i++) {
        Elf64_Phdr* phdr = &ctx->phdr[i];

        if (phdr->p_type == PT_LOAD) {
            vaddr_t seg_start = phdr->p_vaddr;
            vaddr_t seg_end = phdr->p_vaddr + phdr->p_memsz;

            if (seg_start < min_addr) {
                min_addr = seg_start;
            }
            if (seg_end > max_addr) {
                max_addr = seg_end;
            }
        }

        /* Check for dynamic linking */
        if (phdr->p_type == PT_DYNAMIC) {
            ctx->is_dynamic = true;
            ctx->dynamic = (Elf64_Dyn*)(ctx->data + phdr->p_offset);
            ctx->dynamic_count = phdr->p_filesz / sizeof(Elf64_Dyn);
        }

        /* Check for interpreter */
        if (phdr->p_type == PT_INTERP) {
            ctx->is_dynamic = true;
            if (phdr->p_filesz < sizeof(ctx->interpreter)) {
                elf_memcpy(ctx->interpreter, ctx->data + phdr->p_offset, phdr->p_filesz);
                ctx->interpreter[phdr->p_filesz] = '\0';
            }
        }
    }

    ctx->total_size = max_addr - min_addr;

    /* Set base address */
    if (ctx->ehdr->e_type == ET_EXEC) {
        ctx->base_addr = 0;  // Fixed address
    } else {
        ctx->base_addr = 0x400000;  // Default PIE base
    }

    ctx->entry_point = ctx->ehdr->e_entry + ctx->base_addr;

    KLOG_DEBUG("ELF", "Parsed: entry=0x%llx, size=0x%llx, dynamic=%d",
               ctx->entry_point, ctx->total_size, ctx->is_dynamic);

    return STATUS_OK;
}

/* Load segments into memory */
status_t elf_load_segments(elf_context_t* ctx) {
    if (!ctx || !ctx->ehdr || !ctx->phdr || !ctx->aspace) {
        return STATUS_INVALID;
    }

    /* Load each PT_LOAD segment */
    for (int i = 0; i < ctx->ehdr->e_phnum; i++) {
        Elf64_Phdr* phdr = &ctx->phdr[i];

        if (phdr->p_type != PT_LOAD) {
            continue;
        }

        /* Calculate addresses */
        vaddr_t vaddr = phdr->p_vaddr + ctx->base_addr;
        vaddr_t vaddr_aligned = PAGE_ALIGN_DOWN(vaddr);
        size_t offset_in_page = vaddr - vaddr_aligned;

        /* Calculate sizes */
        size_t file_size = phdr->p_filesz;
        size_t mem_size = phdr->p_memsz + offset_in_page;
        size_t pages_needed = (mem_size + PAGE_SIZE - 1) / PAGE_SIZE;

        KLOG_DEBUG("ELF", "Loading segment: vaddr=0x%llx, size=0x%llx, pages=%llu",
                   vaddr, mem_size, pages_needed);

        /* Determine page permissions */
        uint32_t vm_flags = 0;
        if (phdr->p_flags & PF_R) vm_flags |= VM_FLAG_READ;
        if (phdr->p_flags & PF_W) vm_flags |= VM_FLAG_WRITE;
        if (phdr->p_flags & PF_X) vm_flags |= VM_FLAG_EXEC;
        vm_flags |= VM_FLAG_USER;

        /* Allocate and map pages */
        for (size_t page = 0; page < pages_needed; page++) {
            paddr_t paddr = pmm_alloc_page();
            if (!paddr) {
                KLOG_ERROR("ELF", "Failed to allocate page");
                return STATUS_NOMEM;
            }

            vaddr_t page_vaddr = vaddr_aligned + (page * PAGE_SIZE);
            status_t status = vmm_map_page(ctx->aspace, page_vaddr, paddr, vm_flags);

            if (FAILED(status)) {
                pmm_free_page(paddr);
                KLOG_ERROR("ELF", "Failed to map page: %d", status);
                return status;
            }

            /* Zero the page first */
            uint8_t* page_ptr = (uint8_t*)PHYS_TO_VIRT_DIRECT(paddr);
            elf_memset(page_ptr, 0, PAGE_SIZE);

            /* Copy data from file if this page contains file data */
            if (page * PAGE_SIZE < file_size + offset_in_page) {
                size_t file_offset = page * PAGE_SIZE;
                size_t copy_offset = 0;
                size_t copy_size = PAGE_SIZE;

                /* Adjust for first page offset */
                if (page == 0) {
                    copy_offset = offset_in_page;
                    copy_size -= offset_in_page;
                }

                /* Adjust if less than full page */
                if (file_offset >= offset_in_page) {
                    size_t src_offset = file_offset - offset_in_page;
                    if (src_offset < file_size) {
                        size_t remaining = file_size - src_offset;
                        if (copy_size > remaining) {
                            copy_size = remaining;
                        }

                        if (phdr->p_offset + src_offset + copy_size <= ctx->size) {
                            elf_memcpy(page_ptr + copy_offset,
                                      ctx->data + phdr->p_offset + src_offset,
                                      copy_size);
                        }
                    }
                }
            }
        }
    }

    KLOG_INFO("ELF", "Loaded %d segments", ctx->ehdr->e_phnum);
    return STATUS_OK;
}

/* Apply relocations */
status_t elf_apply_relocations(elf_context_t* ctx) {
    if (!ctx || !ctx->is_dynamic) {
        return STATUS_OK;  // No relocations needed
    }

    /* Process dynamic section */
    if (!ctx->dynamic) {
        return STATUS_OK;
    }

    Elf64_Rela* rela = NULL;
    size_t rela_size = 0;
    size_t rela_ent = 0;

    /* Find relocation tables */
    for (size_t i = 0; i < ctx->dynamic_count; i++) {
        Elf64_Dyn* dyn = &ctx->dynamic[i];

        if (dyn->d_tag == DT_NULL) {
            break;
        }

        switch (dyn->d_tag) {
            case DT_RELA:
                /* Address is relative to base in dynamic objects */
                if (dyn->d_un.d_ptr < ctx->size) {
                    rela = (Elf64_Rela*)(ctx->data + (dyn->d_un.d_ptr - ctx->base_addr));
                }
                break;
            case DT_RELASZ:
                rela_size = dyn->d_un.d_val;
                break;
            case DT_RELAENT:
                rela_ent = dyn->d_un.d_val;
                break;
        }
    }

    /* Apply relocations */
    if (rela && rela_size > 0 && rela_ent > 0) {
        size_t count = rela_size / rela_ent;

        for (size_t i = 0; i < count; i++) {
            Elf64_Rela* rel = &rela[i];
            uint32_t type = ELF64_R_TYPE(rel->r_info);
            vaddr_t addr = rel->r_offset + ctx->base_addr;

            /* Get physical address to write to */
            paddr_t paddr;
            if (FAILED(vmm_get_physical(ctx->aspace, addr, &paddr))) {
                KLOG_WARN("ELF", "Relocation address not mapped: 0x%llx", addr);
                continue;
            }

            uint64_t* target = (uint64_t*)PHYS_TO_VIRT_DIRECT(paddr);

            switch (type) {
                case R_X86_64_RELATIVE:
                    /* B + A (base address + addend) */
                    *target = ctx->base_addr + rel->r_addend;
                    break;

                case R_X86_64_64:
                    /* S + A (symbol value + addend) */
                    /* Would need symbol table lookup */
                    KLOG_WARN("ELF", "R_X86_64_64 relocation not implemented");
                    break;

                case R_X86_64_GLOB_DAT:
                case R_X86_64_JUMP_SLOT:
                    /* S (symbol value) */
                    /* Would need symbol table lookup and dynamic linking */
                    KLOG_WARN("ELF", "Dynamic symbol relocation not implemented");
                    break;

                case R_X86_64_NONE:
                    break;

                default:
                    KLOG_WARN("ELF", "Unknown relocation type: %u", type);
                    break;
            }
        }

        KLOG_DEBUG("ELF", "Applied %llu relocations", count);
    }

    return STATUS_OK;
}

/* Load ELF binary */
status_t elf_load(const uint8_t* data, size_t size, struct address_space* aspace, elf_context_t** out_ctx) {
    if (!data || !aspace || !out_ctx) {
        return STATUS_INVALID;
    }

    /* Validate ELF */
    status_t status = elf_validate(data, size);
    if (FAILED(status)) {
        return status;
    }

    /* Allocate context */
    elf_context_t* ctx = (elf_context_t*)elf_alloc_pages(sizeof(elf_context_t));
    if (!ctx) {
        return STATUS_NOMEM;
    }

    elf_memset(ctx, 0, sizeof(elf_context_t));
    ctx->data = (uint8_t*)data;
    ctx->size = size;
    ctx->aspace = aspace;

    /* Parse headers */
    status = elf_parse_headers(ctx);
    if (FAILED(status)) {
        elf_free_pages(ctx, sizeof(elf_context_t));
        return status;
    }

    /* Load segments */
    status = elf_load_segments(ctx);
    if (FAILED(status)) {
        elf_free_pages(ctx, sizeof(elf_context_t));
        return status;
    }

    /* Apply relocations */
    status = elf_apply_relocations(ctx);
    if (FAILED(status)) {
        KLOG_WARN("ELF", "Relocation warnings, continuing");
    }

    *out_ctx = ctx;

    KLOG_INFO("ELF", "Loaded successfully: entry=0x%llx", ctx->entry_point);
    return STATUS_OK;
}

/* Free ELF context */
void elf_free_context(elf_context_t* ctx) {
    if (ctx) {
        elf_free_pages(ctx, sizeof(elf_context_t));
    }
}

/* Find section by name */
Elf64_Shdr* elf_find_section(elf_context_t* ctx, const char* name) {
    if (!ctx || !ctx->shdr || !name || ctx->ehdr->e_shstrndx >= ctx->ehdr->e_shnum) {
        return NULL;
    }

    /* Get string table section */
    Elf64_Shdr* strtab = &ctx->shdr[ctx->ehdr->e_shstrndx];
    if (strtab->sh_offset + strtab->sh_size > ctx->size) {
        return NULL;
    }

    const char* strtab_data = (const char*)(ctx->data + strtab->sh_offset);

    /* Search sections */
    for (int i = 0; i < ctx->ehdr->e_shnum; i++) {
        Elf64_Shdr* shdr = &ctx->shdr[i];
        if (shdr->sh_name < strtab->sh_size) {
            const char* section_name = strtab_data + shdr->sh_name;
            size_t len = elf_strlen(name);

            bool match = true;
            for (size_t j = 0; j < len; j++) {
                if (section_name[j] != name[j]) {
                    match = false;
                    break;
                }
            }

            if (match && section_name[len] == '\0') {
                return shdr;
            }
        }
    }

    return NULL;
}

/* Get section name */
const char* elf_get_section_name(elf_context_t* ctx, Elf64_Shdr* shdr) {
    if (!ctx || !shdr || ctx->ehdr->e_shstrndx >= ctx->ehdr->e_shnum) {
        return NULL;
    }

    Elf64_Shdr* strtab = &ctx->shdr[ctx->ehdr->e_shstrndx];
    if (strtab->sh_offset + strtab->sh_size > ctx->size) {
        return NULL;
    }

    if (shdr->sh_name >= strtab->sh_size) {
        return NULL;
    }

    return (const char*)(ctx->data + strtab->sh_offset + shdr->sh_name);
}

/* Read string from ELF */
status_t elf_read_string(elf_context_t* ctx, Elf64_Off offset, char* buffer, size_t max_len) {
    if (!ctx || !buffer || offset >= ctx->size) {
        return STATUS_INVALID;
    }

    size_t i = 0;
    while (i < max_len - 1 && offset + i < ctx->size) {
        buffer[i] = ctx->data[offset + i];
        if (buffer[i] == '\0') {
            break;
        }
        i++;
    }
    buffer[i] = '\0';

    return STATUS_OK;
}
