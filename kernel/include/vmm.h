#ifndef LIMITLESS_VMM_H
#define LIMITLESS_VMM_H

/*
 * Virtual Memory Manager
 * x86_64 4-level paging implementation
 */

#include "kernel.h"

/* Page table levels */
#define PT_LEVEL_PML4 3  // Page Map Level 4
#define PT_LEVEL_PDPT 2  // Page Directory Pointer Table
#define PT_LEVEL_PD   1  // Page Directory
#define PT_LEVEL_PT   0  // Page Table

/* Page table entry flags */
#define PTE_PRESENT    BIT(0)   // Page is present in memory
#define PTE_WRITE      BIT(1)   // Page is writable
#define PTE_USER       BIT(2)   // Page is accessible from user mode
#define PTE_WRITETHROUGH BIT(3) // Write-through caching
#define PTE_NOCACHE    BIT(4)   // Disable caching
#define PTE_ACCESSED   BIT(5)   // Page has been accessed
#define PTE_DIRTY      BIT(6)   // Page has been written to
#define PTE_HUGE       BIT(7)   // 2MB or 1GB page
#define PTE_GLOBAL     BIT(8)   // Page is global (not flushed on CR3 reload)
#define PTE_NX         BIT(63)  // No execute

/* Page table entry */
typedef uint64_t pte_t;

/* Extract physical address from PTE */
#define PTE_ADDR_MASK 0x000FFFFFFFFFF000ULL
#define PTE_GET_ADDR(pte) ((pte) & PTE_ADDR_MASK)

/* Page table structure (512 entries per level) */
typedef struct page_table {
    pte_t entries[512];
} ALIGNED(4096) page_table_t;

/* Virtual memory region */
typedef struct vm_region {
    vaddr_t start;
    vaddr_t end;
    uint32_t flags;
    uint32_t type;
    struct list_head list_node;
} vm_region_t;

/* VM region types */
#define VM_REGION_CODE     1
#define VM_REGION_DATA     2
#define VM_REGION_HEAP     3
#define VM_REGION_STACK    4
#define VM_REGION_MMAP     5
#define VM_REGION_DEVICE   6
#define VM_REGION_SHARED   7

/* Address space (per-process) */
typedef struct address_space {
    paddr_t pml4_phys;           // Physical address of PML4
    page_table_t* pml4_virt;     // Virtual address of PML4
    struct list_head regions;    // List of VM regions
    size_t total_size;
    uint32_t region_count;
    uint32_t lock;
} address_space_t;

/* TLB operations */
static ALWAYS_INLINE void tlb_flush_all(void) {
    uint64_t cr3;
    __asm__ volatile("mov %%cr3, %0; mov %0, %%cr3" : "=r"(cr3));
}

static ALWAYS_INLINE void tlb_flush_page(vaddr_t addr) {
    __asm__ volatile("invlpg (%0)" : : "r"(addr) : "memory");
}

/* VMM initialization */
status_t vmm_init(void);
status_t vmm_init_kernel_space(void);

/* Address space management */
status_t vmm_create_address_space(address_space_t** out_aspace);
status_t vmm_destroy_address_space(address_space_t* aspace);
status_t vmm_switch_address_space(address_space_t* aspace);
address_space_t* vmm_get_kernel_address_space(void);
address_space_t* vmm_get_current_address_space(void);

/* Page table manipulation */
status_t vmm_map_page(address_space_t* aspace, vaddr_t vaddr, paddr_t paddr, uint32_t flags);
status_t vmm_unmap_page(address_space_t* aspace, vaddr_t vaddr);
status_t vmm_map_pages(address_space_t* aspace, vaddr_t vaddr, paddr_t paddr, size_t count, uint32_t flags);
status_t vmm_unmap_pages(address_space_t* aspace, vaddr_t vaddr, size_t count);

/* Virtual address translation */
status_t vmm_get_physical(address_space_t* aspace, vaddr_t vaddr, paddr_t* out_paddr);
bool vmm_is_mapped(address_space_t* aspace, vaddr_t vaddr);

/* VM region management */
status_t vmm_add_region(address_space_t* aspace, vaddr_t start, size_t size, uint32_t flags, uint32_t type);
status_t vmm_remove_region(address_space_t* aspace, vaddr_t start);
vm_region_t* vmm_find_region(address_space_t* aspace, vaddr_t addr);

/* High-level allocation */
status_t vmm_alloc_pages(address_space_t* aspace, size_t count, uint32_t flags, vaddr_t* out_vaddr);
status_t vmm_free_pages(address_space_t* aspace, vaddr_t vaddr);

/* Page fault handler */
void vmm_page_fault_handler(vaddr_t fault_addr, uint32_t error_code);

/* Kernel mapping utilities */
status_t vmm_map_kernel_region(paddr_t paddr, size_t size, vaddr_t* out_vaddr);
status_t vmm_identity_map(paddr_t start, size_t size, uint32_t flags);

/* Address space cloning (for fork) */
status_t vmm_clone_address_space(address_space_t* src, address_space_t** out_dst);

/* Statistics */
typedef struct vmm_stats {
    size_t total_pages_mapped;
    size_t kernel_pages;
    size_t user_pages;
    size_t page_faults;
    size_t tlb_flushes;
} vmm_stats_t;

void vmm_get_stats(vmm_stats_t* stats);

/* Helper macros for virtual address decomposition */
#define VADDR_PML4_INDEX(addr) (((addr) >> 39) & 0x1FF)
#define VADDR_PDPT_INDEX(addr) (((addr) >> 30) & 0x1FF)
#define VADDR_PD_INDEX(addr)   (((addr) >> 21) & 0x1FF)
#define VADDR_PT_INDEX(addr)   (((addr) >> 12) & 0x1FF)
#define VADDR_OFFSET(addr)     ((addr) & 0xFFF)

/* Helper macros for address conversion */
#define VIRT_TO_PHYS_DIRECT(vaddr) ((paddr_t)((vaddr) - 0xFFFF800000000000ULL))
#define PHYS_TO_VIRT_DIRECT(paddr) ((vaddr_t)((paddr) + 0xFFFF800000000000ULL))

#endif /* LIMITLESS_VMM_H */
