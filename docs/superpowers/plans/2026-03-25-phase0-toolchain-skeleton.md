# Phase 0: Toolchain & Skeleton Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Set up the build system, test framework, and project skeleton so that `make test` runs a passing host-side test and `make boot.bin` assembles a NASM bootloader stub.

**Architecture:** Makefile with two build paths — cross-compilation for OS (`clang -target i686-elf -ffreestanding`) and native compilation for tests (`clang` with `-DWEEZYDOS_TEST`). Minimal test framework as C macros. A stub bootloader that assembles but doesn't boot yet.

**Tech Stack:** Clang/LLVM, NASM, ld.lld, GNU Make, QEMU (verified installed but not used yet)

---

### Task 1: Initialize Git Repository

**Files:**
- Existing: `.gitignore`
- Existing: `CLAUDE.md`

- [ ] **Step 1: Initialize the git repo and make the initial commit**

```bash
cd /home/imcgaunn/git/github.com/imcgaunn/weezy_dos
git init
git add CLAUDE.md .gitignore docs/
git commit -m "Initial commit: project skeleton with design spec and CLAUDE.md"
```

---

### Task 2: Verify Toolchain Prerequisites

**Files:** None created — verification only.

- [ ] **Step 1: Verify clang, nasm, ld.lld, and qemu are installed**

```bash
clang --version
nasm --version
ld.lld --version
qemu-system-x86_64 --version
```

Expected: All four commands print version info. If any is missing, install it:

```bash
# Ubuntu/Debian
sudo apt-get install clang lld nasm qemu-system-x86
```

- [ ] **Step 2: Verify clang can cross-compile to i686-elf**

Create a temporary test file and compile it:

```bash
echo 'void _start(void) { for(;;); }' > /tmp/test_cross.c
clang -target i686-elf -ffreestanding -nostdlib -c /tmp/test_cross.c -o /tmp/test_cross.o
file /tmp/test_cross.o
rm /tmp/test_cross.c /tmp/test_cross.o
```

Expected: `file` output shows `ELF 32-bit LSB relocatable, Intel 80386`.

---

### Task 3: Create Fixed-Width Types Header

**Files:**
- Create: `kernel/types.h`

- [ ] **Step 1: Create kernel/types.h**

```c
#ifndef WEEZYDOS_TYPES_H
#define WEEZYDOS_TYPES_H

#ifdef WEEZYDOS_TEST
#include <stdint.h>
#include <stddef.h>
#else
typedef unsigned char      uint8_t;
typedef unsigned short     uint16_t;
typedef unsigned int       uint32_t;
typedef unsigned long long uint64_t;
typedef signed char        int8_t;
typedef signed short       int16_t;
typedef signed int         int32_t;
typedef signed long long   int64_t;
typedef uint32_t           uintptr_t;
typedef uint32_t           size_t;
#define NULL ((void *)0)
#endif

#endif
```

- [ ] **Step 2: Commit**

```bash
git add kernel/types.h
git commit -m "Add fixed-width types header with test/freestanding split"
```

---

### Task 4: Create the Test Framework

**Files:**
- Create: `tests/test.h`
- Create: `tests/test_main.c`

- [ ] **Step 1: Create tests/test.h**

This is the entire test framework — ~50 lines of macros.

```c
#ifndef WEEZYDOS_TEST_H
#define WEEZYDOS_TEST_H

#include <stdio.h>
#include <string.h>

static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) \
    void test_##name(void); \
    void test_##name(void)

#define ASSERT(expr) do { \
    if (!(expr)) { \
        printf("  FAIL: %s:%d: %s\n", __FILE__, __LINE__, #expr); \
        tests_failed++; \
        return; \
    } \
} while (0)

#define ASSERT_EQ(a, b) do { \
    if ((a) != (b)) { \
        printf("  FAIL: %s:%d: %s != %s\n", __FILE__, __LINE__, #a, #b); \
        tests_failed++; \
        return; \
    } \
} while (0)

#define ASSERT_STR_EQ(a, b) do { \
    if (strcmp((a), (b)) != 0) { \
        printf("  FAIL: %s:%d: \"%s\" != \"%s\"\n", __FILE__, __LINE__, (a), (b)); \
        tests_failed++; \
        return; \
    } \
} while (0)

#define ASSERT_NOT_NULL(ptr) do { \
    if ((ptr) == NULL) { \
        printf("  FAIL: %s:%d: %s is NULL\n", __FILE__, __LINE__, #ptr); \
        tests_failed++; \
        return; \
    } \
} while (0)

#define RUN_TEST(name) do { \
    tests_run++; \
    printf("  %s... ", #name); \
    test_##name(); \
    if (tests_failed == 0 || tests_passed == tests_run - 1) { \
        tests_passed++; \
        printf("OK\n"); \
    } \
} while (0)

/* Use this in test_main.c to conditionally run a single test by name */
#define RUN_TEST_FILTERED(name, filter) do { \
    if (filter == NULL || strcmp(filter, #name) == 0) { \
        RUN_TEST(name); \
    } \
} while (0)

#endif
```

- [ ] **Step 2: Create tests/test_main.c**

```c
#include "test.h"

/* Declare test functions from test files */
void test_string_length(void);

/* Forward declare test runner functions */
void run_string_tests(const char *filter);

int main(int argc, char *argv[]) {
    const char *filter = (argc > 1) ? argv[1] : NULL;

    printf("weezyDOS test runner\n");
    printf("====================\n\n");

    printf("[string]\n");
    run_string_tests(filter);

    printf("\n====================\n");
    printf("Results: %d passed, %d failed, %d total\n",
           tests_passed, tests_failed, tests_run);

    return tests_failed > 0 ? 1 : 0;
}
```

- [ ] **Step 3: Commit**

```bash
git add tests/test.h tests/test_main.c
git commit -m "Add minimal test framework with macros and test runner"
```

---

### Task 5: Create String Utilities with TDD

**Files:**
- Create: `kernel/string.h`
- Create: `kernel/string.c`
- Create: `tests/test_string.c`

- [ ] **Step 1: Write the failing test**

Create `tests/test_string.c`:

```c
#include "test.h"
#include "../kernel/string.h"

TEST(string_length_empty) {
    ASSERT_EQ(wdos_strlen(""), 0);
}

TEST(string_length_hello) {
    ASSERT_EQ(wdos_strlen("hello"), 5);
}

TEST(string_length_one_char) {
    ASSERT_EQ(wdos_strlen("x"), 1);
}

void run_string_tests(const char *filter) {
    RUN_TEST_FILTERED(string_length_empty, filter);
    RUN_TEST_FILTERED(string_length_hello, filter);
    RUN_TEST_FILTERED(string_length_one_char, filter);
}
```

- [ ] **Step 2: Create the header file**

Create `kernel/string.h`:

```c
#ifndef WEEZYDOS_STRING_H
#define WEEZYDOS_STRING_H

#include "types.h"

size_t wdos_strlen(const char *str);

#endif
```

- [ ] **Step 3: Try to compile to verify the test fails**

```bash
clang -DWEEZYDOS_TEST -I. -o /tmp/test_runner tests/test_main.c tests/test_string.c 2>&1
```

Expected: Linker error — `undefined reference to 'wdos_strlen'`. The test correctly fails because the implementation doesn't exist yet.

- [ ] **Step 4: Write the minimal implementation**

Create `kernel/string.c`:

```c
#include "string.h"

size_t wdos_strlen(const char *str) {
    size_t len = 0;
    while (str[len] != '\0') {
        len++;
    }
    return len;
}
```

- [ ] **Step 5: Compile and run tests to verify they pass**

```bash
clang -DWEEZYDOS_TEST -I. -o /tmp/test_runner tests/test_main.c tests/test_string.c kernel/string.c && /tmp/test_runner
```

Expected output:
```
weezyDOS test runner
====================

[string]
  string_length_empty... OK
  string_length_hello... OK
  string_length_one_char... OK

====================
Results: 3 passed, 0 failed, 3 total
```

- [ ] **Step 6: Run a single test by name**

```bash
/tmp/test_runner string_length_hello
```

Expected: Only `string_length_hello` runs and passes.

- [ ] **Step 7: Commit**

```bash
git add kernel/string.h kernel/string.c tests/test_string.c
git commit -m "Add wdos_strlen with passing tests"
```

---

### Task 6: Create the Makefile

**Files:**
- Create: `Makefile`

- [ ] **Step 1: Create the Makefile**

```makefile
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
KERNEL_SRCS := kernel/string.c

# --- Test source files (add new test files here) ---
TEST_SRCS := tests/test_main.c tests/test_string.c

# --- OS Build Targets ---

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Assemble boot sector
$(BUILD_DIR)/stage1.bin: boot/stage1.asm | $(BUILD_DIR)
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
boot: $(BUILD_DIR)/stage1.bin
	@echo "Boot sector assembled: $(BUILD_DIR)/stage1.bin"

# --- Convenience ---

.PHONY: clean
clean:
	rm -rf $(BUILD_DIR)

.PHONY: all
all: boot

.PHONY: run
run: all
	@echo "Full OS image not yet built — boot target only assembles stage1"
```

- [ ] **Step 2: Verify `make test` works**

```bash
make test
```

Expected: Compiles and runs all tests, all pass. Output ends with `Results: 3 passed, 0 failed, 3 total`.

- [ ] **Step 3: Commit**

```bash
git add Makefile
git commit -m "Add Makefile with test and boot build targets"
```

---

### Task 7: Create Stub Bootloader

**Files:**
- Create: `boot/stage1.asm`

- [ ] **Step 1: Create boot/stage1.asm**

A minimal MBR bootloader that prints a character and halts. Not functional yet — just proves NASM assembles and the binary is 512 bytes with the boot signature.

```nasm
; weezyDOS Stage 1 Bootloader
; Loaded by BIOS at 0x7C00
; For now: prints 'W' and halts

[BITS 16]
[ORG 0x7C00]

start:
    ; Set up segments
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00          ; Stack grows down from bootloader

    ; Print 'W' using BIOS teletype
    mov ah, 0x0E            ; BIOS teletype function
    mov al, 'W'             ; Character to print
    mov bh, 0               ; Page number
    int 0x10                ; BIOS video interrupt

    ; Halt
    cli
    hlt
    jmp $                   ; Infinite loop in case of NMI

; Pad to 510 bytes and add boot signature
times 510 - ($ - $$) db 0
dw 0xAA55
```

- [ ] **Step 2: Assemble with make**

```bash
make boot
```

Expected output:
```
nasm -f bin boot/stage1.asm -o build/stage1.bin
Boot sector assembled: build/stage1.bin
```

- [ ] **Step 3: Verify the binary is exactly 512 bytes with correct boot signature**

```bash
wc -c build/stage1.bin
xxd build/stage1.bin | tail -1
```

Expected: `512 build/stage1.bin` and the last line of xxd shows `aa55` at the end.

- [ ] **Step 4: Quick-test in QEMU (manual verification)**

```bash
qemu-system-x86_64 -drive format=raw,file=build/stage1.bin -nographic -serial mon:stdio 2>/dev/null &
sleep 2 && kill %1 2>/dev/null
```

This is a manual sanity check — QEMU should boot and display 'W'. It will be a proper `make run` target later.

- [ ] **Step 5: Commit**

```bash
git add boot/stage1.asm
git commit -m "Add stub Stage 1 bootloader that prints 'W' and halts"
```

---

### Task 8: Create Linker Script

**Files:**
- Create: `link.ld`

- [ ] **Step 1: Create link.ld**

This linker script places the kernel at 0x10000 (where Stage 2 will load it). Not used yet — but must be in place before any kernel linking.

```ld
/* weezyDOS kernel linker script */
/* Kernel is loaded at 0x10000 by the Stage 2 bootloader */

ENTRY(kmain)

SECTIONS
{
    . = 0x10000;

    .text : {
        *(.text)
    }

    .rodata : {
        *(.rodata)
    }

    .data : {
        *(.data)
    }

    .bss : {
        *(.bss)
        *(COMMON)
    }

    /DISCARD/ : {
        *(.comment)
        *(.note.*)
    }
}
```

- [ ] **Step 2: Verify it works with a dummy kernel object**

```bash
echo 'void kmain(void) { for(;;); }' > /tmp/dummy_kernel.c
clang -target i686-elf -ffreestanding -nostdlib -c /tmp/dummy_kernel.c -o /tmp/dummy_kernel.o
ld.lld -T link.ld -o /tmp/dummy_kernel.elf /tmp/dummy_kernel.o
file /tmp/dummy_kernel.elf
rm /tmp/dummy_kernel.c /tmp/dummy_kernel.o /tmp/dummy_kernel.elf
```

Expected: `file` shows `ELF 32-bit LSB executable, Intel 80386`.

- [ ] **Step 3: Commit**

```bash
git add link.ld
git commit -m "Add kernel linker script placing kernel at 0x10000"
```

---

### Task 9: Verify Everything Works End-to-End

**Files:** None — verification only.

- [ ] **Step 1: Clean build and run tests**

```bash
make clean && make test
```

Expected: All 3 tests pass.

- [ ] **Step 2: Build boot sector**

```bash
make boot
```

Expected: `build/stage1.bin` is 512 bytes.

- [ ] **Step 3: Verify project structure**

```bash
find . -not -path './build/*' -not -path './.git/*' -not -path './.superpowers/*' -not -name '.gitignore' -type f | sort
```

Expected file listing:
```
./CLAUDE.md
./Makefile
./boot/stage1.asm
./docs/superpowers/specs/2026-03-25-weezydos-design.md
./docs/superpowers/plans/2026-03-25-phase0-toolchain-skeleton.md
./kernel/string.c
./kernel/string.h
./kernel/types.h
./link.ld
./tests/test.h
./tests/test_main.c
./tests/test_string.c
```

Phase 0 is complete. The foundation is in place for Phase 1: Boot to Prompt.
