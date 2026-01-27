#!/bin/bash
set -e

# 1. 빌드 (Make 실행)
make

# 2. EFI 실행 파일 배치를 위한 디렉토리 생성 및 복사
mkdir -p boot/EFI/BOOT
cp main.efi boot/EFI/BOOT/BOOTX64.EFI

# 3. NVMe 이미지 파일이 없을 경우 생성 (128MB 크기 예시)
# qemu-img 명령어를 사용하여 가상 디스크 파일을 만듭니다.
if [ ! -f nvme.img ]; then
    echo "Creating nvme.img..."
    qemu-img create -f raw nvme.img 128M
fi

# 4. QEMU 실행
# -bios: UEFI 펌웨어 지정
# -drive file=fat:rw:boot: 'boot' 폴더를 가상 FAT 드라이브로 사용
# -drive ... nvme.img: 생성한 NVMe 이미지를 연결
qemu-system-x86_64 \
    -bios /usr/share/ovmf/OVMF.fd \
    -drive file=fat:rw:boot,format=raw \
    -vga std \
    -drive file=nvme.img,if=none,id=mynvme,format=raw \
    -device nvme,serial=coolos_nvme,drive=mynvme