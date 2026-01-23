#!/bin/bash
set -e

# Compile
# Actually, the user's run.sh might have a hardcoded clang command or just call make.
# Looking at the previous run.sh view, line 5 is 'make'.
# But the log showed a long clang command.
# Let me just ensure Makefile is enough.
make

# Create directory structure
mkdir -p boot/EFI/BOOT

# Copy the EFI application
cp main.efi boot/EFI/BOOT/BOOTX64.EFI

# Run in QEMU
# -bios: specify the UEFI firmware
# -drive file=fat:rw:boot: use the 'boot' directory as a virtual FAT drive
# Remove -nographic to see the GOP output (requires X11/Wayland)
qemu-system-x86_64 -bios /usr/share/ovmf/OVMF.fd -drive file=fat:rw:boot,format=raw -vga std
