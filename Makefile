# LimitlessOS Build System
# Enterprise-grade makefile for cross-platform kernel and userspace compilation

.PHONY: all clean kernel bootloader hal userspace installer iso test help

# Build configuration
BUILD_DIR := build
DIST_DIR := dist
ISO_DIR := $(BUILD_DIR)/iso

# Architecture targets (default: x86_64)
ARCH ?= x86_64
TARGET_TRIPLE := $(ARCH)-limitlessos

# Compiler toolchain
CC := clang
CXX := clang++
AS := nasm
LD := ld.lld
AR := ar
OBJCOPY := llvm-objcopy

# Common flags
WARNINGS := -Wall -Wextra -Werror -Wno-unused-parameter
COMMON_CFLAGS := $(WARNINGS) -std=c17 -O2 -g -fno-stack-protector -fno-pie \
				 -mno-red-zone -mcmodel=kernel -ffreestanding -nostdlib \
				 -fno-omit-frame-pointer -mno-sse -mno-sse2
COMMON_CXXFLAGS := $(WARNINGS) -std=c++20 -O2 -g -fno-stack-protector -fno-pie \
				   -mno-red-zone -mcmodel=kernel -ffreestanding -nostdlib \
				   -fno-omit-frame-pointer -mno-sse -mno-sse2 -fno-exceptions -fno-rtti

# Architecture-specific flags
ifeq ($(ARCH),x86_64)
    AS_FORMAT := elf64
    ARCH_CFLAGS := -target x86_64-unknown-none
endif

# Build flags
CFLAGS := $(COMMON_CFLAGS) $(ARCH_CFLAGS) -Ikernel/include -Ihal/include
CXXFLAGS := $(COMMON_CXXFLAGS) $(ARCH_CFLAGS) -Ikernel/include -Ihal/include
ASFLAGS := -f $(AS_FORMAT)
LDFLAGS := -nostdlib -static -z max-page-size=0x1000

# Kernel components (including PE loader)
KERNEL_SOURCES := $(wildcard kernel/src/*.c kernel/src/arch/$(ARCH)/*.c kernel/src/fs/*.c kernel/src/drivers/*.c kernel/src/net/*.c)
KERNEL_ASM_SOURCES := $(wildcard kernel/src/arch/$(ARCH)/*.asm)
KERNEL_OBJECTS := $(patsubst kernel/src/%.c,$(BUILD_DIR)/kernel/%.o,$(KERNEL_SOURCES))
KERNEL_ASM_OBJECTS := $(patsubst kernel/src/arch/$(ARCH)/%.asm,$(BUILD_DIR)/kernel/arch/$(ARCH)/%.o,$(KERNEL_ASM_SOURCES))
KERNEL_BINARY := $(BUILD_DIR)/limitless.elf

# HAL components
HAL_SOURCES := $(wildcard hal/src/*.c hal/src/arch/$(ARCH)/*.c)
HAL_OBJECTS := $(patsubst hal/src/%.c,$(BUILD_DIR)/hal/%.o,$(HAL_SOURCES))
HAL_LIBRARY := $(BUILD_DIR)/libhal.a

# Bootloader
BOOTLOADER_EFI := $(BUILD_DIR)/bootloader/BOOTX64.EFI
BOOTLOADER_LEGACY := $(BUILD_DIR)/bootloader/boot.bin

# Userspace
USERSPACE_SOURCES := $(wildcard userspace/src/*.c)
USERSPACE_OBJECTS := $(patsubst userspace/src/%.c,$(BUILD_DIR)/userspace/%.o,$(USERSPACE_SOURCES))
USERSPACE_TERMINAL := $(BUILD_DIR)/userspace/terminal

# Default target
all: kernel bootloader userspace

# Help target
help:
	@echo "LimitlessOS Build System"
	@echo ""
	@echo "Targets:"
	@echo "  all          - Build kernel, bootloader, and userspace (default)"
	@echo "  kernel       - Build the microkernel"
	@echo "  hal          - Build the Hardware Abstraction Layer"
	@echo "  bootloader   - Build UEFI and BIOS bootloaders"
	@echo "  userspace    - Build userspace components"
	@echo "  installer    - Build the installer"
	@echo "  iso          - Create bootable ISO image"
	@echo "  test         - Run test suite"
	@echo "  clean        - Remove all build artifacts"
	@echo ""
	@echo "Variables:"
	@echo "  ARCH=x86_64  - Target architecture (default: x86_64)"

# Create build directories
$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)/kernel/arch/$(ARCH)
	@mkdir -p $(BUILD_DIR)/hal/arch/$(ARCH)
	@mkdir -p $(BUILD_DIR)/bootloader
	@mkdir -p $(BUILD_DIR)/userspace
	@mkdir -p $(DIST_DIR)
	@mkdir -p $(ISO_DIR)/boot/grub

# HAL library
hal: $(HAL_LIBRARY)

$(HAL_LIBRARY): $(HAL_OBJECTS) | $(BUILD_DIR)
	@echo "[AR] $@"
	@$(AR) rcs $@ $^

$(BUILD_DIR)/hal/%.o: hal/src/%.c | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	@echo "[CC] $<"
	@$(CC) $(CFLAGS) -c $< -o $@

# Kernel
kernel: $(KERNEL_BINARY)

$(KERNEL_BINARY): $(KERNEL_OBJECTS) $(KERNEL_ASM_OBJECTS) $(HAL_LIBRARY) | $(BUILD_DIR)
	@echo "[LD] $@"
	@$(LD) $(LDFLAGS) -T kernel/linker.ld -o $@ $(KERNEL_ASM_OBJECTS) $(KERNEL_OBJECTS) $(HAL_LIBRARY)
	@echo "Kernel built successfully: $@"

$(BUILD_DIR)/kernel/%.o: kernel/src/%.c | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	@echo "[CC] $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/kernel/arch/$(ARCH)/%.o: kernel/src/arch/$(ARCH)/%.asm | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	@echo "[AS] $<"
	@$(AS) $(ASFLAGS) $< -o $@

# Bootloader
bootloader: $(BOOTLOADER_EFI) $(BOOTLOADER_LEGACY)

$(BOOTLOADER_EFI): bootloader/efi/main.c bootloader/efi/console.c | $(BUILD_DIR)
	@echo "[CC] UEFI Bootloader"
	@$(CC) -target x86_64-unknown-windows -ffreestanding -fshort-wchar \
		-mno-red-zone -I bootloader/efi/include \
		-c bootloader/efi/main.c -o $(BUILD_DIR)/bootloader/efi_main.o
	@$(CC) -target x86_64-unknown-windows -ffreestanding -fshort-wchar \
		-mno-red-zone -I bootloader/efi/include \
		-c bootloader/efi/console.c -o $(BUILD_DIR)/bootloader/efi_console.o
	@echo "[LD] $@"
	@$(LD) -flavor link -subsystem:efi_application -entry:efi_main \
		$(BUILD_DIR)/bootloader/efi_main.o $(BUILD_DIR)/bootloader/efi_console.o -out:$@

$(BOOTLOADER_LEGACY): bootloader/bios/boot.asm | $(BUILD_DIR)
	@echo "[AS] BIOS Bootloader"
	@$(AS) -f bin bootloader/bios/boot.asm -o $@

# Userspace
userspace: $(USERSPACE_TERMINAL) $(USERSPACE_OBJECTS)

# Userspace library objects
$(BUILD_DIR)/userspace/%.o: userspace/src/%.c | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	@echo "[CC] $<"
	@$(CC) -O2 -Wall -Iuserspace/include -c $< -o $@

$(USERSPACE_TERMINAL): userspace/terminal/main.c $(USERSPACE_OBJECTS) | $(BUILD_DIR)
	@echo "[CC] Universal Terminal"
	@$(CC) -O2 -Wall -Iuserspace/terminal/include -Iuserspace/include \
		userspace/terminal/main.c userspace/terminal/shim.c \
		userspace/terminal/parser.c $(USERSPACE_OBJECTS) -o $@

# Installer
installer:
	@echo "[BUILD] Installer"
	@$(MAKE) -C installer

# ISO image
iso: all
	@echo "[ISO] Creating bootable image"
	@mkdir -p $(ISO_DIR)/boot
	@cp $(KERNEL_BINARY) $(ISO_DIR)/boot/limitless.elf
	@cp $(BOOTLOADER_LEGACY) $(ISO_DIR)/boot/boot.bin
	@echo 'set timeout=3' > $(ISO_DIR)/boot/grub/grub.cfg
	@echo 'set default=0' >> $(ISO_DIR)/boot/grub/grub.cfg
	@echo 'menuentry "LimitlessOS" {' >> $(ISO_DIR)/boot/grub/grub.cfg
	@echo '    multiboot2 /boot/limitless.elf' >> $(ISO_DIR)/boot/grub/grub.cfg
	@echo '    boot' >> $(ISO_DIR)/boot/grub/grub.cfg
	@echo '}' >> $(ISO_DIR)/boot/grub/grub.cfg
	@grub-mkrescue -o $(DIST_DIR)/limitlessos.iso $(ISO_DIR)
	@echo "ISO created: $(DIST_DIR)/limitlessos.iso"

# Tests
test:
	@echo "[TEST] Running test suite"
	@$(MAKE) -C test

# Clean
clean:
	@echo "[CLEAN] Removing build artifacts"
	@rm -rf $(BUILD_DIR) $(DIST_DIR)
	@echo "Clean complete"

# Print configuration
config:
	@echo "Build Configuration:"
	@echo "  ARCH:    $(ARCH)"
	@echo "  CC:      $(CC)"
	@echo "  LD:      $(LD)"
	@echo "  BUILD:   $(BUILD_DIR)"
	@echo "  DIST:    $(DIST_DIR)"
