/*
 * macOS Persona Implementation
 * Translates Darwin/BSD syscalls and Mach traps to LimitlessOS syscalls
 */

#include "macos_persona.h"
#include "posix_persona.h"
#include <string.h>
#include <stdlib.h>

/* Global macOS context */
static macos_context_t g_macos_ctx = {0};
static macos_stats_t g_macos_stats = {0};

/* Initialize macOS persona */
int macos_persona_init(void) {
    /* Initialize context */
    for (int i = 0; i < sizeof(g_macos_ctx); i++) {
        ((uint8_t*)&g_macos_ctx)[i] = 0;
    }

    g_macos_ctx.pid = getpid();
    g_macos_ctx.ppid = getppid();
    g_macos_ctx.max_fds = 1024;
    g_macos_ctx.darwin_errno = 0;

    /* Initialize Mach ports (fake for now) */
    g_macos_ctx.task_port = 0x1000;
    g_macos_ctx.thread_port = 0x2000;
    g_macos_ctx.host_port = 0x3000;
    g_macos_ctx.bootstrap_port = 0x4000;

    return 0;
}

/* Shutdown macOS persona */
int macos_persona_shutdown(void) {
    return 0;
}

/* Get macOS context */
macos_context_t* macos_get_context(void) {
    return &g_macos_ctx;
}

/* Get errno pointer */
int* __darwin_errno(void) {
    return &g_macos_ctx.darwin_errno;
}

/* Convert Darwin errno to POSIX errno */
int darwin_errno_to_posix(int darwin_err) {
    /* Most Darwin errno values match POSIX */
    return darwin_err;
}

/* Convert POSIX errno to Darwin errno */
int posix_errno_to_darwin(int posix_err) {
    /* Most POSIX errno values match Darwin */
    return posix_err;
}

/* Translate open flags */
static int translate_open_flags(int darwin_flags) {
    int posix_flags = 0;

    if ((darwin_flags & DARWIN_O_RDWR) == DARWIN_O_RDWR) {
        posix_flags |= 0x0002;  // O_RDWR
    } else if (darwin_flags & DARWIN_O_WRONLY) {
        posix_flags |= 0x0001;  // O_WRONLY
    } else {
        posix_flags |= 0x0000;  // O_RDONLY
    }

    if (darwin_flags & DARWIN_O_CREAT) posix_flags |= 0x0100;
    if (darwin_flags & DARWIN_O_TRUNC) posix_flags |= 0x0200;
    if (darwin_flags & DARWIN_O_APPEND) posix_flags |= 0x0400;
    if (darwin_flags & DARWIN_O_EXCL) posix_flags |= 0x0800;

    return posix_flags;
}

/* Translate mmap protection */
static int translate_mmap_prot(int darwin_prot) {
    int posix_prot = 0;

    if (darwin_prot & DARWIN_PROT_READ) posix_prot |= 0x01;
    if (darwin_prot & DARWIN_PROT_WRITE) posix_prot |= 0x02;
    if (darwin_prot & DARWIN_PROT_EXEC) posix_prot |= 0x04;

    return posix_prot;
}

/* Translate mmap flags */
static int translate_mmap_flags(int darwin_flags) {
    int posix_flags = 0;

    if (darwin_flags & DARWIN_MAP_SHARED) posix_flags |= 0x01;
    if (darwin_flags & DARWIN_MAP_PRIVATE) posix_flags |= 0x02;
    if (darwin_flags & DARWIN_MAP_FIXED) posix_flags |= 0x10;
    if (darwin_flags & DARWIN_MAP_ANON) posix_flags |= 0x20;

    return posix_flags;
}

/* BSD Syscall Implementations */

int darwin_open(const char* path, int flags, int mode) {
    int posix_flags = translate_open_flags(flags);
    int fd = open(path, posix_flags, mode);

    if (fd < 0) {
        darwin_errno = posix_errno_to_darwin(errno);
        return -1;
    }

    g_macos_stats.syscalls_translated++;
    g_macos_stats.bsd_syscalls++;
    g_macos_stats.files_opened++;

    return fd;
}

int darwin_close(int fd) {
    int result = close(fd);

    if (result < 0) {
        darwin_errno = posix_errno_to_darwin(errno);
        return -1;
    }

    g_macos_stats.syscalls_translated++;
    g_macos_stats.bsd_syscalls++;

    return result;
}

long darwin_read(int fd, void* buf, size_t count) {
    ssize_t result = read(fd, buf, count);

    if (result < 0) {
        darwin_errno = posix_errno_to_darwin(errno);
        return -1;
    }

    g_macos_stats.syscalls_translated++;
    g_macos_stats.bsd_syscalls++;

    return result;
}

long darwin_write(int fd, const void* buf, size_t count) {
    ssize_t result = write(fd, buf, count);

    if (result < 0) {
        darwin_errno = posix_errno_to_darwin(errno);
        return -1;
    }

    g_macos_stats.syscalls_translated++;
    g_macos_stats.bsd_syscalls++;

    return result;
}

long darwin_lseek(int fd, long offset, int whence) {
    off_t result = lseek(fd, offset, whence);

    if (result < 0) {
        darwin_errno = posix_errno_to_darwin(errno);
        return -1;
    }

    g_macos_stats.syscalls_translated++;
    g_macos_stats.bsd_syscalls++;

    return result;
}

int darwin_fork(void) {
    pid_t result = fork();

    if (result < 0) {
        darwin_errno = posix_errno_to_darwin(errno);
        return -1;
    }

    g_macos_stats.syscalls_translated++;
    g_macos_stats.bsd_syscalls++;

    if (result == 0) {
        /* Child process */
        g_macos_ctx.pid = getpid();
        g_macos_ctx.ppid = getppid();
    } else {
        g_macos_stats.processes_created++;
    }

    return result;
}

int darwin_execve(const char* path, char* const argv[], char* const envp[]) {
    int result = execve(path, argv, envp);

    /* execve only returns on error */
    darwin_errno = posix_errno_to_darwin(errno);

    g_macos_stats.syscalls_translated++;
    g_macos_stats.bsd_syscalls++;

    return result;
}

void darwin_exit(int status) {
    exit(status);
}

int darwin_wait4(int pid, int* status, int options, void* rusage) {
    (void)rusage;  // Not fully supported

    pid_t result = waitpid((pid_t)pid, status, options);

    if (result < 0) {
        darwin_errno = posix_errno_to_darwin(errno);
        return -1;
    }

    g_macos_stats.syscalls_translated++;
    g_macos_stats.bsd_syscalls++;

    return result;
}

int darwin_getpid(void) {
    g_macos_stats.syscalls_translated++;
    g_macos_stats.bsd_syscalls++;
    return getpid();
}

int darwin_getppid(void) {
    g_macos_stats.syscalls_translated++;
    g_macos_stats.bsd_syscalls++;
    return getppid();
}

int darwin_kill(int pid, int sig) {
    int result = kill((pid_t)pid, sig);

    if (result < 0) {
        darwin_errno = posix_errno_to_darwin(errno);
        return -1;
    }

    g_macos_stats.syscalls_translated++;
    g_macos_stats.bsd_syscalls++;

    return result;
}

int darwin_dup(int fd) {
    int result = dup(fd);

    if (result < 0) {
        darwin_errno = posix_errno_to_darwin(errno);
        return -1;
    }

    g_macos_stats.syscalls_translated++;
    g_macos_stats.bsd_syscalls++;

    return result;
}

int darwin_dup2(int oldfd, int newfd) {
    int result = dup2(oldfd, newfd);

    if (result < 0) {
        darwin_errno = posix_errno_to_darwin(errno);
        return -1;
    }

    g_macos_stats.syscalls_translated++;
    g_macos_stats.bsd_syscalls++;

    return result;
}

int darwin_pipe(int pipefd[2]) {
    int result = pipe(pipefd);

    if (result < 0) {
        darwin_errno = posix_errno_to_darwin(errno);
        return -1;
    }

    g_macos_stats.syscalls_translated++;
    g_macos_stats.bsd_syscalls++;

    return result;
}

int darwin_mkdir(const char* path, int mode) {
    int result = mkdir(path, (mode_t)mode);

    if (result < 0) {
        darwin_errno = posix_errno_to_darwin(errno);
        return -1;
    }

    g_macos_stats.syscalls_translated++;
    g_macos_stats.bsd_syscalls++;

    return result;
}

int darwin_rmdir(const char* path) {
    int result = rmdir(path);

    if (result < 0) {
        darwin_errno = posix_errno_to_darwin(errno);
        return -1;
    }

    g_macos_stats.syscalls_translated++;
    g_macos_stats.bsd_syscalls++;

    return result;
}

int darwin_unlink(const char* path) {
    int result = unlink(path);

    if (result < 0) {
        darwin_errno = posix_errno_to_darwin(errno);
        return -1;
    }

    g_macos_stats.syscalls_translated++;
    g_macos_stats.bsd_syscalls++;

    return result;
}

int darwin_rename(const char* oldpath, const char* newpath) {
    int result = rename(oldpath, newpath);

    if (result < 0) {
        darwin_errno = posix_errno_to_darwin(errno);
        return -1;
    }

    g_macos_stats.syscalls_translated++;
    g_macos_stats.bsd_syscalls++;

    return result;
}

int darwin_chdir(const char* path) {
    int result = chdir(path);

    if (result < 0) {
        darwin_errno = posix_errno_to_darwin(errno);
        return -1;
    }

    g_macos_stats.syscalls_translated++;
    g_macos_stats.bsd_syscalls++;

    return result;
}

int darwin_chmod(const char* path, int mode) {
    int result = chmod(path, (mode_t)mode);

    if (result < 0) {
        darwin_errno = posix_errno_to_darwin(errno);
        return -1;
    }

    g_macos_stats.syscalls_translated++;
    g_macos_stats.bsd_syscalls++;

    return result;
}

int darwin_chown(const char* path, int owner, int group) {
    int result = chown(path, (uid_t)owner, (gid_t)group);

    if (result < 0) {
        darwin_errno = posix_errno_to_darwin(errno);
        return -1;
    }

    g_macos_stats.syscalls_translated++;
    g_macos_stats.bsd_syscalls++;

    return result;
}

/* Memory management */
void* darwin_mmap(void* addr, size_t length, int prot, int flags, int fd, long offset) {
    int posix_prot = translate_mmap_prot(prot);
    int posix_flags = translate_mmap_flags(flags);

    void* result = mmap(addr, length, posix_prot, posix_flags, fd, offset);

    if (result == MAP_FAILED) {
        darwin_errno = posix_errno_to_darwin(errno);
        return MAP_FAILED;
    }

    g_macos_stats.syscalls_translated++;
    g_macos_stats.bsd_syscalls++;
    g_macos_stats.mmap_calls++;

    return result;
}

int darwin_munmap(void* addr, size_t length) {
    int result = munmap(addr, length);

    if (result < 0) {
        darwin_errno = posix_errno_to_darwin(errno);
        return -1;
    }

    g_macos_stats.syscalls_translated++;
    g_macos_stats.bsd_syscalls++;

    return result;
}

int darwin_mprotect(void* addr, size_t len, int prot) {
    int posix_prot = translate_mmap_prot(prot);

    int result = mprotect(addr, len, posix_prot);

    if (result < 0) {
        darwin_errno = posix_errno_to_darwin(errno);
        return -1;
    }

    g_macos_stats.syscalls_translated++;
    g_macos_stats.bsd_syscalls++;

    return result;
}

/* Mach Traps */

mach_port_t mach_task_self(void) {
    g_macos_stats.syscalls_translated++;
    g_macos_stats.mach_traps++;
    return g_macos_ctx.task_port;
}

mach_port_t mach_thread_self(void) {
    g_macos_stats.syscalls_translated++;
    g_macos_stats.mach_traps++;
    return g_macos_ctx.thread_port;
}

mach_port_t mach_host_self(void) {
    g_macos_stats.syscalls_translated++;
    g_macos_stats.mach_traps++;
    return g_macos_ctx.host_port;
}

mach_port_t mach_reply_port(void) {
    g_macos_stats.syscalls_translated++;
    g_macos_stats.mach_traps++;
    /* Allocate new reply port */
    static mach_port_t next_port = 0x5000;
    return next_port++;
}

kern_return_t mach_msg_trap(mach_msg_header_t* msg, uint32_t option, uint32_t send_size,
                             uint32_t rcv_size, mach_port_t rcv_name, uint32_t timeout, mach_port_t notify) {
    (void)msg;
    (void)option;
    (void)send_size;
    (void)rcv_size;
    (void)rcv_name;
    (void)timeout;
    (void)notify;

    g_macos_stats.syscalls_translated++;
    g_macos_stats.mach_traps++;

    /* Stub - mach_msg requires full Mach IPC implementation */
    return KERN_FAILURE;
}

kern_return_t task_for_pid(mach_port_t target_tport, int pid, mach_port_t* t) {
    (void)target_tport;
    (void)pid;

    if (!t) {
        return KERN_FAILURE;
    }

    g_macos_stats.syscalls_translated++;
    g_macos_stats.mach_traps++;

    /* Simplified - return fake task port */
    *t = 0x6000 + pid;
    return KERN_SUCCESS;
}

kern_return_t pid_for_task(mach_port_t t, int* pid) {
    (void)t;

    if (!pid) {
        return KERN_FAILURE;
    }

    g_macos_stats.syscalls_translated++;
    g_macos_stats.mach_traps++;

    /* Simplified - extract PID from task port */
    *pid = (int)(t - 0x6000);
    return KERN_SUCCESS;
}

/* sysctl implementation (simplified) */
int darwin_sysctl(int* name, uint32_t namelen, void* oldp, size_t* oldlenp,
                  void* newp, size_t newlen) {
    (void)name;
    (void)namelen;
    (void)oldp;
    (void)oldlenp;
    (void)newp;
    (void)newlen;

    g_macos_stats.syscalls_translated++;
    g_macos_stats.bsd_syscalls++;

    /* Stub - sysctl requires full sysctl tree implementation */
    darwin_errno = DARWIN_ENOSYS;
    return -1;
}

/* General syscall dispatcher */
long darwin_syscall(long number, long arg1, long arg2, long arg3, long arg4, long arg5, long arg6) {
    (void)arg4;
    (void)arg5;
    (void)arg6;

    switch (number) {
        case DARWIN_SYS_exit:
            darwin_exit((int)arg1);
            return 0;

        case DARWIN_SYS_fork:
            return darwin_fork();

        case DARWIN_SYS_read:
            return darwin_read((int)arg1, (void*)arg2, (size_t)arg3);

        case DARWIN_SYS_write:
            return darwin_write((int)arg1, (const void*)arg2, (size_t)arg3);

        case DARWIN_SYS_open:
            return darwin_open((const char*)arg1, (int)arg2, (int)arg3);

        case DARWIN_SYS_close:
            return darwin_close((int)arg1);

        case DARWIN_SYS_wait4:
            return darwin_wait4((int)arg1, (int*)arg2, (int)arg3, (void*)arg4);

        case DARWIN_SYS_getpid:
            return darwin_getpid();

        case DARWIN_SYS_getppid:
            return darwin_getppid();

        case DARWIN_SYS_dup:
            return darwin_dup((int)arg1);

        case DARWIN_SYS_dup2:
            return darwin_dup2((int)arg1, (int)arg2);

        case DARWIN_SYS_pipe:
            return darwin_pipe((int*)arg1);

        case DARWIN_SYS_execve:
            return darwin_execve((const char*)arg1, (char* const*)arg2, (char* const*)arg3);

        case DARWIN_SYS_lseek:
            return darwin_lseek((int)arg1, (long)arg2, (int)arg3);

        case DARWIN_SYS_mkdir:
            return darwin_mkdir((const char*)arg1, (int)arg2);

        case DARWIN_SYS_rmdir:
            return darwin_rmdir((const char*)arg1);

        case DARWIN_SYS_unlink:
            return darwin_unlink((const char*)arg1);

        case DARWIN_SYS_rename:
            return darwin_rename((const char*)arg1, (const char*)arg2);

        case DARWIN_SYS_chdir:
            return darwin_chdir((const char*)arg1);

        case DARWIN_SYS_kill:
            return darwin_kill((int)arg1, (int)arg2);

        case DARWIN_SYS_mmap:
            return (long)darwin_mmap((void*)arg1, (size_t)arg2, (int)arg3, (int)arg4, (int)arg5, (long)arg6);

        case DARWIN_SYS_munmap:
            return darwin_munmap((void*)arg1, (size_t)arg2);

        case DARWIN_SYS_mprotect:
            return darwin_mprotect((void*)arg1, (size_t)arg2, (int)arg3);

        default:
            darwin_errno = DARWIN_ENOSYS;
            return -1;
    }
}

/* Statistics */
int macos_get_stats(macos_stats_t* out_stats) {
    if (!out_stats) return -1;

    out_stats->syscalls_translated = g_macos_stats.syscalls_translated;
    out_stats->bsd_syscalls = g_macos_stats.bsd_syscalls;
    out_stats->mach_traps = g_macos_stats.mach_traps;
    out_stats->files_opened = g_macos_stats.files_opened;
    out_stats->processes_created = g_macos_stats.processes_created;
    out_stats->mmap_calls = g_macos_stats.mmap_calls;

    return 0;
}
