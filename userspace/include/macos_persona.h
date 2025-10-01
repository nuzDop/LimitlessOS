#ifndef LIMITLESS_MACOS_PERSONA_H
#define LIMITLESS_MACOS_PERSONA_H

/*
 * macOS Persona - BSD/Darwin Syscall Translation Layer
 * Translates macOS/Darwin syscalls to LimitlessOS syscalls
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* macOS/Darwin syscall numbers (subset) */
#define DARWIN_SYS_exit         1
#define DARWIN_SYS_fork         2
#define DARWIN_SYS_read         3
#define DARWIN_SYS_write        4
#define DARWIN_SYS_open         5
#define DARWIN_SYS_close        6
#define DARWIN_SYS_wait4        7
#define DARWIN_SYS_link         9
#define DARWIN_SYS_unlink       10
#define DARWIN_SYS_chdir        12
#define DARWIN_SYS_fchdir       13
#define DARWIN_SYS_mknod        14
#define DARWIN_SYS_chmod        15
#define DARWIN_SYS_chown        16
#define DARWIN_SYS_getpid       20
#define DARWIN_SYS_kill         37
#define DARWIN_SYS_getppid      39
#define DARWIN_SYS_dup          41
#define DARWIN_SYS_pipe         42
#define DARWIN_SYS_getegid      43
#define DARWIN_SYS_sigaction    46
#define DARWIN_SYS_getgid       47
#define DARWIN_SYS_sigprocmask  48
#define DARWIN_SYS_getlogin     49
#define DARWIN_SYS_setlogin     50
#define DARWIN_SYS_acct         51
#define DARWIN_SYS_ioctl        54
#define DARWIN_SYS_symlink      57
#define DARWIN_SYS_readlink     58
#define DARWIN_SYS_execve       59
#define DARWIN_SYS_umask        60
#define DARWIN_SYS_msync        65
#define DARWIN_SYS_vfork        66
#define DARWIN_SYS_munmap       73
#define DARWIN_SYS_mprotect     74
#define DARWIN_SYS_madvise      75
#define DARWIN_SYS_getpgrp      81
#define DARWIN_SYS_setpgid      82
#define DARWIN_SYS_dup2         90
#define DARWIN_SYS_fcntl        92
#define DARWIN_SYS_select       93
#define DARWIN_SYS_fsync        95
#define DARWIN_SYS_socket       97
#define DARWIN_SYS_connect      98
#define DARWIN_SYS_getpriority  100
#define DARWIN_SYS_bind         104
#define DARWIN_SYS_setsockopt   105
#define DARWIN_SYS_listen       106
#define DARWIN_SYS_gettimeofday 116
#define DARWIN_SYS_readv        120
#define DARWIN_SYS_writev       121
#define DARWIN_SYS_fchown       123
#define DARWIN_SYS_fchmod       124
#define DARWIN_SYS_rename       128
#define DARWIN_SYS_mkdir        136
#define DARWIN_SYS_rmdir        137
#define DARWIN_SYS_getrlimit    194
#define DARWIN_SYS_setrlimit    195
#define DARWIN_SYS_lseek        199
#define DARWIN_SYS_truncate     200
#define DARWIN_SYS_ftruncate    201
#define DARWIN_SYS_mmap         197
#define DARWIN_SYS___sysctl     202
#define DARWIN_SYS_getdtablesize 89

/* Mach syscalls (Mach traps) */
#define MACH_TRAP_mach_reply_port           -26
#define MACH_TRAP_mach_thread_self          -27
#define MACH_TRAP_mach_task_self            -28
#define MACH_TRAP_mach_host_self            -29
#define MACH_TRAP_mach_msg_trap             -31
#define MACH_TRAP_semaphore_signal_trap     -33
#define MACH_TRAP_semaphore_wait_trap       -36
#define MACH_TRAP_task_for_pid              -45
#define MACH_TRAP_pid_for_task              -46

/* Error codes (Darwin errno) */
#define DARWIN_EPERM            1
#define DARWIN_ENOENT           2
#define DARWIN_ESRCH            3
#define DARWIN_EINTR            4
#define DARWIN_EIO              5
#define DARWIN_ENXIO            6
#define DARWIN_E2BIG            7
#define DARWIN_ENOEXEC          8
#define DARWIN_EBADF            9
#define DARWIN_ECHILD           10
#define DARWIN_EAGAIN           11
#define DARWIN_ENOMEM           12
#define DARWIN_EACCES           13
#define DARWIN_EFAULT           14
#define DARWIN_EINVAL           22
#define DARWIN_EMFILE           24
#define DARWIN_ENOSYS           78

/* File flags (fcntl/open) */
#define DARWIN_O_RDONLY         0x0000
#define DARWIN_O_WRONLY         0x0001
#define DARWIN_O_RDWR           0x0002
#define DARWIN_O_NONBLOCK       0x0004
#define DARWIN_O_APPEND         0x0008
#define DARWIN_O_CREAT          0x0200
#define DARWIN_O_TRUNC          0x0400
#define DARWIN_O_EXCL           0x0800
#define DARWIN_O_CLOEXEC        0x1000000

/* mmap flags */
#define DARWIN_MAP_SHARED       0x0001
#define DARWIN_MAP_PRIVATE      0x0002
#define DARWIN_MAP_FIXED        0x0010
#define DARWIN_MAP_ANON         0x1000

/* mmap protection */
#define DARWIN_PROT_NONE        0x00
#define DARWIN_PROT_READ        0x01
#define DARWIN_PROT_WRITE       0x02
#define DARWIN_PROT_EXEC        0x04

/* Signal numbers */
#define DARWIN_SIGHUP           1
#define DARWIN_SIGINT           2
#define DARWIN_SIGQUIT          3
#define DARWIN_SIGILL           4
#define DARWIN_SIGTRAP          5
#define DARWIN_SIGABRT          6
#define DARWIN_SIGEMT           7
#define DARWIN_SIGFPE           8
#define DARWIN_SIGKILL          9
#define DARWIN_SIGBUS           10
#define DARWIN_SIGSEGV          11
#define DARWIN_SIGSYS           12
#define DARWIN_SIGPIPE          13
#define DARWIN_SIGALRM          14
#define DARWIN_SIGTERM          15

/* Mach port types */
typedef uint32_t mach_port_t;
typedef uint32_t mach_port_name_t;
typedef int kern_return_t;

#define MACH_PORT_NULL          0
#define KERN_SUCCESS            0
#define KERN_FAILURE            5

/* Mach message header */
typedef struct mach_msg_header {
    uint32_t msgh_bits;
    uint32_t msgh_size;
    mach_port_t msgh_remote_port;
    mach_port_t msgh_local_port;
    uint32_t msgh_reserved;
    uint32_t msgh_id;
} mach_msg_header_t;

/* Mach task/thread info */
typedef struct {
    uint32_t suspend_count;
    uint32_t thread_count;
    uint32_t resident_size;
    uint32_t virtual_size;
} task_basic_info_t;

/* macOS Persona Context */
typedef struct macos_context {
    /* Process information */
    int32_t pid;
    int32_t ppid;
    int32_t pgid;

    /* Mach ports */
    mach_port_t task_port;
    mach_port_t thread_port;
    mach_port_t host_port;
    mach_port_t bootstrap_port;

    /* Error state */
    int darwin_errno;

    /* File descriptors */
    int max_fds;

    /* Signal handling */
    void* signal_handlers[32];

    /* Thread safety */
    uint32_t lock;
} macos_context_t;

/* macOS Persona API */
int macos_persona_init(void);
int macos_persona_shutdown(void);
macos_context_t* macos_get_context(void);

/* BSD syscall handlers */
long darwin_syscall(long number, long arg1, long arg2, long arg3, long arg4, long arg5, long arg6);

/* Common BSD syscalls */
int darwin_open(const char* path, int flags, int mode);
int darwin_close(int fd);
long darwin_read(int fd, void* buf, size_t count);
long darwin_write(int fd, const void* buf, size_t count);
long darwin_lseek(int fd, long offset, int whence);
int darwin_fork(void);
int darwin_execve(const char* path, char* const argv[], char* const envp[]);
void darwin_exit(int status);
int darwin_wait4(int pid, int* status, int options, void* rusage);
int darwin_getpid(void);
int darwin_getppid(void);
int darwin_kill(int pid, int sig);
int darwin_dup(int fd);
int darwin_dup2(int oldfd, int newfd);
int darwin_pipe(int pipefd[2]);
int darwin_mkdir(const char* path, int mode);
int darwin_rmdir(const char* path);
int darwin_unlink(const char* path);
int darwin_rename(const char* oldpath, const char* newpath);
int darwin_chdir(const char* path);
int darwin_chmod(const char* path, int mode);
int darwin_chown(const char* path, int owner, int group);

/* Memory management */
void* darwin_mmap(void* addr, size_t length, int prot, int flags, int fd, long offset);
int darwin_munmap(void* addr, size_t length);
int darwin_mprotect(void* addr, size_t len, int prot);

/* Mach traps */
mach_port_t mach_task_self(void);
mach_port_t mach_thread_self(void);
mach_port_t mach_host_self(void);
mach_port_t mach_reply_port(void);
kern_return_t mach_msg_trap(mach_msg_header_t* msg, uint32_t option, uint32_t send_size,
                             uint32_t rcv_size, mach_port_t rcv_name, uint32_t timeout, mach_port_t notify);
kern_return_t task_for_pid(mach_port_t target_tport, int pid, mach_port_t* t);
kern_return_t pid_for_task(mach_port_t t, int* pid);

/* sysctl */
int darwin_sysctl(int* name, uint32_t namelen, void* oldp, size_t* oldlenp,
                  void* newp, size_t newlen);

/* Error handling */
int* __darwin_errno(void);
#define darwin_errno (*__darwin_errno())

/* Helper functions */
int darwin_errno_to_posix(int darwin_err);
int posix_errno_to_darwin(int posix_err);

/* Statistics */
typedef struct macos_stats {
    uint64_t syscalls_translated;
    uint64_t bsd_syscalls;
    uint64_t mach_traps;
    uint64_t files_opened;
    uint64_t processes_created;
    uint64_t mmap_calls;
} macos_stats_t;

int macos_get_stats(macos_stats_t* out_stats);

#endif /* LIMITLESS_MACOS_PERSONA_H */
