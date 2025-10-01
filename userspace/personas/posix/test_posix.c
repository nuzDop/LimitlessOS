/*
 * POSIX Persona Test Program
 */

#include <stdio.h>
#include <string.h>
#include "posix.h"

void test_basic_syscalls(void) {
    printf("\n=== Testing Basic Syscalls ===\n");

    posix_context_t* ctx = NULL;
    status_t status = posix_create_context(&ctx);

    if (FAILED(status) || !ctx) {
        printf("ERROR: Failed to create context\n");
        return;
    }

    /* Test getpid */
    int64_t pid = posix_syscall(ctx, SYS_getpid, 0, 0, 0, 0, 0, 0);
    printf("getpid() = %lld\n", pid);

    /* Test getuid */
    int64_t uid = posix_syscall(ctx, SYS_getuid, 0, 0, 0, 0, 0, 0);
    printf("getuid() = %lld\n", uid);

    /* Test getgid */
    int64_t gid = posix_syscall(ctx, SYS_getgid, 0, 0, 0, 0, 0, 0);
    printf("getgid() = %lld\n", gid);

    /* Test brk */
    int64_t brk = posix_syscall(ctx, SYS_brk, 0, 0, 0, 0, 0, 0);
    printf("brk(0) = 0x%llx\n", brk);

    posix_destroy_context(ctx);
}

void test_file_io(void) {
    printf("\n=== Testing File I/O ===\n");

    posix_context_t* ctx = NULL;
    posix_create_context(&ctx);

    /* Test write to stdout */
    const char* msg = "Hello from POSIX persona!\n";
    int64_t written = posix_syscall(ctx, SYS_write, STDOUT_FILENO, (uint64_t)msg, strlen(msg), 0, 0, 0);
    printf("write() returned %lld\n", written);

    /* Test open */
    int64_t fd = posix_syscall(ctx, SYS_open, (uint64_t)"/tmp/test.txt", 0x42, 0644, 0, 0, 0);  // O_RDWR | O_CREAT
    printf("open() returned fd %lld\n", fd);

    if (fd >= 0) {
        /* Test close */
        int64_t result = posix_syscall(ctx, SYS_close, fd, 0, 0, 0, 0, 0);
        printf("close() returned %lld\n", result);
    }

    posix_destroy_context(ctx);
}

void test_unsupported_syscalls(void) {
    printf("\n=== Testing Unsupported Syscalls ===\n");

    posix_context_t* ctx = NULL;
    posix_create_context(&ctx);

    /* Test fork */
    int64_t result = posix_syscall(ctx, SYS_fork, 0, 0, 0, 0, 0, 0);
    printf("fork() returned %lld (expected -ENOSYS = -38)\n", result);

    /* Test execve */
    result = posix_syscall(ctx, SYS_execve, (uint64_t)"/bin/sh", 0, 0, 0, 0, 0);
    printf("execve() returned %lld (expected -ENOSYS = -38)\n", result);

    posix_destroy_context(ctx);
}

int main(int argc, char** argv) {
    printf("\n");
    printf("========================================\n");
    printf("POSIX Persona Test Suite\n");
    printf("========================================\n");

    /* Initialize persona */
    status_t status = posix_init();
    if (FAILED(status)) {
        printf("ERROR: Failed to initialize POSIX persona\n");
        return 1;
    }

    /* Run tests */
    test_basic_syscalls();
    test_file_io();
    test_unsupported_syscalls();

    printf("\n========================================\n");
    printf("Tests Complete\n");
    printf("========================================\n\n");

    return 0;
}
