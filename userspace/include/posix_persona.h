#ifndef LIMITLESS_POSIX_PERSONA_H
#define LIMITLESS_POSIX_PERSONA_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* POSIX persona common definitions */

/* File modes */
#define O_RDONLY  0x0000
#define O_WRONLY  0x0001
#define O_RDWR    0x0002
#define O_CREAT   0x0040
#define O_TRUNC   0x0200
#define O_APPEND  0x0400

/* Seek whence */
#define SEEK_SET  0
#define SEEK_CUR  1
#define SEEK_END  2

/* Process IDs */
typedef int pid_t;
typedef unsigned int uid_t;
typedef unsigned int gid_t;
typedef long off_t;
typedef long ssize_t;
typedef unsigned int mode_t;

/* File descriptor */
typedef int fd_t;

/* POSIX syscall wrappers */
int posix_open(const char *path, int flags, mode_t mode);
ssize_t posix_read(int fd, void *buf, size_t count);
ssize_t posix_write(int fd, const void *buf, size_t count);
int posix_close(int fd);
off_t posix_lseek(int fd, off_t offset, int whence);

pid_t posix_fork(void);
int posix_execve(const char *path, char *const argv[], char *const envp[]);
pid_t posix_waitpid(pid_t pid, int *status, int options);
void posix_exit(int status);

int posix_mkdir(const char *path, mode_t mode);
int posix_rmdir(const char *path);
int posix_unlink(const char *path);
int posix_rename(const char *old, const char *new);

/* Signal handling */
typedef void (*sighandler_t)(int);
sighandler_t posix_signal(int signum, sighandler_t handler);
int posix_kill(pid_t pid, int sig);

/* Memory management */
void *posix_mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset);
int posix_munmap(void *addr, size_t length);

/* Process info */
pid_t posix_getpid(void);
pid_t posix_getppid(void);
uid_t posix_getuid(void);
gid_t posix_getgid(void);

#endif /* LIMITLESS_POSIX_PERSONA_H */
