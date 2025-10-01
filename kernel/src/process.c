/*
 * Process Manager Implementation
 * Full process lifecycle management with fork/exec
 */

#include "kernel.h"
#include "microkernel.h"
#include "process.h"
#include "vmm.h"
#include "elf.h"
#include "vfs.h"

/* Global process manager */
static process_manager_t process_manager = {0};

/* Helper: Memory copy */
static UNUSED void process_memcpy(void* dest, const void* src, size_t n) {
    uint8_t* d = (uint8_t*)dest;
    const uint8_t* s = (const uint8_t*)src;
    for (size_t i = 0; i < n; i++) {
        d[i] = s[i];
    }
}

/* Helper: Memory set */
static void process_memset(void* dest, int val, size_t n) {
    uint8_t* d = (uint8_t*)dest;
    for (size_t i = 0; i < n; i++) {
        d[i] = (uint8_t)val;
    }
}

/* Helper: String length */
static UNUSED size_t process_strlen(const char* str) {
    size_t len = 0;
    while (str && str[len]) len++;
    return len;
}

/* Helper: String copy */
static void process_strcpy(char* dest, const char* src, size_t max) {
    size_t i = 0;
    while (src && src[i] && i < max - 1) {
        dest[i] = src[i];
        i++;
    }
    dest[i] = '\0';
}

/* Initialize process manager */
status_t process_init(void) {
    if (process_manager.initialized) {
        return STATUS_EXISTS;
    }

    /* Initialize all processes */
    for (uint32_t i = 0; i < PROCESS_MAX_COUNT; i++) {
        process_manager.processes[i].in_use = false;
        process_manager.processes[i].pid = 0;
        process_manager.processes[i].lock = 0;
    }

    process_manager.process_count = 0;
    process_manager.next_pid = 1;
    process_manager.next_tid = 1;
    process_manager.current_process = NULL;
    process_manager.current_thread = NULL;
    process_manager.lock = 0;
    list_init(&process_manager.process_list);
    process_manager.initialized = true;

    KLOG_INFO("PROCESS", "Process manager initialized");
    return STATUS_OK;
}

/* Shutdown process manager */
void process_shutdown(void) {
    if (!process_manager.initialized) {
        return;
    }

    /* Terminate all processes */
    for (uint32_t i = 0; i < PROCESS_MAX_COUNT; i++) {
        if (process_manager.processes[i].in_use) {
            process_t* proc = &process_manager.processes[i];

            /* Free address space */
            if (proc->aspace) {
                vmm_destroy_address_space(proc->aspace);
            }

            /* Free ELF context */
            if (proc->elf_ctx) {
                elf_free_context(proc->elf_ctx);
            }

            proc->in_use = false;
        }
    }

    process_manager.initialized = false;
    KLOG_INFO("PROCESS", "Process manager shutdown");
}

/* Allocate new process structure */
status_t process_create(process_t** out_process) {
    if (!out_process) {
        return STATUS_INVALID;
    }

    __sync_lock_test_and_set(&process_manager.lock, 1);

    /* Find free slot */
    process_t* process = NULL;
    for (uint32_t i = 0; i < PROCESS_MAX_COUNT; i++) {
        if (!process_manager.processes[i].in_use) {
            process = &process_manager.processes[i];
            break;
        }
    }

    if (!process) {
        __sync_lock_release(&process_manager.lock);
        return STATUS_NOMEM;
    }

    /* Initialize process structure */
    process_memset(process, 0, sizeof(process_t));

    process->pid = process_manager.next_pid++;
    process->parent_pid = 0;
    process->pgid = process->pid;
    process->sid = process->pid;
    process->state = PROCESS_STATE_EMBRYO;
    process->exit_code = 0;
    process->flags = PROCESS_FLAG_USER;
    process->aspace = NULL;
    process->elf_ctx = NULL;
    process->heap_start = 0;
    process->heap_end = 0;
    process->brk = 0;
    process->thread_count = 0;
    process->main_thread = NULL;
    process->fd_count = 0;
    process->child_count = 0;
    process->uid = 0;
    process->gid = 0;
    process->euid = 0;
    process->egid = 0;
    process->start_time = 0;
    process->cpu_time = 0;
    process->lock = 0;
    process->in_use = true;

    /* Initialize file descriptors */
    for (uint32_t i = 0; i < PROCESS_MAX_FDS; i++) {
        process->fds[i].in_use = false;
        process->fds[i].file = NULL;
        process->fds[i].flags = 0;
        process->fds[i].ref_count = 0;
    }

    /* Initialize threads */
    for (uint32_t i = 0; i < PROCESS_MAX_THREADS; i++) {
        process->threads[i].in_use = false;
        process->threads[i].tid = 0;
    }

    /* Set working directory to root */
    process_strcpy(process->cwd, "/", VFS_MAX_PATH);

    /* Add to process list */
    list_add(&process->list_node, &process_manager.process_list);
    process_manager.process_count++;

    __sync_lock_release(&process_manager.lock);

    *out_process = process;
    KLOG_INFO("PROCESS", "Created process PID %d", process->pid);
    return STATUS_OK;
}

/* Fork process (duplicate) */
status_t process_fork(process_t* parent, process_t** out_child) {
    if (!parent || !out_child) {
        return STATUS_INVALID;
    }

    /* Create new process */
    process_t* child = NULL;
    status_t status = process_create(&child);
    if (FAILED(status)) {
        return status;
    }

    /* Set parent-child relationship */
    child->parent_pid = parent->pid;
    child->pgid = parent->pgid;
    child->sid = parent->sid;

    /* Copy credentials */
    child->uid = parent->uid;
    child->gid = parent->gid;
    child->euid = parent->euid;
    child->egid = parent->egid;

    /* Copy working directory */
    process_strcpy(child->cwd, parent->cwd, VFS_MAX_PATH);

    /* Copy address space (COW would be implemented here) */
    if (parent->aspace) {
        status = process_copy_address_space(parent->aspace, &child->aspace);
        if (FAILED(status)) {
            child->in_use = false;
            process_manager.process_count--;
            return status;
        }
    }

    /* Copy heap information */
    child->heap_start = parent->heap_start;
    child->heap_end = parent->heap_end;
    child->brk = parent->brk;

    /* Clone file descriptors */
    status = process_fd_clone_all(parent, child);
    if (FAILED(status)) {
        if (child->aspace) {
            vmm_destroy_address_space(child->aspace);
        }
        child->in_use = false;
        process_manager.process_count--;
        return status;
    }

    /* Add child to parent's child list */
    process_add_child(parent, child->pid);

    /* Set child state to ready */
    child->state = PROCESS_STATE_READY;

    *out_child = child;
    KLOG_INFO("PROCESS", "Forked process: parent PID %d -> child PID %d", parent->pid, child->pid);
    return STATUS_OK;
}

/* Execute new program */
status_t process_exec(process_t* process, const char* path, char* const argv[], char* const envp[]) {
    if (!process || !path) {
        return STATUS_INVALID;
    }

    KLOG_INFO("PROCESS", "Exec: PID %d loading %s", process->pid, path);

    /* Open executable file */
    vfs_file_t* file = NULL;
    status_t status = vfs_open(path, VFS_O_RDONLY, 0, &file);
    if (FAILED(status)) {
        KLOG_ERROR("PROCESS", "Failed to open executable: %s", path);
        return status;
    }

    /* Get file size */
    vfs_stat_t stat;
    status = vfs_fstat(file, &stat);
    if (FAILED(status)) {
        vfs_close(file);
        return status;
    }

    /* Allocate buffer for ELF file */
    size_t file_size = stat.size;
    size_t pages_needed = (file_size + PAGE_SIZE - 1) / PAGE_SIZE;
    uint8_t* elf_data = (uint8_t*)pmm_alloc_pages(pages_needed);
    if (!elf_data) {
        vfs_close(file);
        return STATUS_NOMEM;
    }

    /* Read entire file */
    ssize_t bytes_read = vfs_read(file, elf_data, file_size);
    vfs_close(file);

    if (bytes_read < 0 || (size_t)bytes_read != file_size) {
        pmm_free_pages((paddr_t)elf_data, pages_needed);
        return STATUS_ERROR;
    }

    /* Validate ELF */
    status = elf_validate(elf_data, file_size);
    if (FAILED(status)) {
        KLOG_ERROR("PROCESS", "Invalid ELF file: %s", path);
        pmm_free_pages((paddr_t)elf_data, pages_needed);
        return status;
    }

    /* Destroy old address space if exists */
    if (process->aspace) {
        vmm_destroy_address_space(process->aspace);
        process->aspace = NULL;
    }

    /* Free old ELF context if exists */
    if (process->elf_ctx) {
        elf_free_context(process->elf_ctx);
        process->elf_ctx = NULL;
    }

    /* Create new address space */
    status = vmm_create_address_space(&process->aspace);
    if (FAILED(status)) {
        pmm_free_pages((paddr_t)elf_data, pages_needed);
        return status;
    }

    /* Load ELF binary */
    elf_context_t* elf_ctx = NULL;
    status = elf_load(elf_data, file_size, process->aspace, &elf_ctx);

    /* Free ELF data buffer */
    pmm_free_pages((paddr_t)elf_data, pages_needed);

    if (FAILED(status)) {
        KLOG_ERROR("PROCESS", "Failed to load ELF: %s", path);
        vmm_destroy_address_space(process->aspace);
        process->aspace = NULL;
        return status;
    }

    process->elf_ctx = elf_ctx;

    /* Setup heap (place after loaded segments) */
    process->heap_start = elf_ctx->base_addr + elf_ctx->total_size;
    process->heap_start = (process->heap_start + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
    process->heap_end = process->heap_start;
    process->brk = process->heap_start;

    /* Setup user stack */
    vaddr_t stack_ptr = 0;
    status = process_setup_user_stack(process, &stack_ptr);
    if (FAILED(status)) {
        elf_free_context(elf_ctx);
        vmm_destroy_address_space(process->aspace);
        process->aspace = NULL;
        process->elf_ctx = NULL;
        return status;
    }

    /* Create main thread */
    thread_t* main_thread = NULL;
    status = thread_create(process, elf_ctx->entry_point, stack_ptr, &main_thread);
    if (FAILED(status)) {
        elf_free_context(elf_ctx);
        vmm_destroy_address_space(process->aspace);
        process->aspace = NULL;
        process->elf_ctx = NULL;
        return status;
    }

    process->main_thread = main_thread;
    process->state = PROCESS_STATE_READY;

    KLOG_INFO("PROCESS", "Exec complete: PID %d, entry=0x%lx, stack=0x%lx",
              process->pid, elf_ctx->entry_point, stack_ptr);
    return STATUS_OK;
}

/* Exit process */
status_t process_exit(process_t* process, int exit_code) {
    if (!process) {
        return STATUS_INVALID;
    }

    KLOG_INFO("PROCESS", "Process %d exiting with code %d", process->pid, exit_code);

    __sync_lock_test_and_set(&process->lock, 1);

    process->state = PROCESS_STATE_ZOMBIE;
    process->exit_code = exit_code;

    /* Close all file descriptors */
    for (uint32_t i = 0; i < PROCESS_MAX_FDS; i++) {
        if (process->fds[i].in_use) {
            process_fd_free(process, i);
        }
    }

    /* Mark all threads as dead */
    for (uint32_t i = 0; i < PROCESS_MAX_THREADS; i++) {
        if (process->threads[i].in_use) {
            process->threads[i].state = PROCESS_STATE_DEAD;
        }
    }

    /* Reparent children to init (PID 1) */
    for (uint32_t i = 0; i < process->child_count; i++) {
        process_t* child = process_find_by_pid(process->children[i]);
        if (child) {
            child->parent_pid = 1;
        }
    }

    __sync_lock_release(&process->lock);

    /* If no parent, directly free resources */
    process_t* parent = process_find_by_pid(process->parent_pid);
    if (!parent) {
        /* Free address space */
        if (process->aspace) {
            vmm_destroy_address_space(process->aspace);
            process->aspace = NULL;
        }

        /* Free ELF context */
        if (process->elf_ctx) {
            elf_free_context(process->elf_ctx);
            process->elf_ctx = NULL;
        }

        process->state = PROCESS_STATE_DEAD;
    }

    return STATUS_OK;
}

/* Wait for child process */
status_t process_wait(process_t* parent, pid_t child_pid, int* status, uint32_t options) {
    if (!parent) {
        return STATUS_INVALID;
    }

    /* Find zombie child */
    process_t* child = NULL;

    if (child_pid > 0) {
        /* Wait for specific child */
        child = process_find_by_pid(child_pid);
        if (!child || child->parent_pid != parent->pid) {
            return STATUS_NOTFOUND;
        }

        if (child->state != PROCESS_STATE_ZOMBIE) {
            return STATUS_INVALID;  // Would block in real implementation
        }
    } else {
        /* Wait for any child */
        for (uint32_t i = 0; i < parent->child_count; i++) {
            process_t* candidate = process_find_by_pid(parent->children[i]);
            if (candidate && candidate->state == PROCESS_STATE_ZOMBIE) {
                child = candidate;
                break;
            }
        }

        if (!child) {
            return STATUS_INVALID;  // Would block in real implementation
        }
    }

    /* Get exit status */
    if (status) {
        *status = child->exit_code;
    }

    /* Remove from parent's child list */
    process_remove_child(parent, child->pid);

    /* Free child resources */
    if (child->aspace) {
        vmm_destroy_address_space(child->aspace);
        child->aspace = NULL;
    }

    if (child->elf_ctx) {
        elf_free_context(child->elf_ctx);
        child->elf_ctx = NULL;
    }

    /* Remove from process list */
    __sync_lock_test_and_set(&process_manager.lock, 1);
    list_del(&child->list_node);
    process_manager.process_count--;
    __sync_lock_release(&process_manager.lock);

    child->state = PROCESS_STATE_DEAD;
    child->in_use = false;

    KLOG_INFO("PROCESS", "Reaped zombie process PID %d (exit code: %d)", child->pid, child->exit_code);
    return STATUS_OK;
}

/* Kill process */
status_t process_kill(pid_t pid, int signal) {
    process_t* process = process_find_by_pid(pid);
    if (!process) {
        return STATUS_NOTFOUND;
    }

    /* For now, simply exit the process */
    return process_exit(process, signal);
}

/* Create thread */
status_t thread_create(process_t* process, vaddr_t entry_point, vaddr_t stack_ptr, thread_t** out_thread) {
    if (!process || !out_thread) {
        return STATUS_INVALID;
    }

    __sync_lock_test_and_set(&process->lock, 1);

    /* Find free thread slot */
    thread_t* thread = NULL;
    for (uint32_t i = 0; i < PROCESS_MAX_THREADS; i++) {
        if (!process->threads[i].in_use) {
            thread = &process->threads[i];
            break;
        }
    }

    if (!thread) {
        __sync_lock_release(&process->lock);
        return STATUS_NOMEM;
    }

    /* Initialize thread */
    thread->tid = process_manager.next_tid++;
    thread->entry_point = entry_point;
    thread->user_stack = stack_ptr;
    thread->kernel_stack = 0;  // Would allocate kernel stack
    thread->state = PROCESS_STATE_READY;
    thread->priority = 10;
    thread->cpu_time = 0;
    thread->process = process;
    thread->in_use = true;

    process->thread_count++;

    __sync_lock_release(&process->lock);

    *out_thread = thread;
    KLOG_INFO("PROCESS", "Created thread TID %d for PID %d", thread->tid, process->pid);
    return STATUS_OK;
}

/* Exit thread */
status_t thread_exit(thread_t* thread, int exit_code) {
    if (!thread || !thread->process) {
        return STATUS_INVALID;
    }

    thread->state = PROCESS_STATE_DEAD;
    thread->in_use = false;
    thread->process->thread_count--;

    /* If last thread, exit process */
    if (thread->process->thread_count == 0) {
        return process_exit(thread->process, exit_code);
    }

    return STATUS_OK;
}

/* Find process by PID */
process_t* process_find_by_pid(pid_t pid) {
    for (uint32_t i = 0; i < PROCESS_MAX_COUNT; i++) {
        if (process_manager.processes[i].in_use &&
            process_manager.processes[i].pid == pid) {
            return &process_manager.processes[i];
        }
    }
    return NULL;
}

/* Get current process */
process_t* process_get_current(void) {
    return process_manager.current_process;
}

/* Get current thread */
thread_t* thread_get_current(void) {
    return process_manager.current_thread;
}

/* Allocate file descriptor */
int process_fd_alloc(process_t* process, void* file, uint32_t flags) {
    if (!process || !file) {
        return -1;
    }

    __sync_lock_test_and_set(&process->lock, 1);

    /* Find free FD slot */
    for (int i = 0; i < PROCESS_MAX_FDS; i++) {
        if (!process->fds[i].in_use) {
            process->fds[i].file = file;
            process->fds[i].flags = flags;
            process->fds[i].ref_count = 1;
            process->fds[i].in_use = true;
            process->fd_count++;

            __sync_lock_release(&process->lock);
            return i;
        }
    }

    __sync_lock_release(&process->lock);
    return -1;
}

/* Free file descriptor */
status_t process_fd_free(process_t* process, int fd) {
    if (!process || fd < 0 || fd >= PROCESS_MAX_FDS) {
        return STATUS_INVALID;
    }

    __sync_lock_test_and_set(&process->lock, 1);

    if (!process->fds[fd].in_use) {
        __sync_lock_release(&process->lock);
        return STATUS_NOTFOUND;
    }

    /* Close file if this is the last reference */
    if (process->fds[fd].ref_count == 1 && process->fds[fd].file) {
        vfs_close((vfs_file_t*)process->fds[fd].file);
    }

    process->fds[fd].in_use = false;
    process->fds[fd].file = NULL;
    process->fds[fd].flags = 0;
    process->fds[fd].ref_count = 0;
    process->fd_count--;

    __sync_lock_release(&process->lock);
    return STATUS_OK;
}

/* Get file descriptor */
file_descriptor_t* process_fd_get(process_t* process, int fd) {
    if (!process || fd < 0 || fd >= PROCESS_MAX_FDS) {
        return NULL;
    }

    if (!process->fds[fd].in_use) {
        return NULL;
    }

    return &process->fds[fd];
}

/* Duplicate file descriptor */
status_t process_fd_dup(process_t* process, int oldfd, int newfd) {
    if (!process || oldfd < 0 || oldfd >= PROCESS_MAX_FDS) {
        return STATUS_INVALID;
    }

    if (!process->fds[oldfd].in_use) {
        return STATUS_NOTFOUND;
    }

    __sync_lock_test_and_set(&process->lock, 1);

    /* Close newfd if it's open */
    if (newfd >= 0 && newfd < PROCESS_MAX_FDS && process->fds[newfd].in_use) {
        process_fd_free(process, newfd);
    }

    /* Allocate new fd if newfd is -1 */
    if (newfd < 0) {
        newfd = process_fd_alloc(process, process->fds[oldfd].file, process->fds[oldfd].flags);
    } else {
        /* Use specified newfd */
        process->fds[newfd] = process->fds[oldfd];
        process->fds[newfd].ref_count++;
        process->fd_count++;
    }

    __sync_lock_release(&process->lock);
    return (newfd >= 0) ? STATUS_OK : STATUS_NOMEM;
}

/* Clone all file descriptors */
status_t process_fd_clone_all(process_t* parent, process_t* child) {
    if (!parent || !child) {
        return STATUS_INVALID;
    }

    for (int i = 0; i < PROCESS_MAX_FDS; i++) {
        if (parent->fds[i].in_use) {
            child->fds[i] = parent->fds[i];
            child->fds[i].ref_count++;
            child->fd_count++;
        }
    }

    return STATUS_OK;
}

/* Adjust process break (heap) */
status_t process_brk(process_t* process, vaddr_t new_brk, vaddr_t* out_brk) {
    if (!process || !out_brk) {
        return STATUS_INVALID;
    }

    /* If new_brk is 0, just return current brk */
    if (new_brk == 0) {
        *out_brk = process->brk;
        return STATUS_OK;
    }

    /* Validate new_brk is within heap region */
    if (new_brk < process->heap_start) {
        *out_brk = process->brk;
        return STATUS_INVALID;
    }

    /* Calculate pages needed */
    vaddr_t old_brk_page = (process->brk + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
    vaddr_t new_brk_page = (new_brk + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);

    /* Expanding heap */
    if (new_brk_page > old_brk_page) {
        size_t pages_needed = (new_brk_page - old_brk_page) / PAGE_SIZE;

        for (size_t i = 0; i < pages_needed; i++) {
            vaddr_t vaddr = old_brk_page + (i * PAGE_SIZE);
            paddr_t paddr = pmm_alloc_page();

            if (!paddr) {
                *out_brk = process->brk;
                return STATUS_NOMEM;
            }

            uint32_t flags = PTE_USER | PTE_WRITE;
            status_t status = vmm_map_page(process->aspace, vaddr, paddr, flags);

            if (FAILED(status)) {
                pmm_free_page(paddr);
                *out_brk = process->brk;
                return status;
            }
        }
    }
    /* Shrinking heap */
    else if (new_brk_page < old_brk_page) {
        size_t pages_to_free = (old_brk_page - new_brk_page) / PAGE_SIZE;

        for (size_t i = 0; i < pages_to_free; i++) {
            vaddr_t vaddr = new_brk_page + (i * PAGE_SIZE);
            vmm_unmap_page(process->aspace, vaddr);
        }
    }

    process->brk = new_brk;
    process->heap_end = new_brk_page;
    *out_brk = new_brk;

    return STATUS_OK;
}

/* Copy address space (simple copy, COW would be better) */
status_t process_copy_address_space(address_space_t* src, address_space_t** out_dest) {
    if (!src || !out_dest) {
        return STATUS_INVALID;
    }

    /* Create new address space */
    address_space_t* dest = NULL;
    status_t status = vmm_create_address_space(&dest);
    if (FAILED(status)) {
        return status;
    }

    /* Copy all user-space pages from source to destination */
    /* Walk through PML4 entries looking for user pages */
    page_table_t* src_pml4 = src->pml4_virt;
    page_table_t* dst_pml4 = dest->pml4_virt;

    /* Iterate through user-space PML4 entries (first 256 entries) */
    for (int pml4_idx = 0; pml4_idx < 256; pml4_idx++) {
        pte_t pml4e = src_pml4->entries[pml4_idx];
        if (!(pml4e & PTE_PRESENT)) continue;
        if (!(pml4e & PTE_USER)) continue;

        /* Get PDPT */
        paddr_t src_pdpt_phys = PTE_GET_ADDR(pml4e);
        page_table_t* src_pdpt = (page_table_t*)PHYS_TO_VIRT_DIRECT(src_pdpt_phys);

        /* Allocate new PDPT for dest */
        paddr_t dst_pdpt_phys = pmm_alloc_page();
        if (!dst_pdpt_phys) {
            vmm_destroy_address_space(dest);
            return STATUS_NOMEM;
        }

        page_table_t* dst_pdpt = (page_table_t*)PHYS_TO_VIRT_DIRECT(dst_pdpt_phys);
        for (int i = 0; i < 512; i++) dst_pdpt->entries[i] = 0;

        dst_pml4->entries[pml4_idx] = dst_pdpt_phys | (pml4e & 0xFFF);

        /* Iterate through PDPT entries */
        for (int pdpt_idx = 0; pdpt_idx < 512; pdpt_idx++) {
            pte_t pdpte = src_pdpt->entries[pdpt_idx];
            if (!(pdpte & PTE_PRESENT)) continue;

            /* Get PD */
            paddr_t src_pd_phys = PTE_GET_ADDR(pdpte);
            page_table_t* src_pd = (page_table_t*)PHYS_TO_VIRT_DIRECT(src_pd_phys);

            /* Allocate new PD for dest */
            paddr_t dst_pd_phys = pmm_alloc_page();
            if (!dst_pd_phys) {
                vmm_destroy_address_space(dest);
                return STATUS_NOMEM;
            }

            page_table_t* dst_pd = (page_table_t*)PHYS_TO_VIRT_DIRECT(dst_pd_phys);
            for (int i = 0; i < 512; i++) dst_pd->entries[i] = 0;

            dst_pdpt->entries[pdpt_idx] = dst_pd_phys | (pdpte & 0xFFF);

            /* Iterate through PD entries */
            for (int pd_idx = 0; pd_idx < 512; pd_idx++) {
                pte_t pde = src_pd->entries[pd_idx];
                if (!(pde & PTE_PRESENT)) continue;

                /* Get PT */
                paddr_t src_pt_phys = PTE_GET_ADDR(pde);
                page_table_t* src_pt = (page_table_t*)PHYS_TO_VIRT_DIRECT(src_pt_phys);

                /* Allocate new PT for dest */
                paddr_t dst_pt_phys = pmm_alloc_page();
                if (!dst_pt_phys) {
                    vmm_destroy_address_space(dest);
                    return STATUS_NOMEM;
                }

                page_table_t* dst_pt = (page_table_t*)PHYS_TO_VIRT_DIRECT(dst_pt_phys);
                for (int i = 0; i < 512; i++) dst_pt->entries[i] = 0;

                dst_pd->entries[pd_idx] = dst_pt_phys | (pde & 0xFFF);

                /* Copy actual pages */
                for (int pt_idx = 0; pt_idx < 512; pt_idx++) {
                    pte_t pte = src_pt->entries[pt_idx];
                    if (!(pte & PTE_PRESENT)) continue;

                    /* Allocate new page */
                    paddr_t src_page = PTE_GET_ADDR(pte);
                    paddr_t dst_page = pmm_alloc_page();
                    if (!dst_page) {
                        vmm_destroy_address_space(dest);
                        return STATUS_NOMEM;
                    }

                    /* Copy page contents */
                    uint8_t* src_ptr = (uint8_t*)PHYS_TO_VIRT_DIRECT(src_page);
                    uint8_t* dst_ptr = (uint8_t*)PHYS_TO_VIRT_DIRECT(dst_page);
                    for (size_t i = 0; i < PAGE_SIZE; i++) {
                        dst_ptr[i] = src_ptr[i];
                    }

                    /* Set page table entry */
                    dst_pt->entries[pt_idx] = dst_page | (pte & 0xFFF);
                }
            }
        }
    }

    *out_dest = dest;
    return STATUS_OK;
}

/* Setup user stack */
status_t process_setup_user_stack(process_t* process, vaddr_t* out_stack_ptr) {
    if (!process || !out_stack_ptr) {
        return STATUS_INVALID;
    }

    /* User stack at high address */
    vaddr_t stack_top = 0x7FFFFFFFE000UL;
    size_t stack_pages = 64;  // 256KB stack

    /* Allocate and map stack pages */
    for (size_t i = 0; i < stack_pages; i++) {
        vaddr_t vaddr = stack_top - ((i + 1) * PAGE_SIZE);
        paddr_t paddr = pmm_alloc_page();

        if (!paddr) {
            return STATUS_NOMEM;
        }

        uint32_t flags = PTE_USER | PTE_WRITE;
        status_t status = vmm_map_page(process->aspace, vaddr, paddr, flags);

        if (FAILED(status)) {
            pmm_free_page(paddr);
            return status;
        }
    }

    *out_stack_ptr = stack_top;
    return STATUS_OK;
}

/* Add child to parent */
status_t process_add_child(process_t* parent, pid_t child_pid) {
    if (!parent) {
        return STATUS_INVALID;
    }

    __sync_lock_test_and_set(&parent->lock, 1);

    if (parent->child_count >= PROCESS_MAX_CHILDREN) {
        __sync_lock_release(&parent->lock);
        return STATUS_NOMEM;
    }

    parent->children[parent->child_count++] = child_pid;

    __sync_lock_release(&parent->lock);
    return STATUS_OK;
}

/* Remove child from parent */
status_t process_remove_child(process_t* parent, pid_t child_pid) {
    if (!parent) {
        return STATUS_INVALID;
    }

    __sync_lock_test_and_set(&parent->lock, 1);

    /* Find and remove child */
    for (uint32_t i = 0; i < parent->child_count; i++) {
        if (parent->children[i] == child_pid) {
            /* Shift remaining children */
            for (uint32_t j = i; j < parent->child_count - 1; j++) {
                parent->children[j] = parent->children[j + 1];
            }
            parent->child_count--;
            break;
        }
    }

    __sync_lock_release(&parent->lock);
    return STATUS_OK;
}

/* Cleanup zombie processes */
void process_cleanup_zombies(void) {
    for (uint32_t i = 0; i < PROCESS_MAX_COUNT; i++) {
        if (process_manager.processes[i].in_use &&
            process_manager.processes[i].state == PROCESS_STATE_ZOMBIE) {

            process_t* zombie = &process_manager.processes[i];

            /* If parent doesn't exist, clean up immediately */
            process_t* parent = process_find_by_pid(zombie->parent_pid);
            if (!parent) {
                if (zombie->aspace) {
                    vmm_destroy_address_space(zombie->aspace);
                    zombie->aspace = NULL;
                }

                if (zombie->elf_ctx) {
                    elf_free_context(zombie->elf_ctx);
                    zombie->elf_ctx = NULL;
                }

                zombie->state = PROCESS_STATE_DEAD;
                zombie->in_use = false;
                process_manager.process_count--;
            }
        }
    }
}

/* Get process statistics */
status_t process_get_stats(process_stats_t* stats) {
    if (!stats) {
        return STATUS_INVALID;
    }

    process_memset(stats, 0, sizeof(process_stats_t));

    for (uint32_t i = 0; i < PROCESS_MAX_COUNT; i++) {
        if (process_manager.processes[i].in_use) {
            stats->total_processes++;

            if (process_manager.processes[i].state == PROCESS_STATE_ZOMBIE) {
                stats->zombie_processes++;
            } else if (process_manager.processes[i].state != PROCESS_STATE_DEAD) {
                stats->active_processes++;
            }

            stats->total_threads += process_manager.processes[i].thread_count;
            stats->total_cpu_time += process_manager.processes[i].cpu_time;
        }
    }

    return STATUS_OK;
}
