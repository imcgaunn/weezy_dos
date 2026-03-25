# weezyDOS Build System

# --- Toolchain ---
CC       := clang
ASM      := nasm
LD       := ld.lld
OBJCOPY  := llvm-objcopy

# --- Cross-compilation flags (OS build) ---
CFLAGS_KERNEL := -target i686-elf -ffreestanding -nostdlib -Wall -Wextra -std=c11 -O1
ASMFLAGS_BIN  := -f bin
ASMFLAGS_ELF  := -f elf32

# --- Native flags (test build) ---
CFLAGS_TEST   := -DWEEZYDOS_TEST -Wall -Wextra -std=c11 -g -fsanitize=address,undefined

# --- Directories ---
BUILD_DIR := build

# --- Kernel source files ---
KERNEL_C_SRCS := kernel/string.c kernel/console.c kernel/keyboard.c kernel/shell.c kernel/idt.c kernel/main.c
KERNEL_C_OBJS := $(patsubst kernel/%.c,$(BUILD_DIR)/%.o,$(KERNEL_C_SRCS))

# --- ASM kernel objects (32-bit ELF, linked with kernel) ---
KERNEL_ASM_SRCS := boot/idt_asm.asm
KERNEL_ASM_OBJS := $(patsubst boot/%.asm,$(BUILD_DIR)/%.o,$(KERNEL_ASM_SRCS))

# --- Test source files ---
TEST_SRCS := tests/test_main.c tests/test_string.c tests/test_console.c tests/test_keyboard.c tests/test_shell.c

# --- Disk layout ---
STAGE2_SECTORS := 16
FLOPPY_SIZE    := 1474560

# ============================
# OS Build Targets
# ============================

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Assemble boot sectors (flat binary)
$(BUILD_DIR)/stage1.bin: boot/stage1.asm | $(BUILD_DIR)
	$(ASM) $(ASMFLAGS_BIN) $< -o $@

$(BUILD_DIR)/stage2.bin: boot/stage2.asm | $(BUILD_DIR)
	$(ASM) $(ASMFLAGS_BIN) $< -o $@

# Assemble kernel ASM files (ELF objects for linking)
$(BUILD_DIR)/%.o: boot/%.asm | $(BUILD_DIR)
	$(ASM) $(ASMFLAGS_ELF) $< -o $@

# Cross-compile kernel C files
$(BUILD_DIR)/%.o: kernel/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS_KERNEL) -c $< -o $@

# Link kernel ELF
$(BUILD_DIR)/kernel.elf: $(KERNEL_C_OBJS) $(KERNEL_ASM_OBJS) link.ld | $(BUILD_DIR)
	$(LD) -T link.ld -o $@ $(KERNEL_C_OBJS) $(KERNEL_ASM_OBJS)

# Extract flat binary from ELF
$(BUILD_DIR)/kernel.bin: $(BUILD_DIR)/kernel.elf
	$(OBJCOPY) -O binary $< $@

# Build the disk image
$(BUILD_DIR)/disk.img: $(BUILD_DIR)/stage1.bin $(BUILD_DIR)/stage2.bin $(BUILD_DIR)/kernel.bin
	@# Create empty floppy-sized image
	dd if=/dev/zero of=$(BUILD_DIR)/disk.img bs=$(FLOPPY_SIZE) count=1 2>/dev/null
	@# Write stage1 at sector 0
	dd if=$(BUILD_DIR)/stage1.bin of=$(BUILD_DIR)/disk.img bs=512 conv=notrunc 2>/dev/null
	@# Write stage2 at sector 1 (offset 512)
	dd if=$(BUILD_DIR)/stage2.bin of=$(BUILD_DIR)/disk.img bs=512 seek=1 conv=notrunc 2>/dev/null
	@# Write kernel at sector 17 (offset 512 + 16*512 = 8704)
	dd if=$(BUILD_DIR)/kernel.bin of=$(BUILD_DIR)/disk.img bs=512 seek=17 conv=notrunc 2>/dev/null
	@echo "Disk image built: $(BUILD_DIR)/disk.img"

# ============================
# Test Targets
# ============================

$(BUILD_DIR)/test_runner: $(KERNEL_C_SRCS) $(TEST_SRCS) | $(BUILD_DIR)
	$(CC) $(CFLAGS_TEST) -I. -o $@ $(TEST_SRCS) $(KERNEL_C_SRCS)

.PHONY: test
test: $(BUILD_DIR)/test_runner
	./$(BUILD_DIR)/test_runner

# ============================
# Convenience
# ============================

.PHONY: all
all: $(BUILD_DIR)/disk.img

.PHONY: boot
boot: $(BUILD_DIR)/stage1.bin $(BUILD_DIR)/stage2.bin
	@echo "Boot sectors assembled"

.PHONY: run
run: all
	qemu-system-x86_64 -drive format=raw,file=$(BUILD_DIR)/disk.img -nographic -serial mon:stdio

.PHONY: run-gui
run-gui: all
	qemu-system-x86_64 -drive format=raw,file=$(BUILD_DIR)/disk.img

.PHONY: clean
clean:
	rm -rf $(BUILD_DIR)
