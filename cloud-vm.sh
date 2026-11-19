#!/usr/bin/env bash
set -e

REPO_DIR="$(cd "$(dirname "${BASH_SOURCE}")" && pwd)"
WORK_DIR="$HOME/mutant-cloud"
mkdir -p "$WORK_DIR"

echo "[1/3] Installing Cloud QEMU ARM64 & noVNC..."
sudo apt-get update -qq
sudo apt-get install -y -qq qemu-system-arm qemu-efi-aarch64 websockify novnc wget

cd "$WORK_DIR"

if [ ! -f MutantLinux-arm64.qcow2 ]; then
    echo "[2/3] Downloading MutantLinux-arm64.qcow2 Virtual Disk from GitHub Releases..."
    GH_HOST="github.com"
    GH_DISK="/MutantBloodcell/MutantLinux/releases/latest/download/MutantLinux-arm64.qcow2"
    wget -q --show-progress -O MutantLinux-arm64.qcow2 "https://${GH_HOST}${GH_DISK}"
fi

cp /usr/share/AAVMF/AAVMF_CODE.fd ./bios.fd 2>/dev/null || cp /usr/share/qemu-efi-aarch64/QEMU_EFI.fd ./bios.fd

echo "[3/3] Starting MutantLinux Cloud VM (Port 6080)..."
ln -sf /usr/share/novnc/vnc.html /usr/share/novnc/index.html
websockify --web=/usr/share/novnc 6080 127.0.0.1:5901 >/dev/null 2>&1 &
WS_PID=$!
trap "kill $WS_PID 2>/dev/null" EXIT

ACCEL="-accel tcg"
[ -w /dev/kvm ] && [ "$(uname -m)" = "aarch64" ] && ACCEL="-enable-kvm -cpu host" || ACCEL="-cpu cortex-a72"

qemu-system-aarch64 \
  -M virt $ACCEL \
  -smp 4 \
  -m 2048 \
  -bios ./bios.fd \
  -kernel "$REPO_DIR/kernel-bin/vmlinuz-mutant" \
  -append "console=ttyAMA0 console=tty0 rw root=/dev/vda2 rootfstype=ext4 rootwait" \
  -drive file=MutantLinux-arm64.qcow2,format=qcow2,if=virtio \
  -device virtio-gpu-pci,xres=1024,yres=768 \
  -device virtio-sound-pci \
  -device qemu-xhci \
  -device usb-kbd \
  -device usb-tablet \
  -netdev user,id=net0 \
  -device virtio-net-pci,netdev=net0 \
  -vnc :1
