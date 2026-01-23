CC = clang
CFLAGS = -target x86_64-unknown-windows -ffreestanding -fno-stack-protector -fno-stack-check -fshort-wchar -mno-red-zone
LDFLAGS = -target x86_64-unknown-windows -fuse-ld=lld -nostdlib -Wl,-entry:EfiMain -Wl,-subsystem:efi_application

all: main.efi

main.efi: main.c efi.h memory.c memory.h graphics.c graphics.h font.c font.h gdt.c gdt.h interrupt.c interrupt.h
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ main.c memory.c graphics.c font.c gdt.c interrupt.c

clean:
	rm -f main.efi
