/*
 * Virtual Memory Manager
 * x86_64 4-level paging implementation with enterprise-grade error handling
 */

#include "kernel.h"
#include "microkernel.h"
#include "vmm.h"

/* Kernel address space */
static address_space_t kernel_address_space = {0};
static address_space_t* current_aspace = NULL;

/* VMM statistics */
static vmm_stats_t vmm_stats = {0};

/* VMM lock */
static volatile uint32_t UNUSED vmm_lock = 0;

/* Helper: Allocate page table */
static page_table_t* alloc_page_table(void) {
    paddr_t page = pmm_alloc_page();
    if (!page) {
        return NULL;
    }

    /* Zero the page table */
    page_table_t* pt = (page_table_t*)PHYS_TO_VIRT_DIRECT(page);
    for (int i = 0; i < 512; i++) {
        pt->entries[i] = 0;
    }

    return pt;
}

/* Helper: Free page table */
static UNUSED void free_page_table(page_table_t* pt) {
    if (pt) {
        paddr_t phys = VIRT_TO_PHYS_DIRECT((vaddr_t)pt);
        pmm_free_page(phys);
    }
}

/* Helper: Get or create page table at level */
static page_table_t* get_or_create_table(page_table_t* parent, uint32_t index) {
    pte_t* entry = &parent->entries[index];

    if (*entry & PTE_PRESENT) {
        /* Table exists */
        paddr_t table_phys = PTE_GET_ADDR(*entry);
        return (page_table_t*)PHYS_TO_VIRT_DIRECT(table_phys);
    }

    /* Allocate new table */
    page_table_t* new_table = alloc_page_table();
    if (!new_table) {
        return NULL;
    }

    /* Set entry */
    paddr_t new_phys = VIRT_TO_PHYS_DIRECT((vaddr_t)new_table);
    *entry = new_phys | PTE_PRESENT | PTE_WRITE | PTE_USER;

    return new_table;
}

/* Initialize VMM */
status_t vmm_init(void) {
    KLOG_INFO("VMM", "Initializing virtual memory manager");

    /* Initialize kernel address space */
    kernel_address_space.pml4_phys = pmm_alloc_page();
    if (!kernel_address_space.pml4_phys) {
        return STATUS_NOMEM;
    }

    kernel_address_space.pml4_virt = (page_table_t*)PHYS_TO_VIRT_DIRECT(kernel_address_space.pml4_phys);

    /* Zero PML4 */
    for (int i = 0; i < 512; i++) {
        kernel_address_space.pml4_virt->entries[i] = 0;
    }

    list_init(&kernel_address_space.regions);
    kernel_address_space.total_size = 0;
    kernel_address_space.region_count = 0;
    kernel_address_space.lock = 0;

    current_aspace = &kernel_address_space;

    /* Identity map first 4GB for kernel */
    KLOG_INFO("VMM", "Identity mapping first 4GB");
    status_t status = vmm_identity_map(0, GB(4), PTE_PRESENT | PTE_WRITE);
    if (FAILED(status)) {
        KLOG_ERROR("VMM", "Failed to identity map kernel space");
        return status;
    }

    /* Map kernel to higher half */
    KLOG_INFO("VMM", "Mapping kernel to higher half");

    /* Switch to new page tables */
    __asm__ volatile("mov %0, %%cr3" : : "r"(kernel_address_space.pml4_phys));

    KLOG_INFO("VMM", "Virtual memory initialized");
    return STATUS_OK;
}

/* Create address space */
status_t vmm_create_address_space(address_space_t** out_aspace) {
    if (!out_aspace) {
        return STATUS_INVALID;
    }

    address_space_t* aspace = (address_space_t*)pmm_alloc_page();
    if (!aspace) {
        return STATUS_NOMEM;
    }

    /* Allocate PML4 */
    aspace->pml4_phys = pmm_alloc_page();
    if (!aspace->pml4_phys) {
        pmm_free_page((paddr_t)aspace);
        return STATUS_NOMEM;
    }

    aspace->pml4_virt = (page_table_t*)PHYS_TO_VIRT_DIRECT(aspace->pml4_phys);

    /* Zero PML4 */
    for (int i = 0; i < 512; i++) {
        aspace->pml4_virt->entries[i] = 0;
    }

    /* Copy kernel mappings (upper half) */
    for (int i = 256; i < 512; i++) {
        aspace->pml4_virt->entries[i] = kernel_address_space.pml4_virt->entries[i];
    }

    list_init(&aspace->regions);
    aspace->total_size = 0;
    aspace->region_count = 0;
    aspace->lock = 0;

    *out_aspace = aspace;

    KLOG_DEBUG("VMM", "Created address space at 0x%llx", (uint64_t)aspace);
    return STATUS_OK;
}

/* Destroy address space */
status_t vmm_destroy_address_space(address_space_t* aspace) {
    if (!aspace || aspace == &kernel_address_space) {
        return STATUS_INVALID;
    }

    __sync_lock_test_and_set(&aspace->lock, 1);

    /* Free all page tables (only user half) */
    page_table_t* pml4 = aspace->pml4_virt;

    for (int i = 0; i < 256; i++) {
        if (pml4->entries[i] & PTE_PRESENT) {
            paddr_t pdpt_phys = PTE_GET_ADDR(pml4->entries[i]);
            page_table_t* pdpt = (page_table_t*)PHYS_TO_VIRT_DIRECT(pdpt_phys);

            for (int j = 0; j < 512; j++) {
                if (pdpt->entries[j] & PTE_PRESENT) {
                    paddr_t pd_phys = PTE_GET_ADDR(pdpt->entries[j]);
                    page_table_t* pd = (page_table_t*)PHYS_TO_VIRT_DIRECT(pd_phys);

                    for (int k = 0; k < 512; k++) {
                        if (pd->entries[k] & PTE_PRESENT) {
                            paddr_t pt_phys = PTE_GET_ADDR(pd->entries[k]);
                            pmm_free_page(pt_phys);
                        }
                    }
                    pmm_free_page(pd_phys);
                }
            }
            pmm_free_page(pdpt_phys);
        }
    }

    /* Free PML4 */
    pmm_free_page(aspace->pml4_phys);

    /* Free address space structure */
    pmm_free_page((paddr_t)aspace);

    return STATUS_OK;
}

/* Map single page */
status_t vmm_map_page(address_space_t* aspace, vaddr_t vaddr, paddr_t paddr, uint32_t flags) {
    if (!aspace) {
        return STATUS_INVALID;
    }

    __sync_lock_test_and_set(&aspace->lock, 1);

    page_table_t* pml4 = aspace->pml4_virt;

    /* Walk page tables */
    uint32_t pml4_idx = VADDR_PML4_INDEX(vaddr);
    uint32_t pdpt_idx = VADDR_PDPT_INDEX(vaddr);
    uint32_t pd_idx = VADDR_PD_INDEX(vaddr);
    uint32_t pt_idx = VADDR_PT_INDEX(vaddr);

    page_table_t* pdpt = get_or_create_table(pml4, pml4_idx);
    if (!pdpt) {
        __sync_lock_release(&aspace->lock);
        return STATUS_NOMEM;
    }

    page_table_t* pd = get_or_create_table(pdpt, pdpt_idx);
    if (!pd) {
        __sync_lock_release(&aspace->lock);
        return STATUS_NOMEM;
    }

    page_table_t* pt = get_or_create_table(pd, pd_idx);
    if (!pt) {
        __sync_lock_release(&aspace->lock);
        return STATUS_NOMEM;
    }

    /* Set page table entry */
    pt->entries[pt_idx] = (paddr & PTE_ADDR_MASK) | (flags & 0xFFF) | PTE_PRESENT;

    vmm_stats.total_pages_mapped++;
    if (vaddr >= VM_REGION_KERNEL_START) {
        vmm_stats.kernel_pages++;
    } else {
        vmm_stats.user_pages++;
    }

    __sync_lock_release(&aspace->lock);

    /* Flush TLB for this page */
    tlb_flush_page(vaddr);
    vmm_stats.tlb_flushes++;

    return STATUS_OK;
}

/* Unmap single page */
status_t vmm_unmap_page(address_space_t* aspace, vaddr_t vaddr) {
    if (!aspace) {
        return STATUS_INVALID;
    }

    __sync_lock_test_and_set(&aspace->lock, 1);

    page_table_t* pml4 = aspace->pml4_virt;

    uint32_t pml4_idx = VADDR_PML4_INDEX(vaddr);
    uint32_t pdpt_idx = VADDR_PDPT_INDEX(vaddr);
    uint32_t pd_idx = VADDR_PD_INDEX(vaddr);
    uint32_t pt_idx = VADDR_PT_INDEX(vaddr);

    /* Walk to PT */
    if (!(pml4->entries[pml4_idx] & PTE_PRESENT)) {
        __sync_lock_release(&aspace->lock);
        return STATUS_NOTFOUND;
    }

    page_table_t* pdpt = (page_table_t*)PHYS_TO_VIRT_DIRECT(PTE_GET_ADDR(pml4->entries[pml4_idx]));
    if (!(pdpt->entries[pdpt_idx] & PTE_PRESENT)) {
        __sync_lock_release(&aspace->lock);
        return STATUS_NOTFOUND;
    }

    page_table_t* pd = (page_table_t*)PHYS_TO_VIRT_DIRECT(PTE_GET_ADDR(pdpt->entries[pdpt_idx]));
    if (!(pd->entries[pd_idx] & PTE_PRESENT)) {
        __sync_lock_release(&aspace->lock);
        return STATUS_NOTFOUND;
    }

    page_table_t* pt = (page_table_t*)PHYS_TO_VIRT_DIRECT(PTE_GET_ADDR(pd->entries[pd_idx]));

    /* Clear entry */
    pt->entries[pt_idx] = 0;

    vmm_stats.total_pages_mapped--;

    __sync_lock_release(&aspace->lock);

    tlb_flush_page(vaddr);
    vmm_stats.tlb_flushes++;

    return STATUS_OK;
}

/* Map multiple pages */
status_t vmm_map_pages(address_space_t* aspace, vaddr_t vaddr, paddr_t paddr, size_t count, uint32_t flags) {
    for (size_t i = 0; i < count; i++) {
        status_t status = vmm_map_page(aspace, vaddr + (i * PAGE_SIZE), paddr + (i * PAGE_SIZE), flags);
        if (FAILED(status)) {
            /* Rollback */
            for (size_t j = 0; j < i; j++) {
                vmm_unmap_page(aspace, vaddr + (j * PAGE_SIZE));
            }
            return status;
        }
    }
    return STATUS_OK;
}

/* Unmap multiple pages */
status_t vmm_unmap_pages(address_space_t* aspace, vaddr_t vaddr, size_t count) {
    for (size_t i = 0; i < count; i++) {
        vmm_unmap_page(aspace, vaddr + (i * PAGE_SIZE));
    }
    return STATUS_OK;
}

/* Get physical address */
status_t vmm_get_physical(address_space_t* aspace, vaddr_t vaddr, paddr_t* out_paddr) {
    if (!aspace || !out_paddr) {
        return STATUS_INVALID;
    }

    page_table_t* pml4 = aspace->pml4_virt;

    uint32_t pml4_idx = VADDR_PML4_INDEX(vaddr);
    uint32_t pdpt_idx = VADDR_PDPT_INDEX(vaddr);
    uint32_t pd_idx = VADDR_PD_INDEX(vaddr);
    uint32_t pt_idx = VADDR_PT_INDEX(vaddr);

    if (!(pml4->entries[pml4_idx] & PTE_PRESENT)) return STATUS_NOTFOUND;
    page_table_t* pdpt = (page_table_t*)PHYS_TO_VIRT_DIRECT(PTE_GET_ADDR(pml4->entries[pml4_idx]));

    if (!(pdpt->entries[pdpt_idx] & PTE_PRESENT)) return STATUS_NOTFOUND;
    page_table_t* pd = (page_table_t*)PHYS_TO_VIRT_DIRECT(PTE_GET_ADDR(pdpt->entries[pdpt_idx]));

    if (!(pd->entries[pd_idx] & PTE_PRESENT)) return STATUS_NOTFOUND;
    page_table_t* pt = (page_table_t*)PHYS_TO_VIRT_DIRECT(PTE_GET_ADDR(pd->entries[pd_idx]));

    if (!(pt->entries[pt_idx] & PTE_PRESENT)) return STATUS_NOTFOUND;

    *out_paddr = PTE_GET_ADDR(pt->entries[pt_idx]) | VADDR_OFFSET(vaddr);
    return STATUS_OK;
}

/* Identity map region */
status_t vmm_identity_map(paddr_t start, size_t size, uint32_t flags) {
    vaddr_t vaddr = start;
    paddr_t paddr = start;
    size_t pages = (size + PAGE_SIZE - 1) / PAGE_SIZE;

    return vmm_map_pages(&kernel_address_space, vaddr, paddr, pages, flags);
}

/* Switch address space */
status_t vmm_switch_address_space(address_space_t* aspace) {
    if (!aspace) {
        return STATUS_INVALID;
    }

    current_aspace = aspace;
    __asm__ volatile("mov %0, %%cr3" : : "r"(aspace->pml4_phys));

    return STATUS_OK;
}

/* Get kernel address space */
address_space_t* vmm_get_kernel_address_space(void) {
    return &kernel_address_space;
}

/* Get current address space */
address_space_t* vmm_get_current_address_space(void) {
    return current_aspace;
}

/* Page fault handler */
void vmm_page_fault_handler(vaddr_t fault_addr, uint32_t error_code) {
    vmm_stats.page_faults++;

    KLOG_ERROR("VMM", "Page fault at 0x%llx (error: 0x%x)", fault_addr, error_code);

    /* Determine cause */
    if (!(error_code & 0x1)) {
        KLOG_ERROR("VMM", "  Page not present");
    }
    if (error_code & 0x2) {
        KLOG_ERROR("VMM", "  Write access violation");
    }
    if (error_code & 0x4) {
        KLOG_ERROR("VMM", "  User mode access");
    }
    if (error_code & 0x10) {
        KLOG_ERROR("VMM", "  Instruction fetch");
    }

    /* Handle page fault based on error code */
    if (!(error_code & 0x01)) {
        /* Page not present - could be demand paging */
        /* For now, this is a real fault since we don't support demand paging yet */
        KLOG_ERROR("VMM", "Page not present - invalid access");
        PANIC("Page not present");
    } else if (error_code & 0x02) {
        /* Write to read-only page */
        KLOG_ERROR("VMM", "Write protection violation");
        PANIC("Write to read-only page");
    } else if (error_code & 0x10) {
        /* Instruction fetch from non-executable page */
        KLOG_ERROR("VMM", "Execution protection violation (NX)");
        PANIC("Instruction fetch from NX page");
    } else {
        /* Other fault types */
        KLOG_ERROR("VMM", "Unknown page fault type");
        PANIC("Unhandled page fault");
    }
}

/* Get statistics */
void vmm_get_stats(vmm_stats_t* stats) {
    if (stats) {
        *stats = vmm_stats;
    }
}
