# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

weezyDOS is a simple DOS-like operating system built from scratch for x86. It boots via BIOS legacy mode, provides a FAT16 filesystem, a command shell, and can load/execute flat binary (.COM) programs. Developed using TDD with host-side unit tests.

**Toolchain:** Clang/LLVM (`-target i686-elf -ffreestanding`) + NASM + ld.lld

## Build Commands

```bash
make all          # Build bootable disk image (build/disk.img)
make test         # Build and run all host-side unit tests
make run          # Launch weezyDOS in QEMU with KVM
make clean        # Remove build artifacts
```

Run a single test by name:
```bash
./build/test_runner <test_name>
```

## Architecture

Two-stage ASM bootloader (`boot/`) hands off to a monolithic C kernel (`kernel/`). All OS services (console, memory, FAT, disk, shell, program loader) live in a single flat binary loaded at 0x10000.

### Dual Compilation Model

The same kernel C source compiles two ways:
- **OS target:** `clang -target i686-elf -ffreestanding` — cross-compiled, no stdlib
- **Test target:** `clang` (host) with `-DWEEZYDOS_TEST` — links against host libc

The `WEEZYDOS_TEST` define gates hardware access so kernel logic is testable without QEMU.

### The Seam Pattern

Hardware modules expose interfaces through function pointer structs (e.g., `disk_ops.read_sector`). Tests swap in fake implementations (RAM-backed disk, buffer-backed console). Logic modules never call hardware directly — they always go through the ops struct. This is the core pattern that makes TDD possible for bare-metal code.

### What to TDD vs. What to QEMU-Test

**Host-testable (must have tests):** memory allocator, FAT16 driver, shell parser, string utilities, program loader logic.

**QEMU-only (keep thin):** bootloader ASM, VGA writes (0xB8000), keyboard ISR (port 0x60), ATA PIO (port I/O), GDT/IDT setup.

## Design Spec

Full design document with memory map, FAT16 layout, command reference, and phased development plan: `docs/superpowers/specs/2026-03-25-weezydos-design.md`
