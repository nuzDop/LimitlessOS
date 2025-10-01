#ifndef LIMITLESS_KERNEL_H
#define LIMITLESS_KERNEL_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* Kernel version */
#define LIMITLESS_VERSION_MAJOR 0
#define LIMITLESS_VERSION_MINOR 1
#define LIMITLESS_VERSION_PATCH 0
#define LIMITLESS_VERSION_STRING "0.1.0-phase1"

/* Architecture detection */
#if defined(__x86_64__)
    #define ARCH_X86_64 1
    #define ARCH_NAME "x86_64"
#elif defined(__aarch64__)
    #define ARCH_ARM64 1
    #define ARCH_NAME "arm64"
#else
    #error "Unsupported architecture"
#endif

/* Kernel panic and assertions */
void kernel_panic(const char* message) __attribute__((noreturn));
void kernel_assert(bool condition, const char* message);

#define PANIC(msg) kernel_panic(msg)
#define ASSERT(cond, msg) kernel_assert(cond, msg)
#define KASSERT(cond) kernel_assert(cond, #cond)

/* Kernel entry point */
void kernel_main(void* boot_info);

/* Memory sizes */
#define KB(x) ((x) * 1024UL)
#define MB(x) ((x) * 1024UL * 1024UL)
#define GB(x) ((x) * 1024UL * 1024UL * 1024UL)

/* Page sizes */
#define PAGE_SIZE 4096UL
#define PAGE_MASK (~(PAGE_SIZE - 1))
#define PAGE_ALIGN_UP(addr) (((addr) + PAGE_SIZE - 1) & PAGE_MASK)
#define PAGE_ALIGN_DOWN(addr) ((addr) & PAGE_MASK)

/* Compiler attributes */
#define PACKED __attribute__((packed))
#define ALIGNED(x) __attribute__((aligned(x)))
#define NORETURN __attribute__((noreturn))
#define UNUSED __attribute__((unused))
#define PURE __attribute__((pure))
#define CONST __attribute__((const))
#define ALWAYS_INLINE __attribute__((always_inline)) inline
#define SECTION(name) __attribute__((section(name)))

/* Standard types */
typedef uint64_t paddr_t;  // Physical address
typedef uint64_t vaddr_t;  // Virtual address
typedef uint64_t size_t;
typedef int64_t  ssize_t;

/* Status codes */
typedef enum {
    STATUS_OK = 0,
    STATUS_ERROR = -1,
    STATUS_NOMEM = -2,
    STATUS_INVALID = -3,
    STATUS_NOTFOUND = -4,
    STATUS_TIMEOUT = -5,
    STATUS_BUSY = -6,
    STATUS_DENIED = -7,
    STATUS_EXISTS = -8,
    STATUS_NOSUPPORT = -9,
} status_t;

#define SUCCESS(s) ((s) == STATUS_OK)
#define FAILED(s) ((s) != STATUS_OK)

/* Null pointer */
#ifndef NULL
#define NULL ((void*)0)
#endif

/* Boolean */
#ifndef bool
typedef _Bool bool;
#define true 1
#define false 0
#endif

/* Min/Max */
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

/* Array size */
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

/* Bit operations */
#define BIT(n) (1UL << (n))
#define BIT_SET(x, bit) ((x) |= BIT(bit))
#define BIT_CLEAR(x, bit) ((x) &= ~BIT(bit))
#define BIT_TEST(x, bit) (((x) & BIT(bit)) != 0)

/* Offset of */
#ifndef offsetof
#define offsetof(type, member) __builtin_offsetof(type, member)
#endif

/* Container of */
#define container_of(ptr, type, member) \
    ((type*)((char*)(ptr) - offsetof(type, member)))

/* Logging levels */
typedef enum {
    LOG_DEBUG = 0,
    LOG_INFO = 1,
    LOG_WARN = 2,
    LOG_ERROR = 3,
    LOG_FATAL = 4,
} log_level_t;

/* Kernel logging */
void kernel_log(log_level_t level, const char* subsystem, const char* format, ...);

#define KLOG_DEBUG(subsys, ...) kernel_log(LOG_DEBUG, subsys, __VA_ARGS__)
#define KLOG_INFO(subsys, ...)  kernel_log(LOG_INFO, subsys, __VA_ARGS__)
#define KLOG_WARN(subsys, ...)  kernel_log(LOG_WARN, subsys, __VA_ARGS__)
#define KLOG_ERROR(subsys, ...) kernel_log(LOG_ERROR, subsys, __VA_ARGS__)
#define KLOG_FATAL(subsys, ...) kernel_log(LOG_FATAL, subsys, __VA_ARGS__)

/* Kernel string functions (freestanding) */
void* memset(void* dest, int c, size_t n);
void* memcpy(void* dest, const void* src, size_t n);
void* memmove(void* dest, const void* src, size_t n);
int memcmp(const void* s1, const void* s2, size_t n);
size_t strlen(const char* str);
char* strcpy(char* dest, const char* src);
char* strncpy(char* dest, const char* src, size_t n);
int strcmp(const char* s1, const char* s2);
int strncmp(const char* s1, const char* s2, size_t n);

#endif /* LIMITLESS_KERNEL_H */
