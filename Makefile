# weezyDOS Build System
# Two build paths: cross-compiled OS and native host tests

# --- Toolchain ---
CC       := clang
ASM      := nasm
LD       := ld.lld

# --- Cross-compilation flags (OS build) ---
CFLAGS_KERNEL := -target i686-elf -ffreestanding -nostdlib -Wall -Wextra -std=c11 -O1
ASMFLAGS      := -f elf32

# --- Native flags (test build) ---
CFLAGS_TEST   := -DWEEZYDOS_TEST -Wall -Wextra -std=c11 -g -fsanitize=address,undefined

# --- Directories ---
BUILD_DIR := build

# --- Kernel source files (add new .c files here) ---
KERNEL_SRCS := kernel/string.c kernel/console.c kernel/keyboard.c kernel/shell.c

# --- Test source files (add new test files here) ---
TEST_SRCS := tests/test_main.c tests/test_string.c tests/test_console.c tests/test_keyboard.c tests/test_shell.c

# --- OS Build Targets ---

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Assemble boot sector
$(BUILD_DIR)/stage1.bin: boot/stage1.asm | $(BUILD_DIR)
	$(ASM) -f bin $< -o $@

$(BUILD_DIR)/stage2.bin: boot/stage2.asm | $(BUILD_DIR)
	$(ASM) -f bin $< -o $@

# Cross-compile kernel C files
$(BUILD_DIR)/%.o: kernel/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS_KERNEL) -c $< -o $@

# --- Test Targets ---

$(BUILD_DIR)/test_runner: $(KERNEL_SRCS) $(TEST_SRCS) | $(BUILD_DIR)
	$(CC) $(CFLAGS_TEST) -I. -o $@ $(TEST_SRCS) $(KERNEL_SRCS)

.PHONY: test
test: $(BUILD_DIR)/test_runner
	./$(BUILD_DIR)/test_runner

# --- Boot target (just assembles, no full OS yet) ---

.PHONY: boot
boot: $(BUILD_DIR)/stage1.bin $(BUILD_DIR)/stage2.bin
	@echo "Boot sectors assembled"

# --- Convenience ---

.PHONY: clean
clean:
	rm -rf $(BUILD_DIR)

.PHONY: all
all: boot

.PHONY: run
run: all
	@echo "Full OS image not yet built — boot target only assembles stage1"
