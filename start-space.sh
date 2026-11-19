#!/usr/bin/env bash
set -e

WEB_PORT="${PORT:-10000}"

# Auto-redirect root URL to noVNC with autoconnect and mobile screen scaling
cat << 'HTML' > /usr/share/novnc/index.html
<!DOCTYPE html>
<html>
<head><meta http-equiv="refresh" content="0; url=vnc.html?autoconnect=true&resize=scale"></head>
<body>Loading MutantLinux...</body>
</html>
HTML

# Start websockify on Render's $PORT
websockify --web=/usr/share/novnc "$WEB_PORT" 127.0.0.1:5901 >/dev/null 2>&1 &

cp /usr/share/AAVMF/AAVMF_CODE.fd /app/bios.fd 2>/dev/null || cp /usr/share/qemu-efi-aarch64/QEMU_EFI.fd /app/bios.fd

# Keep QEMU running and boot directly from rootfs.ext4 (/dev/vda) to stay under 512MB RAM
while true; do
    qemu-system-aarch64 \
      -M virt \
      -accel tcg,tb-size=32 \
      -cpu cortex-a72 \
      -smp 1 \
      -m 384 \
      -object memory-backend-file,id=mem,size=384M,mem-path=/var/tmp/vm.ram,share=on,prealloc=off \
      -numa node,memdev=mem \
      -bios /app/bios.fd \
      -kernel /app/kernel-bin/vmlinuz-mutant \
      -append "console=ttyAMA0 console=tty0 rw root=/dev/vda rootfstype=ext4 usbhid.kbpoll=250" \
      -drive file=/app/rootfs.ext4,format=raw,if=virtio \
      -device virtio-gpu-pci \
      -device qemu-xhci \
      -device usb-kbd \
      -device usb-tablet \
      -vnc :1,lossy=on \
      -serial stdio || true
    sleep 2
done
