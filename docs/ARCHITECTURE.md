# LimitlessOS Architecture

## Overview

LimitlessOS is a microkernel-based operating system designed to run on any hardware and execute applications from multiple operating systems simultaneously through a persona system.

## Core Principles

1. **Microkernel Design**: Minimal kernel with user-space drivers
2. **Hardware Abstraction**: Unified HAL for cross-platform support
3. **Capability-Based Security**: Fine-grained permissions
4. **Zero-Copy IPC**: High-performance message passing
5. **Performance-First**: Lightweight at idle, scales on demand

## System Architecture

```
┌─────────────────────────────────────────────────────────┐
│                   User Applications                      │
│  (Windows PE, Linux ELF, macOS Mach-O)                  │
├─────────────────────────────────────────────────────────┤
│                      Personas                            │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐             │
│  │ Windows  │  │  Linux   │  │  macOS   │             │
│  │ Persona  │  │ Persona  │  │ Persona  │             │
│  └──────────┘  └──────────┘  └──────────┘             │
├─────────────────────────────────────────────────────────┤
│                   User-Space Services                    │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐             │
│  │ Drivers  │  │   VFS    │  │ Network  │  ...        │
│  └──────────┘  └──────────┘  └──────────┘             │
├─────────────────────────────────────────────────────────┤
│                     Microkernel                          │
│  • IPC          • Scheduling     • Memory Management    │
│  • Capabilities • Interrupts     • Basic syscalls       │
├─────────────────────────────────────────────────────────┤
│             Hardware Abstraction Layer (HAL)             │
│  • CPU   • Storage  • Network  • Display  • Audio      │
├─────────────────────────────────────────────────────────┤
│                       Hardware                           │
│  • x86_64   • ARM64   • RISC-V  (future)               │
└─────────────────────────────────────────────────────────┘
```

## Microkernel

### Responsibilities

- **Process/Thread Management**: Creation, scheduling, termination
- **Inter-Process Communication (IPC)**: Zero-copy message passing
- **Memory Management**: Virtual memory, page allocation
- **Interrupt Handling**: IRQ routing to user-space drivers
- **Capability System**: Security and permissions

### Key Components

#### Process Management
- Process descriptor with address space
- Thread descriptor with CPU context
- Priority-based scheduler
- SMP support

#### IPC System
- Endpoint-based messaging
- 128-byte message size (optimized for cache line)
- Synchronous and asynchronous modes
- Zero-copy for large transfers

#### Memory Management
- 4KB page granularity
- User space: 0x0000000000400000 - 0x00007FFFFFFFF000
- Kernel space: 0xFFFFFF8000000000 - ...
- Physical memory manager (bitmap-based)

#### Capability System
- Per-process capability space
- Types: Memory, IPC, Thread, Process, IRQ, I/O Port, Device
- Permissions: Read, Write, Execute, Grant, Revoke
- Capability derivation for fine-grained access

### Syscall Interface

```c
// Process/Thread
proc_create(name, priority, *pid)
thread_create(pid, entry, arg, *tid)
thread_exit(exit_code)
thread_yield()

// IPC
ipc_endpoint_create(*endpoint)
ipc_send(endpoint, *msg, timeout)
ipc_receive(endpoint, *msg, timeout)
ipc_reply(endpoint, msg_id, *data, size)

// Memory
vm_map(vaddr, paddr, size, flags)
vm_unmap(vaddr, size)
vm_allocate(size, flags, *vaddr)

// Capabilities
cap_create(type, object_id, perms, *cap)
cap_grant(target_pid, cap)
cap_revoke(target_pid, cap)
```

## Hardware Abstraction Layer

### Architecture

The HAL provides unified contracts for hardware access:

```c
// CPU Management
hal_cpu_count()
hal_cpu_info(cpu_id, *info)
hal_cpu_enable_interrupts()

// Storage
hal_storage_read(device, lba, *buffer, count)
hal_storage_write(device, lba, *buffer, count)

// Network
hal_network_send(device, *packet, size)
hal_network_receive(device, *buffer, *size)

// Display
hal_display_set_mode(device, *mode)
hal_display_blit(device, *data, x, y, w, h)

// Timer
hal_timer_get_ticks()
hal_timer_oneshot(ns, callback, *context)
```

### Driver Model

All device drivers run in user space:

1. Driver registers with kernel for IRQ
2. Device communicates via memory-mapped I/O or I/O ports
3. Interrupts delivered via IPC
4. Driver handles request, responds via IPC

## Personas

### Concept

Personas allow running applications from different operating systems without modification:

- **Windows Persona**: PE loader, ntdll, Win32 API, GDI, DirectX
- **Linux Persona**: ELF loader, POSIX syscalls, glibc
- **macOS Persona**: Mach-O loader, Mach ports, Cocoa, libobjc

### Implementation

Each persona is a user-space service that:

1. Loads application binaries
2. Translates syscalls to microkernel primitives
3. Provides OS-specific APIs
4. Manages process lifecycle

### Binary Translation

For cross-architecture support (x86 ↔ ARM):
- JIT compilation of hot paths
- Caching of translated blocks
- Optimizations based on runtime profiling

## Boot Process

### UEFI Boot

1. UEFI firmware loads `BOOTX64.EFI`
2. Bootloader detects memory map
3. Loads kernel to 0x100000 (1MB)
4. Prepares boot info structure
5. Exits boot services
6. Jumps to kernel entry point

### BIOS Boot

1. BIOS loads MBR boot sector (512 bytes) to 0x7C00
2. Boot sector loads stage 2 loader
3. Stage 2 switches to protected mode
4. Loads kernel
5. Switches to long mode (64-bit)
6. Jumps to kernel entry point

### Kernel Initialization

1. Early console initialization
2. Physical memory manager setup
3. HAL initialization
4. CPU detection and setup
5. Scheduler initialization
6. IPC subsystem initialization
7. Capability system initialization
8. Start init process

## Universal Terminal

### Architecture

```
User Command (e.g., "apt install nginx")
        ↓
   Command Shim (apt)
        ↓
    Parser (translate to normalized command)
        ↓
Native Package Manager Backend
        ↓
    Execute Operation
```

### Supported Commands

- **Linux**: apt, yum, dnf, pacman, apk, zypper
- **macOS**: brew
- **Windows**: choco, winget
- **Language**: npm, pip, cargo

All commands map to LimitlessOS native package manager.

## Security Model

### Capability-Based

- No ambient authority
- Explicit capability passing
- Fine-grained permissions
- Capability revocation

### Sandboxing

- Process isolation via address spaces
- Resource limits enforced by kernel
- Device access via capabilities
- Network access controlled

### Authentication

Phase 1: Local username/password
Phase 3+: Enterprise integration (AD/LDAP/OAuth)

## Performance Targets

### Boot Time
- SSD: ≤10 seconds to desktop
- HDD: ≤20 seconds to desktop

### Memory Usage
- CLI Edition: ~200 MB idle
- Desktop Edition: 400-800 MB idle
- Pro Edition: 800-1600 MB idle

### Application Launch
- Common apps: ≤300 ms

### Techniques
- Lazy loading of modules
- Aggressive background idling
- Hardware acceleration
- Fast paths for common operations

## File System

(Not implemented in Phase 1)

### VFS Layer

Abstract interface for multiple filesystems:
- ext4 (Linux)
- NTFS (Windows)
- APFS (macOS)
- LimitlessFS (native)

### LimitlessFS

Planned features:
- Copy-on-write
- Snapshots
- Compression
- Encryption
- Checksums

## Network Stack

(Not implemented in Phase 1)

### Architecture

```
Applications
    ↓
Socket API
    ↓
TCP/UDP/IP Stack (user-space)
    ↓
Network Drivers (user-space)
    ↓
HAL Network Interface
    ↓
Hardware
```

## Future Enhancements

### Phase 2-5

See `Roadmap.md` for detailed roadmap.

### Long-term Goals

- Hardware-accelerated virtualization
- GPU compute support
- Real-time guarantees
- Formal verification of critical paths
- OEM partnerships for pre-installs

## Development

### Adding Features

1. Design API in relevant header
2. Implement in kernel or user-space service
3. Add tests
4. Update documentation
5. Submit for review

### Coding Standards

- C17 for kernel
- C++20 for complex user-space services
- Doxygen comments for public APIs
- Consistent naming: `subsystem_function_name`

## References

- Microkernel design: L4, seL4
- Capability systems: KeyKOS, EROS
- IPC optimization: Fast path techniques
- HAL design: NT HAL, Darwin I/O Kit
