# ============================================================================
# Zertreinia OS - Build system
#
# Requires a freestanding i686 cross toolchain. See README.md for setup.
#   - i686-elf-gcc   (C compiler + linker)
#   - nasm           (assembler)
#   - grub-mkrescue  (ISO creation, from GRUB + xorriso)
#   - qemu-system-i386 (optional, for `make run`)
#
# You may override the toolchain on the command line, e.g.:
#   make CC=gcc                (uses host gcc with -m32; less reliable)
# ============================================================================

# ---- Toolchain -------------------------------------------------------------
# Default: a proper i686-elf cross compiler (recommended, see README).
# Quick path: `make USE_HOST_GCC=1 ...` builds with the host gcc using -m32,
# which is handy on a distro that ships gcc but not i686-elf-gcc.
AS      := nasm
GRUB    := grub-mkrescue
QEMU    := qemu-system-i386

ifeq ($(USE_HOST_GCC),1)
  CC          := gcc
  ARCH_CFLAGS := -m32
  ARCH_LDFLAGS:= -m32 -no-pie -Wl,-z,noexecstack
  LIBS        :=
else
  CC          := i686-elf-gcc
  ARCH_CFLAGS :=
  ARCH_LDFLAGS:=
  LIBS        := -lgcc
endif

# ---- Directories -----------------------------------------------------------
BUILD   := build
ISODIR  := isodir
INCLUDE := include

# ---- Flags -----------------------------------------------------------------
# This is the well-trodden, robust flag set for a freestanding i686 kernel.
WARNINGS := -Wall -Wextra -Wno-unused-parameter
CFLAGS   := -std=gnu11 -ffreestanding -O2 -g $(WARNINGS) -I$(INCLUDE) \
            -fno-stack-protector -fno-pic -fno-pie $(ARCH_CFLAGS)
ASFLAGS  := -f elf32
LDFLAGS  := -T linker.ld -ffreestanding -O2 -nostdlib $(ARCH_LDFLAGS)

# ---- Source discovery ------------------------------------------------------
C_SRCS   := $(wildcard kernel/*.c cpu/*.c drivers/*.c mm/*.c libc/*.c fs/*.c gui/*.c)
ASM_SRCS := $(wildcard boot/*.asm cpu/*.asm)

OBJS := $(patsubst %.c,$(BUILD)/%.o,$(C_SRCS)) \
        $(patsubst %.asm,$(BUILD)/%.o,$(ASM_SRCS))

KERNEL := $(BUILD)/kernel.bin
ISO    := Zertreinia.iso

# ---- Targets ---------------------------------------------------------------
.PHONY: all kernel iso run run-kernel debug clean info

all: $(KERNEL)

kernel: $(KERNEL)

# Compile C sources.
$(BUILD)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

# Assemble NASM sources.
$(BUILD)/%.o: %.asm
	@mkdir -p $(dir $@)
	$(AS) $(ASFLAGS) $< -o $@

# Link the kernel binary and verify Multiboot compliance.
$(KERNEL): $(OBJS) linker.ld
	@mkdir -p $(dir $@)
	$(CC) $(LDFLAGS) -o $@ $(OBJS) $(LIBS)
	@echo "--- verifying Multiboot compliance ---"
	@if command -v grub-file >/dev/null 2>&1; then \
		grub-file --is-x86-multiboot $(KERNEL) && echo "OK: $(KERNEL) is Multiboot v1" \
		|| echo "ERROR: $(KERNEL) is NOT Multiboot compliant"; \
	else echo "(grub-file not found, skipping check)"; fi

# Build a bootable ISO using GRUB.
iso: $(KERNEL)
	@mkdir -p $(ISODIR)/boot/grub
	cp $(KERNEL) $(ISODIR)/boot/kernel.bin
	cp iso/boot/grub/grub.cfg $(ISODIR)/boot/grub/grub.cfg
	$(GRUB) -o $(ISO) $(ISODIR)
	@echo "--- created $(ISO) (attach this to VirtualBox / QEMU) ---"

# Quick test: boot the ISO in QEMU (closest to the VirtualBox experience).
run: iso
	$(QEMU) -cdrom $(ISO) -m 64M -serial stdio

# Faster test: let QEMU load the kernel directly via Multiboot.
run-kernel: $(KERNEL)
	$(QEMU) -kernel $(KERNEL) -m 64M -serial stdio

# Boot the ISO and wait for a GDB connection on :1234.
debug: iso
	$(QEMU) -cdrom $(ISO) -m 64M -serial stdio -s -S

clean:
	rm -rf $(BUILD) $(ISODIR) $(ISO)

info:
	@echo "C sources  : $(C_SRCS)"
	@echo "ASM sources: $(ASM_SRCS)"
	@echo "Objects    : $(OBJS)"
	@echo "Kernel     : $(KERNEL)"
	@echo "ISO        : $(ISO)"
