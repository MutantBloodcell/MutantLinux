#!/data/data/com.termux/files/usr/bin/bash

MUTANT_DIR="$HOME/MutantLinux"
BIOS="$PREFIX/share/qemu/edk2-aarch64-code.fd"
KERNEL="$MUTANT_DIR/build/vmlinuz-mutant"
INITRD="$MUTANT_DIR/iso/boot/initramfs.img"
ISO="$MUTANT_DIR/MutantLinux-arm64.iso"
NOVNC_DIR="$MUTANT_DIR/novnc"
RAM_FILE="$TMPDIR/mutant-vm.ram"

if [ ! -d "$NOVNC_DIR" ]; then
    echo "noVNC not found at $NOVNC_DIR!"
    exit 1
fi

# Start lightweight websockify (<18 MB RAM)
PYTHONOPTIMIZE=2 websockify --web="$NOVNC_DIR" --heartbeat=30 6080 127.0.0.1:5901 >/dev/null 2>&1 &
WS_PID=$!

# Clean up websockify and disk-backed RAM file on exit
cleanup() {
    kill "$WS_PID" 2>/dev/null
    rm -f "$RAM_FILE"
}
trap cleanup EXIT

echo "=========================================================="
echo "  MUTANTLINUX BROWSER VM READY (DISK-BACKED RAM)"
echo "  Open in Browser: http://127.0.0"
echo "  Phone physical RAM protected via file-backed paging!"
echo "=========================================================="

qemu-system-aarch64 \
  -M virt \
  -accel tcg,tb-size=64 \
  -cpu cortex-a72 \
  -smp 4 \
  -m 3072 \
  -object memory-backend-file,id=mem,size=3072M,mem-path="$RAM_FILE",share=on,prealloc=off \
  -numa node,memdev=mem \
  -bios "$BIOS" \
  -kernel "$KERNEL" \
  -initrd "$INITRD" \
  -append "console=ttyAMA0 console=tty0 rw root=/dev/ram0 rootfstype=ramfs usbhid.kbpoll=250" \
  -cdrom "$ISO" \
  -device virtio-gpu-pci \
  -device virtio-sound-pci \
  -device qemu-xhci \
  -device usb-kbd \
  -device usb-tablet \
  -vnc :1,lossy=on \
  -serial mon:stdio
