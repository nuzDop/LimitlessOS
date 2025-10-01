/*
 * POSIX Persona Implementation
 * Translates POSIX syscalls to LimitlessOS microkernel primitives
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "posix.h"

/* Global POSIX state */
static struct {
    bool initialized;
    uint32_t process_count;
} posix_state = {0};

/* Initialize POSIX persona */
status_t posix_init(void) {
    if (posix_state.initialized) {
        return STATUS_EXISTS;
    }

    printf("[POSIX] Initializing POSIX persona\n");

    posix_state.initialized = true;
    posix_state.process_count = 0;

    printf("[POSIX] Persona initialized\n");
    return STATUS_OK;
}

/* Create POSIX context */
status_t posix_create_context(posix_context_t** out_ctx) {
    if (!out_ctx) {
        return STATUS_INVALID;
    }

    posix_context_t* ctx = (posix_context_t*)calloc(1, sizeof(posix_context_t));
    if (!ctx) {
        return STATUS_NOMEM;
    }

    /* Initialize context */
    ctx->pid = 1000 + posix_state.process_count++;
    ctx->ppid = 1;
    ctx->uid = 1000;
    ctx->gid = 1000;
    ctx->euid = ctx->uid;
    ctx->egid = ctx->gid;

    /* Initialize FDs */
    for (int i = 0; i < MAX_FDS; i++) {
        ctx->fds[i].fd = i;
        ctx->fds[i].active = false;
        ctx->fds[i].flags = 0;
        ctx->fds[i].private_data = NULL;
    }

    /* Setup standard FDs */
    ctx->fds[STDIN_FILENO].active = true;
    ctx->fds[STDOUT_FILENO].active = true;
    ctx->fds[STDERR_FILENO].active = true;
    ctx->next_fd = 3;

    /* Set working directory */
    strcpy(ctx->cwd, "/");

    /* Initialize environment */
    ctx->environ = NULL;
    ctx->environ_count = 0;

    /* Initialize memory */
    ctx->brk_start = 0x40000000;
    ctx->brk_current = ctx->brk_start;

    *out_ctx = ctx;

    printf("[POSIX] Created context for PID %llu\n", ctx->pid);
    return STATUS_OK;
}

/* Destroy POSIX context */
status_t posix_destroy_context(posix_context_t* ctx) {
    if (!ctx) {
        return STATUS_INVALID;
    }

    /* Close all FDs */
    for (int i = 0; i < MAX_FDS; i++) {
        if (ctx->fds[i].active) {
            posix_free_fd(ctx, i);
        }
    }

    /* Free environment */
    if (ctx->environ) {
        for (int i = 0; i < ctx->environ_count; i++) {
            free(ctx->environ[i]);
        }
        free(ctx->environ);
    }

    free(ctx);

    printf("[POSIX] Destroyed context\n");
    return STATUS_OK;
}

/* Allocate file descriptor */
int posix_alloc_fd(posix_context_t* ctx) {
    if (!ctx) {
        return -EINVAL;
    }

    for (int i = ctx->next_fd; i < MAX_FDS; i++) {
        if (!ctx->fds[i].active) {
            ctx->fds[i].active = true;
            ctx->fds[i].flags = 0;
            ctx->fds[i].private_data = NULL;
            ctx->next_fd = i + 1;
            return i;
        }
    }

    return -EMFILE;
}

/* Free file descriptor */
void posix_free_fd(posix_context_t* ctx, int fd) {
    if (ctx && fd >= 0 && fd < MAX_FDS) {
        ctx->fds[fd].active = false;
        ctx->fds[fd].flags = 0;
        if (ctx->fds[fd].private_data) {
            free(ctx->fds[fd].private_data);
            ctx->fds[fd].private_data = NULL;
        }
    }
}

/* Get FD entry */
fd_entry_t* posix_get_fd(posix_context_t* ctx, int fd) {
    if (!ctx || fd < 0 || fd >= MAX_FDS || !ctx->fds[fd].active) {
        return NULL;
    }
    return &ctx->fds[fd];
}

/* Syscall dispatcher */
int64_t posix_syscall(posix_context_t* ctx, uint64_t syscall_num,
                      uint64_t arg1, uint64_t arg2, uint64_t arg3,
                      uint64_t arg4, uint64_t arg5, uint64_t arg6) {
    if (!ctx) {
        return -EINVAL;
    }

    switch (syscall_num) {
        case SYS_read:
            return posix_sys_read(ctx, (int)arg1, (void*)arg2, (size_t)arg3);

        case SYS_write:
            return posix_sys_write(ctx, (int)arg1, (const void*)arg2, (size_t)arg3);

        case SYS_open:
            return posix_sys_open(ctx, (const char*)arg1, (int)arg2, (int)arg3);

        case SYS_close:
            return posix_sys_close(ctx, (int)arg1);

        case SYS_getpid:
            return posix_sys_getpid(ctx);

        case SYS_getuid:
            return posix_sys_getuid(ctx);

        case SYS_getgid:
            return posix_sys_getgid(ctx);

        case SYS_geteuid:
            return ctx->euid;

        case SYS_getegid:
            return ctx->egid;

        case SYS_brk:
            return posix_sys_brk(ctx, arg1);

        case SYS_exit:
            return posix_sys_exit(ctx, (int)arg1);

        case SYS_fork:
            return posix_sys_fork(ctx);

        case SYS_execve:
            return posix_sys_execve(ctx, (const char*)arg1, (char* const*)arg2, (char* const*)arg3);

        case SYS_sched_yield:
            /* TODO: Call kernel scheduler */
            return 0;

        default:
            printf("[POSIX] Unimplemented syscall: %llu\n", syscall_num);
            return -ENOSYS;
    }
}

/* Syscall: read */
int64_t posix_sys_read(posix_context_t* ctx, int fd, void* buf, size_t count) {
    fd_entry_t* fde = posix_get_fd(ctx, fd);
    if (!fde) {
        return -EBADF;
    }

    if (!buf) {
        return -EFAULT;
    }

    /* TODO: Implement actual file I/O via VFS */
    if (fd == STDIN_FILENO) {
        /* Read from stdin */
        return fread(buf, 1, count, stdin);
    }

    return -ENOSYS;
}

/* Syscall: write */
int64_t posix_sys_write(posix_context_t* ctx, int fd, const void* buf, size_t count) {
    fd_entry_t* fde = posix_get_fd(ctx, fd);
    if (!fde) {
        return -EBADF;
    }

    if (!buf) {
        return -EFAULT;
    }

    /* TODO: Implement actual file I/O via VFS */
    if (fd == STDOUT_FILENO) {
        return fwrite(buf, 1, count, stdout);
    } else if (fd == STDERR_FILENO) {
        return fwrite(buf, 1, count, stderr);
    }

    return -ENOSYS;
}

/* Syscall: open */
int64_t posix_sys_open(posix_context_t* ctx, const char* path, int flags, int mode) {
    if (!path) {
        return -EFAULT;
    }

    int fd = posix_alloc_fd(ctx);
    if (fd < 0) {
        return fd;
    }

    /* TODO: Open file via VFS */
    printf("[POSIX] open(%s, %d, %d) -> fd %d (stub)\n", path, flags, mode, fd);

    return fd;
}

/* Syscall: close */
int64_t posix_sys_close(posix_context_t* ctx, int fd) {
    if (fd < 0 || fd >= 3) {  // Don't close stdin/stdout/stderr
        fd_entry_t* fde = posix_get_fd(ctx, fd);
        if (!fde) {
            return -EBADF;
        }

        posix_free_fd(ctx, fd);
        return 0;
    }

    return -EINVAL;
}

/* Syscall: getpid */
int64_t posix_sys_getpid(posix_context_t* ctx) {
    return ctx->pid;
}

/* Syscall: getuid */
int64_t posix_sys_getuid(posix_context_t* ctx) {
    return ctx->uid;
}

/* Syscall: getgid */
int64_t posix_sys_getgid(posix_context_t* ctx) {
    return ctx->gid;
}

/* Syscall: brk */
int64_t posix_sys_brk(posix_context_t* ctx, uint64_t addr) {
    if (addr == 0) {
        /* Query current brk */
        return ctx->brk_current;
    }

    if (addr < ctx->brk_start) {
        return -EINVAL;
    }

    /* TODO: Actually allocate/free memory via VMM */
    ctx->brk_current = addr;

    return addr;
}

/* Syscall: exit */
int64_t posix_sys_exit(posix_context_t* ctx, int status) {
    printf("[POSIX] Process %llu exited with status %d\n", ctx->pid, status);

    /* TODO: Terminate process via kernel */
    exit(status);

    return 0;  // Never reached
}

/* Syscall: fork */
int64_t posix_sys_fork(posix_context_t* ctx) {
    printf("[POSIX] fork() called (not implemented)\n");

    /* TODO: Implement via kernel process creation */
    return -ENOSYS;
}

/* Syscall: execve */
int64_t posix_sys_execve(posix_context_t* ctx, const char* path, char* const argv[], char* const envp[]) {
    if (!path) {
        return -EFAULT;
    }

    printf("[POSIX] execve(%s) called (not implemented)\n", path);

    /* TODO: Load ELF and replace process image */
    return -ENOSYS;
}

/* Load ELF binary (stub) */
status_t posix_load_elf(const char* path, elf_info_t* info) {
    if (!path || !info) {
        return STATUS_INVALID;
    }

    printf("[POSIX] Loading ELF: %s (stub)\n", path);

    /* TODO: Implement ELF parser and loader */
    info->entry_point = 0x400000;
    info->base_address = 0x400000;
    info->load_size = 0x100000;
    info->is_dynamic = false;
    info->interpreter[0] = '\0';

    return STATUS_NOSUPPORT;
}

/* Execute program */
status_t posix_exec(posix_context_t* ctx, const char* path, char* const argv[], char* const envp[]) {
    if (!ctx || !path) {
        return STATUS_INVALID;
    }

    elf_info_t elf_info;
    status_t status = posix_load_elf(path, &elf_info);

    if (FAILED(status)) {
        return status;
    }

    printf("[POSIX] Would execute %s at 0x%llx\n", path, elf_info.entry_point);

    /* TODO: Setup stack, load ELF, jump to entry point */

    return STATUS_NOSUPPORT;
}
