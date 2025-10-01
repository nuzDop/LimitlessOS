# Building LimitlessOS

This document describes how to build LimitlessOS Phase 1 from source.

## Prerequisites

### Required Tools

- **Clang/LLVM** (version 14+)
- **NASM** (for assembly)
- **GNU Make**
- **ld.lld** (LLVM linker)
- **GRUB** (for ISO creation)

### Ubuntu/Debian

```bash
sudo apt update
sudo apt install clang llvm lld nasm make grub-pc-bin xorriso
```

### Fedora

```bash
sudo dnf install clang llvm lld nasm make grub2-pc grub2-efi-x64-modules xorriso
```

### Arch Linux

```bash
sudo pacman -S clang llvm lld nasm make grub xorriso
```

## Building

### Build Everything

```bash
make all
```

This builds:
- Kernel (`build/limitless.elf`)
- HAL library (`build/libhal.a`)
- UEFI bootloader (`build/bootloader/BOOTX64.EFI`)
- BIOS bootloader (`build/bootloader/boot.bin`)
- Universal terminal (`build/userspace/terminal`)

### Build Individual Components

```bash
# Kernel only
make kernel

# Bootloaders
make bootloader

# HAL
make hal

# Userspace
make userspace

# Installer
make installer
```

### Create Bootable ISO

```bash
make iso
```

Output: `dist/limitlessos.iso`

### Architecture Targets

Default architecture is x86_64. To build for ARM64 (future):

```bash
make ARCH=arm64
```

## Build Configuration

Build variables can be overridden:

```bash
# Use different compiler
make CC=gcc

# Debug build
make CFLAGS="-O0 -g"

# Verbose output
make V=1
```

## Directory Structure

```
build/
├── kernel/          # Kernel objects
├── hal/             # HAL objects
├── bootloader/      # Bootloader binaries
└── userspace/       # Userspace binaries

dist/
└── limitlessos.iso  # Bootable ISO image
```

## Testing

### QEMU

Test the built system with QEMU:

```bash
qemu-system-x86_64 -cdrom dist/limitlessos.iso -m 2G
```

### VirtualBox

1. Create a new VM (Type: Other, Version: Other/Unknown 64-bit)
2. Allocate at least 2GB RAM
3. Attach `dist/limitlessos.iso` as optical drive
4. Boot

### Real Hardware

⚠️ **WARNING**: Phase 1 is experimental. Test in VM first.

1. Write ISO to USB:
   ```bash
   sudo dd if=dist/limitlessos.iso of=/dev/sdX bs=4M status=progress
   ```

2. Boot from USB on target machine

## Cleaning

```bash
# Remove all build artifacts
make clean

# Show build configuration
make config
```

## Troubleshooting

### Missing GRUB modules

If ISO creation fails:
```bash
# Ubuntu/Debian
sudo apt install grub-pc-bin grub-efi-amd64-bin

# Fedora
sudo dnf install grub2-efi-x64-modules
```

### Linker errors

Ensure you have LLVM linker (ld.lld):
```bash
which ld.lld
```

### NASM not found

```bash
sudo apt install nasm  # Ubuntu/Debian
sudo dnf install nasm  # Fedora
sudo pacman -S nasm    # Arch
```

## Development

### Code Style

- Kernel: C17, 4-space indentation
- Comments: Doxygen-style for functions
- Headers: Include guards with `LIMITLESS_` prefix

### Adding New Features

1. Update relevant headers in `kernel/include/` or `hal/include/`
2. Implement in `kernel/src/` or `hal/src/`
3. Update `Makefile` if adding new files
4. Test thoroughly in QEMU

## Phase 1 Status

Current implementation:
- ✅ Microkernel core
- ✅ HAL framework (stub drivers)
- ✅ UEFI bootloader (basic)
- ✅ BIOS bootloader (stub)
- ✅ Universal terminal framework
- ✅ Basic installer
- ✅ Local authentication

Not yet implemented:
- Virtual memory paging
- Full IPC system
- Storage drivers
- Network stack
- GUI compositor
- Windows/Linux/macOS personas

See `Roadmap.md` for full feature roadmap.
