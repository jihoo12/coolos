CC = clang
CFLAGS = -target x86_64-unknown-windows -ffreestanding -fno-stack-protector -fno-stack-check -fshort-wchar -mno-red-zone
LDFLAGS = -target x86_64-unknown-windows -fuse-ld=lld -nostdlib -Wl,-entry:EfiMain -Wl,-subsystem:efi_application

all: main.efi

main.efi: main.c efi.h memory.c memory.h graphics.c graphics.h font.c font.h gdt.c gdt.h interrupt.c interrupt.h heap.c heap.h acpi.c acpi.h libc.c libc.h apic.c apic.h timer.c timer.h ioapic.c ioapic.h keyboard.c keyboard.h schedule.c schedule.h syscall.h syscall.c syscall_entry.S pci.c pci.h nvme.c nvme.h
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ main.c memory.c graphics.c font.c gdt.c interrupt.c heap.c acpi.c libc.c apic.c timer.c ioapic.c keyboard.c schedule.c syscall.c syscall_entry.S pci.c nvme.c

clean:
	rm -f main.efi
