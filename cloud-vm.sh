#!/usr/bin/env bash
set -e

REPO_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
WORK_DIR="$HOME/mutant-cloud"
mkdir -p "$WORK_DIR"

echo "[1/3] Installing Cloud QEMU ARM64, ISO tools & noVNC..."
sudo apt-get update -qq
sudo apt-get install -y -qq qemu-system-arm qemu-efi-aarch64 websockify novnc wget cpio pigz xorriso dosfstools mtools

cd "$WORK_DIR"

if [ ! -f MutantLinux-arm64.iso ]; then
    echo "[2/3] Fetching MutantLinux-arm64.iso from GitHub Releases..."
    GH_HOST="github.com"
    GH_PATH="/MutantBloodcell/MutantLinux/releases/latest/download/MutantLinux-arm64.iso"
    if ! wget -q --show-progress -O MutantLinux-arm64.iso "https://${GH_HOST}${GH_PATH}"; then
        rm -f MutantLinux-arm64.iso
        echo "Release asset not found yet—building ISO directly in the cloud..."
        mkdir -p iso/boot
        (cd "$REPO_DIR/rootfs" && find . -print0 | cpio --null -o --format=newc --owner=0:0 2>/dev/null | pigz -6 > "$WORK_DIR/iso/boot/initramfs.img")
        cp "$REPO_DIR/kernel-bin/vmlinuz-mutant" iso/boot/
        dd if=/dev/zero of=iso/efiboot.img bs=1M count=96 status=none
        mkfs.vfat iso/efiboot.img >/dev/null
        mmd -i iso/efiboot.img ::/EFI ::/EFI/BOOT
        mcopy -i iso/efiboot.img "$REPO_DIR/kernel-bin/BOOTAA64.EFI" ::/EFI/BOOT/
        xorriso -as mkisofs \
          -o MutantLinux-arm64.iso \
          -V "MUTANTLINUX" \
          -e efiboot.img \
          -no-emul-boot \
          -append_partition 2 0xef iso/efiboot.img \
          iso/ >/dev/null 2>&1
    fi
fi

cp /usr/share/AAVMF/AAVMF_CODE.fd ./bios.fd 2>/dev/null || cp /usr/share/qemu-efi-aarch64/QEMU_EFI.fd ./bios.fd

echo "=========================================================="
echo "  MUTANTLINUX CLOUD VM ONLINE!"
echo "  Open Port 6080 in your browser (/vnc.html)"
echo "=========================================================="

websockify --web=/usr/share/novnc 6080 127.0.0.1:5901 >/dev/null 2>&1 &
WS_PID=$!
trap "kill $WS_PID 2>/dev/null" EXIT

ACCEL="-accel tcg"
[ -w /dev/kvm ] && [ "$(uname -m)" = "aarch64" ] && ACCEL="-enable-kvm -cpu host" || ACCEL="-cpu cortex-a72"

qemu-system-aarch64 \
  -M virt $ACCEL \
  -smp 4 \
  -m 4096 \
  -bios ./bios.fd \
  -cdrom MutantLinux-arm64.iso \
  -device virtio-gpu-pci \
  -device virtio-sound-pci \
  -device qemu-xhci \
  -device usb-kbd \
  -device usb-tablet \
  -vnc :1
