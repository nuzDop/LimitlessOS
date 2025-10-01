# GitHub Copilot Instructions for LimitlessOS

## Project Overview

LimitlessOS is an **AI-first, universal operating system** built on a microkernel architecture. It runs on any hardware and executes Windows, Linux, and macOS applications side-by-side through a persona system.

**Current Status**: v1.0.0-alpha (28,435 LOC, 40% complete)
**Target**: v3.0.0-stable (150,000+ LOC, World-Class OS)
**Timeline**: 18-24 months

## Core Principles

When generating code for LimitlessOS, always adhere to these principles:

1. **Universal Compatibility** - Run every app from every platform
2. **Invisible Performance** - Faster than native on all platforms
3. **Bulletproof Security** - Unhackable by design
4. **Zero Configuration** - Works perfectly out of the box
5. **AI-Native** - Intelligence built into every layer
6. **Beautiful & Minimal** - Military-grade professional aesthetics
7. **Infinitely Scalable** - From IoT to supercomputers

## Architecture Guidelines

### Microkernel Design

LimitlessOS uses a **microkernel architecture** with these key components:

```
User Applications (Windows PE, Linux ELF, macOS Mach-O)
    ↓
Personas (Windows, Linux, macOS)
    ↓
User-Space Services (Drivers, VFS, Network)
    ↓
Microkernel (IPC, Scheduler, Memory Manager, Capabilities)
    ↓
Hardware Abstraction Layer (HAL)
    ↓
Hardware
```

#### Key Design Patterns

1. **Minimal Kernel Surface**: Keep kernel minimal (<10,000 LOC core)
2. **User-Space Drivers**: All drivers run in user-space for stability
3. **Capability-Based Security**: Fine-grained permissions, no root
4. **Zero-Copy IPC**: High-performance message passing
5. **Performance-First**: Lightweight at idle, scales on demand

### Code Structure

```
limitlessos/
├── kernel/              # Microkernel core
│   ├── include/        # Public kernel headers
│   └── src/            # Kernel implementation
│       ├── arch/       # Architecture-specific code (x86_64, arm64)
│       ├── ipc.c       # Inter-Process Communication
│       ├── scheduler.c # Process/thread scheduler
│       ├── pmm.c       # Physical Memory Manager
│       ├── vmm.c       # Virtual Memory Manager
│       ├── process.c   # Process management
│       └── main.c      # Kernel entry point
├── hal/                # Hardware Abstraction Layer
│   ├── include/        # HAL headers
│   └── src/            # HAL implementation
│       ├── pci.c       # PCI bus driver
│       ├── timer.c     # Timer abstraction
│       ├── cpu.c       # CPU abstraction
│       └── usb/        # USB stack (Phase 1 priority)
├── drivers/            # User-space drivers (Phase 1+)
│   ├── usb/            # USB controllers (XHCI, EHCI, UHCI)
│   ├── storage/        # Storage (AHCI, NVMe)
│   ├── network/        # Network cards
│   ├── gpu/            # Graphics (Intel, AMD, NVIDIA)
│   └── input/          # Input devices
├── userspace/          # User-space components
│   ├── ai-companion/   # AI assistant integration
│   ├── personas/       # OS compatibility layers
│   │   ├── windows/    # Windows persona (PE, Win32 API)
│   │   ├── linux/      # Linux persona (ELF, POSIX)
│   │   └── macos/      # macOS persona (Mach-O, Cocoa)
│   ├── terminal/       # Universal terminal
│   ├── compositor/     # Wayland compositor (Phase 3)
│   └── settings/       # System settings
├── docs/               # Documentation
│   ├── ARCHITECTURE.md # System architecture
│   ├── BUILD.md        # Build instructions
│   └── API/            # API documentation
└── tests/              # Test suite
```

## IPC (Inter-Process Communication)

### Implementation Pattern

When generating IPC code, follow this structure:

```c
/* IPC endpoint structure */
typedef struct ipc_endpoint_impl {
    ipc_endpoint_t id;
    pid_t owner;
    bool active;
    
    /* Message queue */
    ipc_message_t* queue;
    uint32_t queue_size;
    uint32_t queue_head;
    uint32_t queue_tail;
    
    /* Waiting threads */
    struct list_head waiting_senders;
    struct list_head waiting_receivers;
    
    /* Statistics */
    uint64_t messages_sent;
    uint64_t messages_received;
    
    uint32_t lock;
} ipc_endpoint_impl_t;

/* IPC message structure */
typedef struct ipc_message {
    uint64_t msg_id;
    pid_t sender;
    uint64_t timestamp;
    size_t payload_size;
    void* payload;
    uint32_t flags;  // ZERO_COPY, URGENT, etc.
} ipc_message_t;
```

### Key Features to Include

1. **Zero-copy transfers** for large messages
2. **Priority queues** for urgent messages
3. **Capability-based** access control
4. **Async/sync** communication modes
5. **Message queuing** with backpressure
6. **Statistics tracking** for AI optimization

### Example API

```c
status_t ipc_endpoint_create(ipc_endpoint_t* out_endpoint);
status_t ipc_endpoint_destroy(ipc_endpoint_t endpoint);
status_t ipc_send(ipc_endpoint_t endpoint, const void* data, size_t size);
status_t ipc_receive(ipc_endpoint_t endpoint, void* buffer, size_t* size);
status_t ipc_send_async(ipc_endpoint_t endpoint, const void* data, size_t size, ipc_callback_t callback);
```

## Scheduler

### Implementation Pattern

The scheduler should be **priority-based, preemptive** with AI optimization hooks:

```c
/* Scheduler configuration */
typedef struct sched_config {
    sched_policy_t policy;      // PRIORITY, ROUND_ROBIN, AI_DRIVEN
    uint64_t time_slice_ns;     // Default: 10ms
    bool preemptive;
    bool smp_enabled;           // Multi-core support
    bool ai_optimization;       // Enable AI-driven scheduling
} sched_config_t;

/* Thread structure */
typedef struct thread {
    pid_t tid;
    pid_t pid;
    proc_state_t state;         // RUNNING, READY, BLOCKED, TERMINATED
    priority_t priority;        // REALTIME, HIGH, NORMAL, LOW, IDLE
    
    /* CPU affinity */
    uint32_t cpu_affinity_mask;
    uint32_t last_cpu;
    
    /* Scheduling statistics (for AI) */
    uint64_t total_runtime_ns;
    uint64_t total_wait_time_ns;
    uint64_t context_switches;
    uint64_t cache_misses;      // For AI cache optimization
    
    /* AI prediction */
    float predicted_runtime_ms;
    float io_probability;
    
    struct list_head list_node;
} thread_t;
```

### AI Hook Points

When implementing scheduling, include these AI integration points:

1. **Telemetry Collection**:
   - CPU usage per thread
   - Context switch frequency
   - Cache miss rates
   - I/O wait times

2. **Prediction Hooks**:
   - `ai_predict_thread_behavior(thread_t* thread)`
   - `ai_optimize_core_allocation(thread_t* thread)`
   - `ai_detect_anomaly(thread_t* thread)`

3. **Adaptive Scheduling**:
   ```c
   /* AI-driven scheduler that learns from behavior */
   status_t sched_ai_optimize(void) {
       // Collect telemetry
       sched_telemetry_t telemetry = sched_collect_telemetry();
       
       // Send to AI model (user-space)
       ai_optimization_t* opt = ai_analyze_scheduling(telemetry);
       
       // Apply recommendations
       if (opt->should_adjust_priorities) {
           sched_adjust_priorities(opt->priority_adjustments);
       }
       
       if (opt->should_rebalance_cores) {
           sched_rebalance_cores(opt->core_assignments);
       }
   }
   ```

### Example API

```c
status_t sched_init(sched_config_t* config);
status_t sched_create_thread(pid_t pid, thread_entry_t entry, void* arg, thread_t** out_thread);
void sched_schedule(void);  // Called by timer interrupt
void sched_yield(void);
status_t sched_set_priority(pid_t tid, priority_t priority);
status_t sched_set_affinity(pid_t tid, uint32_t cpu_mask);
```

## Memory Manager

### Physical Memory Manager (PMM)

Use bitmap-based allocation:

```c
/* PMM implementation */
typedef struct pmm_state {
    uint8_t* bitmap;
    size_t bitmap_size;
    paddr_t base_addr;
    size_t total_pages;
    size_t free_pages;
    size_t used_pages;
    uint32_t lock;
    
    /* AI telemetry */
    uint64_t total_allocations;
    uint64_t total_frees;
    uint64_t fragmentation_index;
} pmm_state_t;

/* API */
status_t pmm_init(paddr_t start, size_t size);
paddr_t pmm_alloc_page(void);
paddr_t pmm_alloc_pages(size_t count);
void pmm_free_page(paddr_t addr);
void pmm_free_pages(paddr_t addr, size_t count);
```

### Virtual Memory Manager (VMM)

Implement page tables with ASLR:

```c
/* VMM implementation */
typedef struct vmm_space {
    uint64_t* pml4;  // Page table root (x86_64)
    pid_t owner;
    size_t total_pages;
    size_t used_pages;
    
    /* Memory regions */
    struct list_head regions;
    
    /* ASLR state */
    uint64_t aslr_base;
    uint64_t stack_base;
    uint64_t heap_base;
    
    uint32_t lock;
} vmm_space_t;

/* API */
status_t vmm_init(void);
status_t vmm_create_space(vmm_space_t** out_space);
status_t vmm_map_page(vmm_space_t* space, vaddr_t vaddr, paddr_t paddr, uint32_t flags);
status_t vmm_unmap_page(vmm_space_t* space, vaddr_t vaddr);
status_t vmm_protect(vmm_space_t* space, vaddr_t vaddr, size_t size, uint32_t flags);
```

### AI Integration

Add memory pressure detection:

```c
/* AI hook for memory optimization */
typedef struct mem_telemetry {
    size_t free_memory;
    size_t used_memory;
    float fragmentation;
    uint32_t pressure_level;  // 0-100
    uint64_t swap_activity;
} mem_telemetry_t;

/* API for AI */
mem_telemetry_t vmm_get_telemetry(void);
status_t vmm_ai_optimize(ai_mem_recommendation_t* rec);
```

## Driver Scaffolding

### User-Space Driver Pattern

All drivers run in user-space for stability:

```c
/* Generic driver structure */
typedef struct driver {
    const char* name;
    driver_type_t type;  // USB, STORAGE, NETWORK, GPU, etc.
    
    /* Operations */
    status_t (*init)(struct driver* drv);
    status_t (*probe)(struct driver* drv, device_t* dev);
    status_t (*remove)(struct driver* drv, device_t* dev);
    status_t (*suspend)(struct driver* drv);
    status_t (*resume)(struct driver* drv);
    
    /* Statistics for AI */
    uint64_t interrupts_handled;
    uint64_t errors;
    uint64_t throughput_bytes;
} driver_t;

/* Device structure */
typedef struct device {
    uint32_t id;
    device_type_t type;
    const char* name;
    
    /* Hardware info */
    uint16_t vendor_id;
    uint16_t device_id;
    uint32_t class_code;
    
    /* Driver binding */
    driver_t* driver;
    void* driver_data;
    
    /* Power management */
    power_state_t power_state;
} device_t;
```

### Phase 1 Driver Stubs

Generate scaffolding for these critical drivers:

#### 1. USB Stack (Priority: CRITICAL)

```c
/* hal/src/usb/usb_core.c - Core USB stack */
/* hal/src/usb/xhci.c - USB 3.x controller */
/* hal/src/usb/ehci.c - USB 2.0 controller */
/* hal/src/usb/uhci.c - USB 1.1 controller */
/* hal/src/usb/usb_hid.c - HID class driver */

typedef struct usb_device {
    uint8_t address;
    uint16_t vendor_id;
    uint16_t product_id;
    uint8_t class;
    uint8_t subclass;
    uint8_t protocol;
    usb_speed_t speed;  // LOW, FULL, HIGH, SUPER
} usb_device_t;

/* USB API */
status_t usb_init(void);
status_t usb_enumerate_devices(void);
status_t usb_send_control(usb_device_t* dev, usb_setup_t* setup);
status_t usb_transfer(usb_device_t* dev, usb_transfer_t* transfer);
```

#### 2. PCIe Driver

```c
/* hal/src/pci.c - Already exists, enhance as needed */

typedef struct pci_device {
    uint16_t vendor_id;
    uint16_t device_id;
    uint8_t bus;
    uint8_t device;
    uint8_t function;
    uint32_t class_code;
    uint32_t bar[6];  // Base Address Registers
} pci_device_t;

/* PCIe API */
status_t pci_enumerate_devices(void);
status_t pci_read_config(pci_device_t* dev, uint8_t offset, uint32_t* value);
status_t pci_write_config(pci_device_t* dev, uint8_t offset, uint32_t value);
status_t pci_enable_msi(pci_device_t* dev);
```

#### 3. NVMe Driver

```c
/* drivers/storage/nvme.c - NVMe driver */

typedef struct nvme_device {
    pci_device_t* pci_dev;
    void* admin_queue;
    void* io_queues[16];
    uint32_t queue_count;
    uint64_t capacity_bytes;
} nvme_device_t;

/* NVMe API */
status_t nvme_init(pci_device_t* pci_dev);
status_t nvme_read_blocks(nvme_device_t* dev, uint64_t lba, void* buffer, size_t count);
status_t nvme_write_blocks(nvme_device_t* dev, uint64_t lba, const void* buffer, size_t count);
status_t nvme_flush(nvme_device_t* dev);
```

#### 4. GPU Driver Stubs

```c
/* drivers/gpu/intel.c - Intel GPU driver */
/* drivers/gpu/amdgpu.c - AMD GPU driver */
/* drivers/gpu/nouveau.c - NVIDIA GPU driver (open-source) */

typedef struct gpu_device {
    pci_device_t* pci_dev;
    void* framebuffer;
    size_t framebuffer_size;
    uint32_t width;
    uint32_t height;
    uint32_t bpp;  // Bits per pixel
    
    /* Operations */
    status_t (*set_mode)(struct gpu_device* gpu, uint32_t width, uint32_t height);
    status_t (*blit)(struct gpu_device* gpu, rect_t* src, rect_t* dst);
} gpu_device_t;
```

#### 5. Networking Driver

```c
/* drivers/network/e1000.c - Intel E1000 driver */
/* drivers/network/rtl8139.c - Realtek driver */

typedef struct net_device {
    pci_device_t* pci_dev;
    uint8_t mac_address[6];
    bool link_up;
    uint32_t link_speed;  // Mbps
    
    /* Ring buffers */
    void* rx_ring;
    void* tx_ring;
    
    /* Statistics */
    uint64_t rx_packets;
    uint64_t tx_packets;
    uint64_t rx_errors;
    uint64_t tx_errors;
} net_device_t;

/* Network API */
status_t net_init(pci_device_t* pci_dev);
status_t net_send_packet(net_device_t* dev, const void* data, size_t size);
status_t net_receive_packet(net_device_t* dev, void* buffer, size_t* size);
```

## Persona Framework

### Windows Persona

```c
/* userspace/personas/windows/pe_loader.c */

typedef struct windows_persona {
    pid_t pid;
    
    /* PE loader state */
    void* image_base;
    size_t image_size;
    
    /* Win32 API stubs */
    void* kernel32_base;
    void* ntdll_base;
    void* user32_base;
    
    /* Registry emulation */
    void* registry;
    
    /* File system mapping */
    const char* drive_c;  // Maps to /mnt/windows or similar
} windows_persona_t;

/* API */
status_t windows_persona_create(windows_persona_t** out_persona);
status_t windows_persona_load_pe(windows_persona_t* persona, const char* path);
status_t windows_persona_execute(windows_persona_t* persona);
```

### Linux Persona

```c
/* userspace/personas/linux/elf_loader.c */

typedef struct linux_persona {
    pid_t pid;
    
    /* ELF loader state */
    void* entry_point;
    
    /* POSIX environment */
    char** environ;
    
    /* File descriptors */
    int fd_table[1024];
    
    /* Signal handlers */
    void* signal_handlers[64];
} linux_persona_t;

/* API */
status_t linux_persona_create(linux_persona_t** out_persona);
status_t linux_persona_load_elf(linux_persona_t* persona, const char* path);
status_t linux_persona_execute(linux_persona_t* persona);
```

### macOS Persona

```c
/* userspace/personas/macos/macho_loader.c */

typedef struct macos_persona {
    pid_t pid;
    
    /* Mach-O loader state */
    void* dyld_base;  // Dynamic linker
    
    /* Mach ports */
    void* port_table;
    
    /* Objective-C runtime */
    void* objc_runtime;
    
    /* Cocoa frameworks */
    void* foundation_base;
    void* appkit_base;
} macos_persona_t;

/* API */
status_t macos_persona_create(macos_persona_t** out_persona);
status_t macos_persona_load_macho(macos_persona_t* persona, const char* path);
status_t macos_persona_execute(macos_persona_t* persona);
```

### Cross-Platform Execution

```c
/* Unified persona interface */
typedef struct persona {
    persona_type_t type;  // WINDOWS, LINUX, MACOS
    pid_t pid;
    void* private_data;   // Type-specific data
    
    /* Operations */
    status_t (*load)(struct persona* p, const char* path);
    status_t (*execute)(struct persona* p);
    status_t (*syscall)(struct persona* p, syscall_t* call);
    status_t (*terminate)(struct persona* p);
} persona_t;
```

## AI Integration

### Telemetry Collection

```c
/* userspace/ai-companion/telemetry.c */

typedef struct system_telemetry {
    /* CPU metrics */
    float cpu_usage[256];  // Per-core
    uint64_t context_switches;
    uint64_t interrupts;
    
    /* Memory metrics */
    size_t free_memory;
    size_t used_memory;
    float fragmentation;
    
    /* I/O metrics */
    uint64_t disk_reads;
    uint64_t disk_writes;
    uint64_t network_rx;
    uint64_t network_tx;
    
    /* Process metrics */
    uint32_t process_count;
    uint32_t thread_count;
    
    /* Anomalies */
    bool anomaly_detected;
    anomaly_type_t anomaly_type;
} system_telemetry_t;

/* API */
status_t ai_collect_telemetry(system_telemetry_t* telemetry);
status_t ai_analyze_telemetry(system_telemetry_t* telemetry, ai_insights_t* insights);
status_t ai_apply_optimization(ai_optimization_t* opt);
```

### Anomaly Detection

```c
/* AI hook for detecting system anomalies */
typedef struct anomaly {
    anomaly_type_t type;  // CPU_SPIKE, MEM_LEAK, DISK_THRASH, etc.
    severity_t severity;  // LOW, MEDIUM, HIGH, CRITICAL
    const char* description;
    uint64_t timestamp;
    
    /* Recommended action */
    action_type_t recommended_action;  // THROTTLE, KILL, ALERT
    pid_t affected_process;
} anomaly_t;

/* API */
status_t ai_detect_anomalies(system_telemetry_t* telemetry, anomaly_t* anomalies, size_t* count);
status_t ai_handle_anomaly(anomaly_t* anomaly);
```

### Scheduling Optimization

```c
/* AI-driven process scheduling */
typedef struct ai_sched_recommendation {
    pid_t tid;
    priority_t new_priority;
    uint32_t new_cpu_affinity;
    float confidence;  // 0.0-1.0
} ai_sched_recommendation_t;

/* API */
status_t ai_optimize_scheduling(system_telemetry_t* telemetry, 
                                ai_sched_recommendation_t* recs, 
                                size_t* count);
```

## Data Structures & Algorithms

### AI-Driven Process Scheduling

```c
/* Priority queue with AI predictions */
typedef struct ai_priority_queue {
    struct list_head queues[5];  // Per priority level
    
    /* AI state */
    float predicted_runtimes[1024];  // Per thread
    float io_probabilities[1024];
    float cache_affinities[256][1024];  // [cpu][thread]
    
    /* Learning parameters */
    float learning_rate;
    uint64_t training_samples;
} ai_priority_queue_t;

/* Algorithm: AI-enhanced scheduling decision */
thread_t* ai_pick_next_thread(ai_priority_queue_t* queue, uint32_t cpu_id) {
    thread_t* best = NULL;
    float best_score = -INFINITY;
    
    // Consider all runnable threads
    for_each_runnable_thread(thread) {
        float score = 0.0f;
        
        // Factor 1: Priority
        score += thread->priority * 10.0f;
        
        // Factor 2: Predicted runtime (prefer short tasks)
        score -= queue->predicted_runtimes[thread->tid] * 0.1f;
        
        // Factor 3: Cache affinity (prefer last CPU)
        if (thread->last_cpu == cpu_id) {
            score += queue->cache_affinities[cpu_id][thread->tid] * 5.0f;
        }
        
        // Factor 4: I/O probability (prefer CPU-bound on busy cores)
        if (cpu_load[cpu_id] > 0.8f) {
            score += (1.0f - queue->io_probabilities[thread->tid]) * 3.0f;
        }
        
        if (score > best_score) {
            best_score = score;
            best = thread;
        }
    }
    
    return best;
}
```

### Secure Sandboxing

```c
/* Capability-based sandboxing */
typedef struct sandbox {
    pid_t pid;
    
    /* Capabilities */
    capability_set_t capabilities;
    
    /* Resource limits */
    size_t max_memory;
    uint64_t max_cpu_time_ns;
    uint32_t max_file_descriptors;
    uint32_t max_threads;
    
    /* File system access */
    const char* allowed_paths[64];
    size_t allowed_path_count;
    
    /* Network access */
    bool network_allowed;
    uint16_t allowed_ports[16];
    size_t allowed_port_count;
    
    /* System call filtering */
    bool allowed_syscalls[512];  // Bitmap
} sandbox_t;

/* API */
status_t sandbox_create(sandbox_t** out_sandbox);
status_t sandbox_add_capability(sandbox_t* sandbox, capability_t cap);
status_t sandbox_set_limits(sandbox_t* sandbox, resource_limits_t* limits);
status_t sandbox_allow_path(sandbox_t* sandbox, const char* path, access_mode_t mode);
status_t sandbox_execute(sandbox_t* sandbox, const char* program, char** argv);
```

### Cross-Platform Binary Translation

```c
/* JIT compiler for cross-architecture execution */
typedef struct jit_context {
    arch_t source_arch;  // x86_64, aarch64
    arch_t target_arch;
    
    /* Code cache */
    void* code_cache;
    size_t code_cache_size;
    size_t code_cache_used;
    
    /* Translation map */
    struct hash_table translation_map;  // source_pc -> target_pc
    
    /* Statistics */
    uint64_t blocks_translated;
    uint64_t cache_hits;
    uint64_t cache_misses;
} jit_context_t;

/* API */
status_t jit_create(arch_t source, arch_t target, jit_context_t** out_ctx);
status_t jit_translate_block(jit_context_t* ctx, void* source_pc, void** target_pc);
status_t jit_execute(jit_context_t* ctx, void* entry_point);
```

## Build System

### Makefile Patterns

When generating Makefiles, follow this structure:

```makefile
# Component Makefile Template

CC := clang
CFLAGS := -Wall -Wextra -Werror -std=c17 -O2 -g
LDFLAGS := -nostdlib -static

SOURCES := $(wildcard src/*.c)
OBJECTS := $(SOURCES:.c=.o)
TARGET := component.elf

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(LDFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJECTS) $(TARGET)

.PHONY: all clean
```

### Root Makefile Structure

```makefile
.PHONY: all kernel hal drivers userspace bootloader iso clean

all: kernel hal drivers userspace bootloader

kernel:
	$(MAKE) -C kernel

hal:
	$(MAKE) -C hal

drivers:
	$(MAKE) -C drivers

userspace:
	$(MAKE) -C userspace

bootloader:
	$(MAKE) -C bootloader

iso: all
	./scripts/create-iso.sh

clean:
	$(MAKE) -C kernel clean
	$(MAKE) -C hal clean
	$(MAKE) -C drivers clean
	$(MAKE) -C userspace clean
	$(MAKE) -C bootloader clean
```

## CI/CD Workflows

### GitHub Actions Template

```yaml
name: Build and Test

on:
  push:
    branches: [ main, develop ]
  pull_request:
    branches: [ main, develop ]

jobs:
  build:
    runs-on: ubuntu-latest
    
    steps:
    - uses: actions/checkout@v3
    
    - name: Install dependencies
      run: |
        sudo apt update
        sudo apt install -y clang llvm lld nasm make grub-pc-bin xorriso qemu-system-x86
    
    - name: Build kernel
      run: make kernel
    
    - name: Build HAL
      run: make hal
    
    - name: Build bootloader
      run: make bootloader
    
    - name: Build userspace
      run: make userspace
    
    - name: Create ISO
      run: make iso
    
    - name: Run tests
      run: make test
    
    - name: Upload artifacts
      uses: actions/upload-artifact@v3
      with:
        name: limitlessos-iso
        path: dist/limitlessos.iso

  qemu-test:
    runs-on: ubuntu-latest
    needs: build
    
    steps:
    - uses: actions/download-artifact@v3
      with:
        name: limitlessos-iso
    
    - name: Test in QEMU
      run: |
        timeout 60 qemu-system-x86_64 \
          -cdrom limitlessos.iso \
          -m 2G \
          -nographic \
          -serial mon:stdio || true
```

## Documentation Guidelines

### Code Documentation

Use Doxygen-style comments:

```c
/**
 * @brief Initialize the IPC subsystem
 * 
 * Sets up IPC endpoints, message queues, and initializes
 * the zero-copy transfer mechanism.
 * 
 * @return STATUS_OK on success
 * @return STATUS_EXISTS if already initialized
 * @return STATUS_NOMEM if memory allocation fails
 * 
 * @note This must be called during kernel initialization
 * @see ipc_endpoint_create
 */
status_t ipc_init(void);
```

### Architecture Diagrams

Generate ASCII diagrams for documentation:

```
IPC Message Flow
================

Sender Process              Kernel              Receiver Process
     |                         |                         |
     |---[1] ipc_send()------->|                         |
     |                         |                         |
     |                     [2] Validate                  |
     |                     [3] Queue message             |
     |                         |                         |
     |                         |<--[4] ipc_receive()-----|
     |                         |                         |
     |                     [5] Dequeue message           |
     |                     [6] Copy to receiver          |
     |                         |                         |
     |                         |---[7] Return data------>|
     |                         |                         |
```

### API Reference

Document all public APIs:

```markdown
## IPC API Reference

### `ipc_endpoint_create()`

**Prototype:**
```c
status_t ipc_endpoint_create(ipc_endpoint_t* out_endpoint);
```

**Description:**
Creates a new IPC endpoint for inter-process communication.

**Parameters:**
- `out_endpoint`: Pointer to store the created endpoint ID

**Returns:**
- `STATUS_OK`: Success
- `STATUS_INVALID`: Invalid parameter
- `STATUS_NOMEM`: Out of memory

**Example:**
```c
ipc_endpoint_t endpoint;
if (ipc_endpoint_create(&endpoint) == STATUS_OK) {
    // Use endpoint...
}
```
```

## Phase Roadmap Integration

Follow this development sequence:

### Phase 1: Critical Foundation (Weeks 1-8, ~7,500 LOC)
**Focus**: Boot on real hardware, basic functionality

1. **USB Subsystem** (1,800 LOC)
   - USB 1.1/2.0/3.x controller drivers
   - HID class driver (keyboard, mouse)
   - Mass storage class driver

2. **Filesystem Layer** (3,200 LOC)
   - FAT32 implementation
   - ext4 implementation
   - NTFS read support

3. **ACPI Subsystem** (1,200 LOC)
   - Table parsing
   - Power management
   - CPU enumeration

4. **SMP Support** (900 LOC)
   - APIC management
   - Per-CPU data structures
   - IPI handling

5. **Real NVMe Driver** (600 LOC)
   - Admin queue management
   - I/O queue management
   - Block operations

### Phase 2: Core OS Expansion (Weeks 9-16)

1. **Complete VFS** with NTFS, ext4, APFS, ZFS
2. **AI-optimized scheduler** with telemetry
3. **Persona expansion** with runtime libraries
4. **Secure sandboxing** with capabilities
5. **AI-assisted debugging** with anomaly detection

### Phase 3: Desktop & Usability (Weeks 17-24)

1. **Wayland compositor**
2. **Window manager**
3. **Desktop shell**
4. **Natural language OS control**
5. **App store layer**

### Phase 4: Security & Reliability (Weeks 25-40)

1. **Formally verified microkernel**
2. **End-to-end encryption**
3. **AI intrusion detection**
4. **Self-healing kernel**
5. **Hardware attestation**

### Phase 5: Innovation & Beyond (Weeks 41+)

1. **Built-in AI assistant**
2. **Quantum computing APIs**
3. **AR/VR integration**
4. **Distributed computing mesh**
5. **Persona Fusion Mode**

## Coding Standards

### Style Guide

1. **Naming Conventions**:
   - Functions: `subsystem_action_noun()` (e.g., `ipc_send_message()`)
   - Types: `noun_t` (e.g., `thread_t`, `ipc_message_t`)
   - Constants: `SUBSYSTEM_CONSTANT` (e.g., `IPC_MAX_ENDPOINTS`)
   - Structs: `struct noun` or `typedef struct noun noun_t`

2. **Formatting**:
   - 4 spaces for indentation (no tabs)
   - K&R style braces
   - Maximum line length: 100 characters
   - Space after keywords: `if (`, `for (`, `while (`

3. **Error Handling**:
   ```c
   status_t function(void) {
       if (error_condition) {
           return STATUS_ERROR;
       }
       
       // Success path
       return STATUS_OK;
   }
   ```

4. **Memory Management**:
   - Always check allocation results
   - Free resources in reverse order of allocation
   - Use RAII patterns where possible

### Security Guidelines

1. **Always validate inputs**:
   ```c
   status_t function(void* ptr, size_t size) {
       if (!ptr || size == 0) {
           return STATUS_INVALID;
       }
       // ...
   }
   ```

2. **Use capabilities for access control**
3. **Avoid buffer overflows** - use sized operations
4. **Zero sensitive data** after use
5. **Use constant-time comparisons** for secrets

## Testing Patterns

### Unit Tests

```c
/* tests/ipc_test.c */

void test_ipc_endpoint_create(void) {
    ipc_endpoint_t endpoint;
    status_t status = ipc_endpoint_create(&endpoint);
    
    ASSERT_EQ(status, STATUS_OK);
    ASSERT_NE(endpoint, 0);
}

void test_ipc_send_receive(void) {
    ipc_endpoint_t endpoint;
    ipc_endpoint_create(&endpoint);
    
    const char* msg = "Hello, IPC!";
    ipc_send(endpoint, msg, strlen(msg) + 1);
    
    char buffer[64];
    size_t size = sizeof(buffer);
    ipc_receive(endpoint, buffer, &size);
    
    ASSERT_STREQ(buffer, msg);
}
```

### Integration Tests

```c
/* tests/integration/scheduler_test.c */

void test_thread_scheduling(void) {
    // Create multiple threads
    thread_t* threads[10];
    for (int i = 0; i < 10; i++) {
        sched_create_thread(0, test_thread_entry, NULL, &threads[i]);
    }
    
    // Run scheduler
    for (int i = 0; i < 100; i++) {
        sched_schedule();
    }
    
    // Verify all threads got CPU time
    for (int i = 0; i < 10; i++) {
        ASSERT_GT(threads[i]->total_runtime_ns, 0);
    }
}
```

## Performance Optimization

### Guidelines

1. **Zero-copy where possible**
2. **Cache-friendly data structures**
3. **Lock-free algorithms** for hot paths
4. **SIMD** for bulk operations
5. **Lazy initialization** for large structures

### Profiling Hooks

```c
/* Always include profiling in hot paths */
#ifdef ENABLE_PROFILING
    uint64_t start = read_tsc();
    // ... operation ...
    uint64_t end = read_tsc();
    perf_record_latency("operation_name", end - start);
#endif
```

## Common Pitfalls to Avoid

1. ❌ **Don't use floating-point in kernel** (use fixed-point for AI metrics)
2. ❌ **Don't allocate on stack** in kernel (use heap or static)
3. ❌ **Don't use standard library** (use kernel implementations)
4. ❌ **Don't ignore error returns**
5. ❌ **Don't mix user and kernel pointers** without validation
6. ❌ **Don't block interrupts** for extended periods
7. ❌ **Don't use recursive locks** (design lock-free instead)

## Resources

### External Documentation

- [OSDev Wiki](https://wiki.osdev.org/) - OS development resources
- [Intel SDM](https://software.intel.com/content/www/us/en/develop/articles/intel-sdm.html) - x86_64 architecture
- [ARM Architecture Reference Manual](https://developer.arm.com/documentation/) - ARM64 architecture
- [UEFI Specification](https://uefi.org/specifications) - UEFI boot
- [ACPI Specification](https://uefi.org/specifications) - Power management

### Internal Documentation

- `docs/ARCHITECTURE.md` - System architecture overview
- `docs/BUILD.md` - Build instructions
- `ROADMAP.md` - Development roadmap
- `GAP_ANALYSIS.md` - Feature comparison

---

## Quick Reference

### Common Status Codes

```c
typedef enum status {
    STATUS_OK = 0,
    STATUS_ERROR = -1,
    STATUS_INVALID = -2,
    STATUS_NOMEM = -3,
    STATUS_NOTFOUND = -4,
    STATUS_EXISTS = -5,
    STATUS_TIMEOUT = -6,
    STATUS_BUSY = -7,
} status_t;
```

### Common Macros

```c
#define SUCCESS(s) ((s) == STATUS_OK)
#define FAILED(s) ((s) != STATUS_OK)
#define KASSERT(x) do { if (!(x)) panic("Assertion failed: " #x); } while(0)
#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))
#define ALIGN_UP(x, align) (((x) + (align) - 1) & ~((align) - 1))
#define ALIGN_DOWN(x, align) ((x) & ~((align) - 1))
```

### Memory Barriers

```c
#define barrier() __asm__ __volatile__("" ::: "memory")
#define mb() __asm__ __volatile__("mfence" ::: "memory")
#define rmb() __asm__ __volatile__("lfence" ::: "memory")
#define wmb() __asm__ __volatile__("sfence" ::: "memory")
```

---

**Remember**: LimitlessOS is about building the perfect operating system. Every line of code should reflect our core principles: universal, fast, secure, intelligent, beautiful, and scalable.
