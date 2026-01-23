# CoolOS

CoolOS is a toy operating system written from scratch, focusing on **UEFI boot** and **x86_64 architecture**. It serves as a learning project to understand low-level system programming, memory management, and hardware interfacing.

## 🚀 Features

- **UEFI Boot**: Bootstrapped via UEFI `EfiMain` using `clang` and `lld`.
- **Graphics**: Basic VGA text and hex output via UEFI Graphics Output Protocol (GOP).
- **Memory Management**:
  - Bitmap-based physical page allocator.
  - 4-level paging support.
  - Kernel memory protection.
- **Kernel Infrastructure**:
  - GDT (Global Descriptor Table) & IDT (Interrupt Descriptor Table) initialization.
  - ACPI parsing (RSDP, FADT, MADT) to locate system tables.
  - APIC (Advanced Programmable Interrupt Controller) & Timer support.
  - Basic Heap Allocator (`kmalloc`, `kfree`, aligned allocations).
- **Custom Libc**: Minimal freestanding C library implementation (memory operations, etc.).

## 🛠️ Prerequisites

To build and run CoolOS, you need:

- **Compiler**: `clang` with `lld` (cross-compilation support for `x86_64-unknown-windows`).
- **Emulator**: `QEMU` (x86_64).
- **Firmware**: `OVMF` (Open Virtual Machine Firmware) for UEFI support.
- **Build Tools**: `make`, `bash`.

## 📦 Getting Started

### 1. Clone the repository
```bash
git clone https://github.com/jihoo12/coolos.git
cd coolos
```

### 2. Build and Run
The project includes a `run.sh` script that automates the build process, sets up the virtual FAT drive for UEFI, and launches QEMU.

```bash
chmod +x run.sh
./run.sh
```

> [!NOTE]
> Make sure the path to `OVMF.fd` in `run.sh` matches your system's location (default: `/usr/share/ovmf/OVMF.fd`).

## 📂 Project Structure

| File | Description |
| :--- | :--- |
| `main.c` | Kernel entry point and initialization logic. |
| `memory.c/h` | Physical page allocator and page table management. |
| `graphics.c/h` | Framebuffer-based graphics primitives. |
| `font.c/h` | Simple bitmap font for text rendering. |
| `interrupt.c/h` | IDT setup and interrupt service routines. |
| `acpi.c/h` | ACPI table discovery and parsing. |
| `apic.c/h` | Local APIC initialization and management. |
| `heap.c/h` | Kernel heap allocator. |
| `timer.c/h` | LAPIC-based system timer. |
| `libc.c/h` | Minimal freestanding C library functions. |
| `gdt.c/h` | Segment descriptor table setup. |
| `efi.h` | UEFI protocol and structure definitions. |

