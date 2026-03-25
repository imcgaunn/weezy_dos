# weezyDOS Design Spec

## Overview

weezyDOS is a simple DOS-like operating system built from scratch for educational purposes. It boots via BIOS legacy mode on x86, presents a command-line shell, provides a FAT16 filesystem, and can load and execute flat binary programs from disk. The entire system is developed using TDD with host-side unit tests.

**Target:** QEMU with KVM acceleration (x86_64 host, boots in 32-bit protected mode).
**Toolchain:** Clang/LLVM + NASM + ld.lld.
**Stretch goal:** ARM64 support via UEFI on qemu-system-aarch64.

## Architecture

### Monolithic Flat Binary

A two-stage ASM bootloader hands off to a single C kernel binary. All OS services live in one address space — faithful to how DOS actually worked.

### Boot Sequence (x86, BIOS legacy)

1. **Stage 1** (`boot/stage1.asm`, 512 bytes) — BIOS loads MBR at 0x7C00. Loads Stage 2 from subsequent disk sectors.
2. **Stage 2** (`boot/stage2.asm`) — Sets up GDT, enables A20 gate, switches CPU to 32-bit protected mode. Loads the kernel binary from disk into memory at 0x10000.
3. **C Kernel** (`kernel/main.c`, `kmain()`) — Initializes console, memory, disk, filesystem, and drops into the command shell loop.

### Memory Map

| Address Range | Purpose |
|---|---|
| 0x00000000 - 0x000003FF | IVT (Interrupt Vector Table) |
| 0x00000400 - 0x000004FF | BIOS Data Area |
| 0x00007C00 - 0x00007DFF | Stage 1 Bootloader (MBR) |
| 0x00008000 - 0x0000FFFF | Stage 2 Bootloader |
| 0x00010000 - 0x0007FFFF | Kernel |
| 0x00080000 - 0x0009FFFF | User programs / heap |
| 0x000A0000 - 0x000BFFFF | Video memory (VGA) |
| 0x000C0000 - 0x000FFFFF | BIOS ROM |

### Kernel Components

All implemented in C, compiled into a single binary:

- **Console I/O** (`console.c`) — VGA text mode driver (writes to 0xB8000), keyboard input via ISR (port 0x60).
- **Memory Manager** (`memory.c`) — Simple block allocator over the user memory area (0x80000–0x9FFFF). 16-byte aligned allocations.
- **FAT Filesystem** (`fat.c`) — FAT16 read/write. Parses BPB, follows cluster chains, reads/writes directory entries. 8.3 filenames, flat root directory (no subdirectories in v1).
- **Disk Driver** (`disk.c`) — ATA PIO mode sector read/write via port I/O.
- **Program Loader** (`loader.c`) — Loads flat binary .COM files from FAT into user memory at 0x80000 and jumps to them.
- **Command Shell** (`shell.c`) — COMMAND.COM equivalent. Tokenizes input, dispatches built-in commands, falls back to loading external programs.
- **String Utilities** (`string.c`) — Freestanding implementations of strcmp, memcpy, strlen, itoa, etc.

## FAT16 Filesystem

### Disk Layout

| Region | Description |
|---|---|
| Sector 0 | Boot sector + BIOS Parameter Block (BPB) |
| Sectors 1–N | Reserved sectors (Stage 2 + kernel) |
| FAT Table 1 | File Allocation Table |
| FAT Table 2 | Backup FAT |
| Root Directory | Fixed-size, 512 entries max |
| Data Area | File contents, cluster-aligned |

The OS itself lives in the reserved sectors — no filesystem entry needed for the kernel.

### Operations

**Phase 1 (read-only):** Read BPB, read FAT table, list root directory, find file by name, read file contents (follow cluster chain).

**Phase 2 (read-write):** Allocate clusters, create directory entries, write file contents, delete files (free clusters + remove entry).

## Command Shell

### Built-in Commands

| Command | Description | Phase |
|---|---|---|
| DIR | List files in root directory | 1 |
| TYPE \<file\> | Print file contents | 1 |
| ECHO \<text\> | Print text to screen | 1 |
| CLS | Clear screen | 1 |
| VER | Show weezyDOS version | 1 |
| HELP | List available commands | 1 |
| MEM | Show memory usage | 1 |
| COPY \<src\> \<dst\> | Copy a file | 2 |
| DEL \<file\> | Delete a file | 2 |

### External Program Execution

If input doesn't match a built-in command:
1. Append `.COM` if no extension given.
2. Search root directory for the file.
3. Load flat binary into user memory (0x80000+).
4. Jump to loaded address (execution begins at first byte).
5. Program returns via INT 0x20 or far return.

### System Calls (INT 0x21)

Programs call OS services via `INT 0x21` with AH = function number:
- Print character, print string, read character, terminate program.

## Program Format (.COM)

Flat binaries with no headers or relocation. Compiled with `clang -target i686-elf -Ttext=0x80000`. Execution begins at byte 0 of the loaded file.

## Project Structure

```
weezy_dos/
├── boot/
│   ├── stage1.asm          # MBR bootloader (512 bytes)
│   ├── stage2.asm          # Second stage — loads kernel, enters protected mode
│   └── gdt.asm             # GDT definitions
├── kernel/
│   ├── main.c              # kmain() entry point
│   ├── console.c/.h        # VGA text mode + keyboard
│   ├── memory.c/.h         # Block allocator
│   ├── fat.c/.h            # FAT16 filesystem
│   ├── disk.c/.h           # ATA PIO disk driver
│   ├── loader.c/.h         # Program loader
│   ├── shell.c/.h          # Command interpreter
│   ├── string.c/.h         # Freestanding string utilities
│   └── types.h             # Fixed-width integer types
├── tests/
│   ├── test_main.c         # Test runner entry point
│   ├── test_memory.c       # Memory allocator tests
│   ├── test_fat.c          # FAT filesystem tests
│   ├── test_shell.c        # Command parser tests
│   ├── test_string.c       # String utility tests
│   └── test_loader.c       # Program loader tests
├── scripts/
│   ├── create_disk.sh      # Build a FAT-formatted disk image
│   └── run_qemu.sh         # Launch weezyDOS in QEMU
├── link.ld                 # Linker script for kernel binary
├── Makefile                # Build system
└── CLAUDE.md
```

## Build System

### OS Build (`make all`)

1. NASM assembles `boot/*.asm` → `boot/*.bin`
2. Clang cross-compiles (`-target i686-elf -ffreestanding`) `kernel/*.c` → `kernel/*.o`
3. ld.lld links `kernel/*.o` → `kernel.bin` (using `link.ld`)
4. `cat` + `dd` combines stage1.bin + stage2.bin + kernel.bin → `weezy.img`
5. `scripts/create_disk.sh` creates a FAT16-formatted disk image with the OS injected into reserved sectors

### Test Build (`make test`)

1. Clang compiles (host target, not cross) `kernel/*.c` + `tests/*.c` with `-DWEEZYDOS_TEST`
2. Links against host libc → `build/test_runner` (native Linux executable)
3. Runs `./build/test_runner` — no QEMU needed

### Run (`make run`)

Launches `qemu-system-x86_64 -drive format=raw,file=build/disk.img -enable-kvm`

## TDD Strategy

### Dual Compilation

The same kernel C source compiles two ways:
- **For the OS:** cross-compiled to i686-elf, freestanding, no stdlib.
- **For tests:** compiled natively with `-DWEEZYDOS_TEST` to stub hardware access and link against host libc.

### The Seam Pattern

Hardware-touching code is never called directly by logic modules. Each hardware module exposes its interface through a struct of function pointers (e.g., `disk_ops.read_sector`). Tests swap in fake implementations:
- FAT tests use a RAM-backed byte array as the "disk."
- Console tests use a buffer instead of VGA memory.
- Memory tests use a host-allocated heap.

### What Gets Host-Tested

- Memory allocator — alloc, free, coalescing, alignment
- FAT driver — BPB parsing, cluster chain traversal, directory operations (all against byte-array disk)
- Shell parser — tokenization, command matching, argument parsing
- String utilities — all freestanding string functions
- Program loader — file validation, memory region setup

### What Requires QEMU

- Bootloader ASM (mode switching, GDT/IDT setup)
- VGA text output (memory-mapped I/O at 0xB8000)
- Keyboard ISR (port 0x60)
- ATA disk I/O (port I/O)
- Interrupt dispatch

These are kept as thin as possible — just enough to bridge C logic to hardware.

### Test Framework

Minimal custom framework (~50 lines of C macros). No external dependencies.

```c
TEST(memory_alloc_returns_aligned_block) {
    mem_init(test_heap, HEAP_SIZE);
    void *p = mem_alloc(64);
    ASSERT_NOT_NULL(p);
    ASSERT(((uintptr_t)p % 16) == 0);
}
```

Run all tests: `make test`
Run single test: `./build/test_runner memory_alloc_returns_aligned_block`

## Development Phases

### Phase 0: Toolchain & Skeleton
Makefile, linker script, test framework, verify cross-compilation. One passing test (`string_length`). One NASM file that assembles.

### Phase 1: Boot to Prompt
Stage 1 + Stage 2 bootloader. Protected mode entry. VGA console output. Keyboard input. Shell loop (read line, echo back).
**Milestone:** Boots in QEMU, shows `A:\>` prompt, echoes typed text.

### Phase 2: Built-in Commands
Shell command parser (TDD). Built-in commands: ECHO, CLS, VER, HELP, MEM. Memory allocator (TDD).
**Milestone:** Shell parses and dispatches commands.

### Phase 3: Filesystem (Read-Only)
ATA PIO disk driver. FAT16 BPB parsing (TDD). Directory listing (TDD). File reading via cluster chain (TDD). DIR and TYPE commands.
**Milestone:** `DIR` lists files, `TYPE` prints file contents from a FAT16 disk image.

### Phase 4: Program Loading
Load .COM flat binary from FAT into user memory. INT 0x21 handler for syscalls (print string, exit).
**Milestone:** Type `HELLO` at prompt, loads HELLO.COM from disk, runs it, returns to shell.

### Phase 5: Read-Write Filesystem
FAT cluster allocation (TDD). File creation and writing (TDD). COPY and DEL commands.
**Milestone:** Can create, copy, and delete files on the disk image.

### Stretch: ARM64 Support
UEFI boot stub for AArch64. UART console (instead of VGA). virtio-blk disk (instead of ATA PIO). Same kernel C code, different hardware layer.
**Milestone:** Boots on `qemu-system-aarch64 -machine virt`.
