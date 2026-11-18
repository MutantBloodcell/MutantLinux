#!/data/data/com.termux/files/usr/bin/bash

MUTANT_DIR="$HOME/MutantLinux"
BIOS="$PREFIX/share/qemu/edk2-aarch64-code.fd"
KERNEL="$MUTANT_DIR/build/vmlinuz-mutant"
INITRD="$MUTANT_DIR/iso/boot/initramfs.img"
ISO="$MUTANT_DIR/MutantLinux-arm64.iso"
RAM_FILE="$TMPDIR/mutant-vm.ram"

trap "rm -f $RAM_FILE" EXIT

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
  -vnc :1 \
  -serial mon:stdio
