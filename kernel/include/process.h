#ifndef LIMITLESS_PROCESS_H
#define LIMITLESS_PROCESS_H

/*
 * Process Manager
 * Process creation, execution, and lifecycle management
 */

#include "kernel.h"
#include "vmm.h"
#include "elf.h"
#include "vfs.h"

/* Process states */
typedef enum {
    PROCESS_STATE_EMBRYO = 0,  // Being created
    PROCESS_STATE_READY,        // Ready to run
    PROCESS_STATE_RUNNING,      // Currently executing
    PROCESS_STATE_BLOCKED,      // Waiting for event
    PROCESS_STATE_ZOMBIE,       // Terminated, waiting for parent
    PROCESS_STATE_DEAD,         // Fully terminated
} process_state_t;

/* Process flags */
#define PROCESS_FLAG_KERNEL     0x0001  // Kernel process
#define PROCESS_FLAG_USER       0x0002  // User process
#define PROCESS_FLAG_TRACED     0x0004  // Being traced
#define PROCESS_FLAG_STOPPED    0x0008  // Stopped by signal

/* Maximum processes */
#define PROCESS_MAX_COUNT 1024
#define PROCESS_MAX_THREADS 64
#define PROCESS_MAX_CHILDREN 256
#define PROCESS_MAX_FDS 1024

/* File descriptor */
typedef struct file_descriptor {
    void* file;                // VFS file pointer
    uint32_t flags;            // Open flags
    uint32_t ref_count;        // Reference count
    bool in_use;
} file_descriptor_t;

/* Forward declaration for CPU context */
struct cpu_context;

/* Thread structure */
typedef struct thread {
    tid_t tid;                 // Thread ID
    uint64_t kernel_stack;     // Kernel stack pointer
    uint64_t user_stack;       // User stack pointer
    uint64_t entry_point;      // Entry point address
    uint32_t state;            // Thread state
    uint32_t priority;         // Scheduling priority
    uint64_t cpu_time;         // CPU time used (nanoseconds)
    struct process* process;   // Parent process
    struct list_head list_node; // Scheduler list linkage
    struct cpu_context* context; // CPU execution context
    bool in_use;
} thread_t;

/* Process structure */
typedef struct process {
    pid_t pid;                 // Process ID
    pid_t parent_pid;          // Parent process ID
    pid_t pgid;                // Process group ID
    pid_t sid;                 // Session ID

    /* State */
    process_state_t state;
    int exit_code;
    uint32_t flags;

    /* Memory management */
    address_space_t* aspace;   // Virtual address space
    vaddr_t heap_start;        // Heap start address
    vaddr_t heap_end;          // Heap end address
    vaddr_t brk;               // Current break pointer

    /* ELF context */
    elf_context_t* elf_ctx;

    /* Threads */
    thread_t threads[PROCESS_MAX_THREADS];
    uint32_t thread_count;
    thread_t* main_thread;

    /* File descriptors */
    file_descriptor_t fds[PROCESS_MAX_FDS];
    uint32_t fd_count;

    /* Parent-child relationships */
    pid_t children[PROCESS_MAX_CHILDREN];
    uint32_t child_count;

    /* Working directory */
    char cwd[VFS_MAX_PATH];

    /* User credentials */
    uint32_t uid;              // User ID
    uint32_t gid;              // Group ID
    uint32_t euid;             // Effective user ID
    uint32_t egid;             // Effective group ID

    /* Timing */
    uint64_t start_time;       // Process start time
    uint64_t cpu_time;         // Total CPU time

    /* Synchronization */
    uint32_t lock;

    /* List node */
    struct list_head list_node;

    bool in_use;
} process_t;

/* Process manager state */
typedef struct process_manager {
    process_t processes[PROCESS_MAX_COUNT];
    uint32_t process_count;
    pid_t next_pid;
    pid_t next_tid;
    process_t* current_process;
    thread_t* current_thread;
    struct list_head process_list;
    uint32_t lock;
    bool initialized;
} process_manager_t;

/* Process manager API */
status_t process_init(void);
void process_shutdown(void);

/* Process lifecycle */
status_t process_create(process_t** out_process);
status_t process_fork(process_t* parent, process_t** out_child);
status_t process_exec(process_t* process, const char* path, char* const argv[], char* const envp[]);
status_t process_exit(process_t* process, int exit_code);
status_t process_wait(process_t* parent, pid_t child_pid, int* status, uint32_t options);
status_t process_kill(pid_t pid, int signal);

/* Thread management */
status_t thread_create(process_t* process, vaddr_t entry_point, vaddr_t stack_ptr, thread_t** out_thread);
status_t thread_exit(thread_t* thread, int exit_code);

/* Process lookup */
process_t* process_find_by_pid(pid_t pid);
process_t* process_get_current(void);
thread_t* thread_get_current(void);

/* File descriptor management */
int process_fd_alloc(process_t* process, void* file, uint32_t flags);
status_t process_fd_free(process_t* process, int fd);
file_descriptor_t* process_fd_get(process_t* process, int fd);
status_t process_fd_dup(process_t* process, int oldfd, int newfd);
status_t process_fd_clone_all(process_t* parent, process_t* child);

/* Memory management */
status_t process_brk(process_t* process, vaddr_t new_brk, vaddr_t* out_brk);
status_t process_mmap(process_t* process, vaddr_t addr, size_t length, uint32_t prot, uint32_t flags, int fd, uint64_t offset, vaddr_t* out_addr);
status_t process_munmap(process_t* process, vaddr_t addr, size_t length);

/* Address space management */
status_t process_copy_address_space(address_space_t* src, address_space_t** out_dest);
status_t process_setup_user_stack(process_t* process, vaddr_t* out_stack_ptr);

/* Utility functions */
status_t process_add_child(process_t* parent, pid_t child_pid);
status_t process_remove_child(process_t* parent, pid_t child_pid);
void process_cleanup_zombies(void);

/* Statistics */
typedef struct process_stats {
    uint32_t total_processes;
    uint32_t active_processes;
    uint32_t zombie_processes;
    uint32_t total_threads;
    uint64_t total_cpu_time;
} process_stats_t;

status_t process_get_stats(process_stats_t* stats);

#endif /* LIMITLESS_PROCESS_H */
