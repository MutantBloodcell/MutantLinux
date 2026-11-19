#!/data/data/com.termux/files/usr/bin/bash
set -e

MUTANT_DIR="$HOME/MutantLinux"
BIOS="$PREFIX/share/qemu/edk2-aarch64-code.fd"
KERNEL="$MUTANT_DIR/kernel-bin/vmlinuz-mutant"
DISK="$MUTANT_DIR/build/rootfs.ext4"
RAM_FILE="$TMPDIR/mutant-vm.ram"

mkdir -p "$MUTANT_DIR/build"
ln -sf /init "$MUTANT_DIR/rootfs/sbin/init"

if [ ! -f "$DISK" ]; then
    echo "Creating rootfs.ext4 virtual disk..."
    ROOTFS_MB=$(du -sm "$MUTANT_DIR/rootfs" | awk '{print $1}'); truncate -s "$((ROOTFS_MB + 1024))M" "$DISK"
    mkfs.ext4 -F -d "$MUTANT_DIR/rootfs" "$DISK" >/dev/null
fi

TOTAL_KB=$(awk '/MemTotal:/ {print $2}' /proc/meminfo 2>/dev/null || echo 4096000)
VM_RAM=$((TOTAL_KB / 2048))
[ "$VM_RAM" -lt 1024 ] && VM_RAM=1024
[ "$VM_RAM" -gt 8192 ] && VM_RAM=8192

HOST_CORES=$(nproc 2>/dev/null || echo 2)
[ "$HOST_CORES" -gt 4 ] && HOST_CORES=4

trap "rm -f $RAM_FILE" EXIT

if [ -w /dev/kvm ]; then
    ACCEL_FLAGS="-enable-kvm -cpu host -smp $HOST_CORES"
else
    ACCEL_FLAGS="-accel tcg,thread=multi,tb-size=256 -cpu cortex-a72 -smp $HOST_CORES"
fi

echo "Booting MutantLinux (${VM_RAM}MB RAM, ${HOST_CORES} vCPUs)..."

qemu-system-aarch64 \
  -M virt,gic-version=3 \
  $ACCEL_FLAGS \
  -m "$VM_RAM" \
  -object memory-backend-file,id=mem,size="${VM_RAM}M",mem-path="$RAM_FILE",share=on,prealloc=off \
  -numa node,memdev=mem \
  -bios "$BIOS" \
  -kernel "$KERNEL" \
  -append "console=ttyAMA0 console=tty0 rw root=/dev/vda rootfstype=ext4 rootwait mitigations=off" \
  -drive file="$DISK",format=raw,if=none,id=hd0,cache=writeback,aio=threads \
  -device virtio-blk-pci,drive=hd0 \
  -device virtio-gpu-pci,xres=800,yres=600 \
  -device virtio-sound-pci \
  -device qemu-xhci \
  -device usb-kbd \
  -device usb-tablet \
  -netdev user,id=net0 \
  -device virtio-net-pci,netdev=net0 \
  -vnc :1,lossy=on \
  -serial mon:stdio
