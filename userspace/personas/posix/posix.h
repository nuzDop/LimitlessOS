#ifndef LIMITLESS_POSIX_PERSONA_H
#define LIMITLESS_POSIX_PERSONA_H

/*
 * POSIX Persona - Linux Application Compatibility Layer
 * Provides POSIX syscall translation to LimitlessOS microkernel
 */

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* POSIX syscall numbers (x86_64 Linux ABI) */
#define SYS_read           0
#define SYS_write          1
#define SYS_open           2
#define SYS_close          3
#define SYS_stat           4
#define SYS_fstat          5
#define SYS_lstat          6
#define SYS_poll           7
#define SYS_lseek          8
#define SYS_mmap           9
#define SYS_mprotect      10
#define SYS_munmap        11
#define SYS_brk           12
#define SYS_rt_sigaction  13
#define SYS_rt_sigprocmask 14
#define SYS_ioctl         16
#define SYS_access        21
#define SYS_pipe          22
#define SYS_select        23
#define SYS_sched_yield   24
#define SYS_dup           32
#define SYS_dup2          33
#define SYS_getpid        39
#define SYS_fork          57
#define SYS_execve        59
#define SYS_exit          60
#define SYS_wait4         61
#define SYS_kill          62
#define SYS_getuid        102
#define SYS_getgid        104
#define SYS_setuid        105
#define SYS_setgid        106
#define SYS_geteuid       107
#define SYS_getegid       108
#define SYS_socket        41
#define SYS_connect       42
#define SYS_accept        43
#define SYS_sendto        44
#define SYS_recvfrom      45
#define SYS_bind          49
#define SYS_listen        50
#define SYS_clone         56
#define SYS_getdents      78
#define SYS_getcwd        79
#define SYS_chdir         80
#define SYS_mkdir         83
#define SYS_rmdir         84
#define SYS_unlink        87

/* File descriptor table */
#define MAX_FDS 1024

typedef struct fd_entry {
    int fd;
    bool active;
    uint32_t flags;
    void* private_data;
} fd_entry_t;

/* Process context for POSIX persona */
typedef struct posix_context {
    uint64_t pid;
    uint64_t ppid;
    uint64_t uid;
    uint64_t gid;
    uint64_t euid;
    uint64_t egid;

    /* File descriptors */
    fd_entry_t fds[MAX_FDS];
    uint32_t next_fd;

    /* Working directory */
    char cwd[4096];

    /* Environment */
    char** environ;
    int environ_count;

    /* Signals */
    void* signal_handlers[64];

    /* Memory */
    uint64_t brk_start;
    uint64_t brk_current;
} posix_context_t;

/* ELF loader */
typedef struct elf_info {
    uint64_t entry_point;
    uint64_t base_address;
    uint64_t load_size;
    bool is_dynamic;
    char interpreter[256];
} elf_info_t;

/* POSIX Persona API */
status_t posix_init(void);
status_t posix_create_context(posix_context_t** out_ctx);
status_t posix_destroy_context(posix_context_t* ctx);
status_t posix_load_elf(const char* path, elf_info_t* info);
status_t posix_exec(posix_context_t* ctx, const char* path, char* const argv[], char* const envp[]);

/* Syscall dispatcher */
int64_t posix_syscall(posix_context_t* ctx, uint64_t syscall_num,
                      uint64_t arg1, uint64_t arg2, uint64_t arg3,
                      uint64_t arg4, uint64_t arg5, uint64_t arg6);

/* Syscall implementations */
int64_t posix_sys_read(posix_context_t* ctx, int fd, void* buf, size_t count);
int64_t posix_sys_write(posix_context_t* ctx, int fd, const void* buf, size_t count);
int64_t posix_sys_open(posix_context_t* ctx, const char* path, int flags, int mode);
int64_t posix_sys_close(posix_context_t* ctx, int fd);
int64_t posix_sys_fork(posix_context_t* ctx);
int64_t posix_sys_execve(posix_context_t* ctx, const char* path, char* const argv[], char* const envp[]);
int64_t posix_sys_exit(posix_context_t* ctx, int status);
int64_t posix_sys_getpid(posix_context_t* ctx);
int64_t posix_sys_getuid(posix_context_t* ctx);
int64_t posix_sys_getgid(posix_context_t* ctx);
int64_t posix_sys_brk(posix_context_t* ctx, uint64_t addr);
int64_t posix_sys_mmap(posix_context_t* ctx, void* addr, size_t length, int prot, int flags, int fd, off_t offset);
int64_t posix_sys_munmap(posix_context_t* ctx, void* addr, size_t length);

/* File descriptor management */
int posix_alloc_fd(posix_context_t* ctx);
void posix_free_fd(posix_context_t* ctx, int fd);
fd_entry_t* posix_get_fd(posix_context_t* ctx, int fd);

/* Standard file descriptors */
#define STDIN_FILENO  0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2

/* Error codes (POSIX errno) */
#define EPERM   1
#define ENOENT  2
#define ESRCH   3
#define EINTR   4
#define EIO     5
#define ENXIO   6
#define E2BIG   7
#define ENOEXEC 8
#define EBADF   9
#define ECHILD  10
#define EAGAIN  11
#define ENOMEM  12
#define EACCES  13
#define EFAULT  14
#define EBUSY   16
#define EEXIST  17
#define ENODEV  19
#define ENOTDIR 20
#define EISDIR  21
#define EINVAL  22
#define ENFILE  23
#define EMFILE  24
#define ENOSYS  38

#endif /* LIMITLESS_POSIX_PERSONA_H */
