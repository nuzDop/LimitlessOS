/*
 * Physical Memory Manager (PMM)
 * Enterprise-grade bitmap-based physical page allocator
 */

#include "kernel.h"
#include "microkernel.h"

/* Physical memory bitmap */
static uint8_t* pmm_bitmap = NULL;
static size_t pmm_bitmap_size = 0;
static paddr_t pmm_base_addr = 0;
static size_t pmm_total_pages = 0;
static size_t pmm_free_page_count = 0;
static size_t pmm_used_pages = 0;

/* Spinlock for PMM (will be implemented properly with SMP support) */
static volatile uint32_t pmm_lock = 0;

/* Bitmap operations */
static ALWAYS_INLINE void bitmap_set(size_t index) {
    pmm_bitmap[index / 8] |= (1 << (index % 8));
}

static ALWAYS_INLINE void bitmap_clear(size_t index) {
    pmm_bitmap[index / 8] &= ~(1 << (index % 8));
}

static ALWAYS_INLINE bool bitmap_test(size_t index) {
    return (pmm_bitmap[index / 8] & (1 << (index % 8))) != 0;
}

/* Find first free page */
static size_t find_free_page(void) {
    for (size_t i = 0; i < pmm_total_pages; i++) {
        if (!bitmap_test(i)) {
            return i;
        }
    }
    return (size_t)-1;
}

/* Find consecutive free pages */
static size_t find_free_pages(size_t count) {
    size_t start = 0;
    size_t found = 0;

    for (size_t i = 0; i < pmm_total_pages; i++) {
        if (!bitmap_test(i)) {
            if (found == 0) {
                start = i;
            }
            found++;
            if (found == count) {
                return start;
            }
        } else {
            found = 0;
        }
    }

    return (size_t)-1;
}

/* Initialize physical memory manager */
status_t pmm_init(paddr_t start, size_t size) {
    if (size == 0 || start == 0) {
        return STATUS_INVALID;
    }

    /* Align to page boundaries */
    paddr_t aligned_start = PAGE_ALIGN_UP(start);
    size_t aligned_size = PAGE_ALIGN_DOWN(size - (aligned_start - start));

    pmm_base_addr = aligned_start;
    pmm_total_pages = aligned_size / PAGE_SIZE;

    /* Calculate bitmap size (1 bit per page) */
    pmm_bitmap_size = (pmm_total_pages + 7) / 8;

    /* Place bitmap at the start of physical memory */
    pmm_bitmap = (uint8_t*)aligned_start;

    /* Mark bitmap pages as used */
    size_t bitmap_pages = (pmm_bitmap_size + PAGE_SIZE - 1) / PAGE_SIZE;

    /* Initialize bitmap - all pages free */
    for (size_t i = 0; i < pmm_bitmap_size; i++) {
        pmm_bitmap[i] = 0;
    }

    /* Mark bitmap area as used */
    for (size_t i = 0; i < bitmap_pages; i++) {
        bitmap_set(i);
        pmm_used_pages++;
    }

    pmm_free_page_count = pmm_total_pages - bitmap_pages;

    return STATUS_OK;
}

/* Allocate a single physical page */
paddr_t pmm_alloc_page(void) {
    /* Acquire lock (simplified for now) */
    __sync_lock_test_and_set(&pmm_lock, 1);

    if (pmm_free_page_count == 0) {
        __sync_lock_release(&pmm_lock);
        return 0;
    }

    size_t page_index = find_free_page();
    if (page_index == (size_t)-1) {
        __sync_lock_release(&pmm_lock);
        return 0;
    }

    bitmap_set(page_index);
    pmm_free_page_count--;
    pmm_used_pages++;

    paddr_t page_addr = pmm_base_addr + (page_index * PAGE_SIZE);

    __sync_lock_release(&pmm_lock);

    return page_addr;
}

/* Free a single physical page */
void pmm_free_page(paddr_t page) {
    if (page < pmm_base_addr || page >= pmm_base_addr + (pmm_total_pages * PAGE_SIZE)) {
        return;
    }

    __sync_lock_test_and_set(&pmm_lock, 1);

    size_t page_index = (page - pmm_base_addr) / PAGE_SIZE;

    if (!bitmap_test(page_index)) {
        /* Double free - ignore */
        __sync_lock_release(&pmm_lock);
        return;
    }

    bitmap_clear(page_index);
    pmm_free_page_count++;
    pmm_used_pages--;

    __sync_lock_release(&pmm_lock);
}

/* Allocate multiple consecutive pages */
paddr_t pmm_alloc_pages(size_t count) {
    if (count == 0) {
        return 0;
    }

    if (count == 1) {
        return pmm_alloc_page();
    }

    __sync_lock_test_and_set(&pmm_lock, 1);

    if (pmm_free_page_count < count) {
        __sync_lock_release(&pmm_lock);
        return 0;
    }

    size_t start_index = find_free_pages(count);
    if (start_index == (size_t)-1) {
        __sync_lock_release(&pmm_lock);
        return 0;
    }

    /* Mark pages as used */
    for (size_t i = 0; i < count; i++) {
        bitmap_set(start_index + i);
    }

    pmm_free_page_count -= count;
    pmm_used_pages += count;

    paddr_t start_addr = pmm_base_addr + (start_index * PAGE_SIZE);

    __sync_lock_release(&pmm_lock);

    return start_addr;
}

/* Free multiple consecutive pages */
void pmm_free_pages(paddr_t base, size_t count) {
    if (count == 0) {
        return;
    }

    if (base < pmm_base_addr || base >= pmm_base_addr + (pmm_total_pages * PAGE_SIZE)) {
        return;
    }

    __sync_lock_test_and_set(&pmm_lock, 1);

    size_t start_index = (base - pmm_base_addr) / PAGE_SIZE;

    for (size_t i = 0; i < count; i++) {
        size_t index = start_index + i;
        if (index < pmm_total_pages && bitmap_test(index)) {
            bitmap_clear(index);
            pmm_free_page_count++;
            pmm_used_pages--;
        }
    }

    __sync_lock_release(&pmm_lock);
}

/* Get free memory size */
size_t pmm_get_free_memory(void) {
    return pmm_free_page_count * PAGE_SIZE;
}

/* Get total memory size */
size_t pmm_get_total_memory(void) {
    return pmm_total_pages * PAGE_SIZE;
}

/* Get used memory size */
size_t pmm_get_used_memory(void) {
    return pmm_used_pages * PAGE_SIZE;
}

/* PMM statistics */
void pmm_get_stats(size_t* total, size_t* used, size_t* free) {
    if (total) *total = pmm_total_pages;
    if (used) *used = pmm_used_pages;
    if (free) *free = pmm_free_page_count;
}
