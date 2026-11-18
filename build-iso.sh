#!/data/data/com.termux/files/usr/bin/bash
set -e

MUTANT_DIR="$HOME/MutantLinux"
mkdir -p "$MUTANT_DIR/iso/boot"

START_TIME=$(date +%s)

echo "[1/3] Packing initramfs.img (Multi-core parallel compression)..."
cd "$MUTANT_DIR/rootfs"
if command -v pigz >/dev/null 2>&1; then
    find . -print0 | cpio --null -o --format=newc --owner=0:0 2>/dev/null | pigz -3 > "$MUTANT_DIR/iso/boot/initramfs.img"
else
    find . -print0 | cpio --null -o --format=newc --owner=0:0 2>/dev/null | gzip -1 > "$MUTANT_DIR/iso/boot/initramfs.img"
fi

# Only copy kernel if updated
if [ "$MUTANT_DIR/build/vmlinuz-mutant" -nt "$MUTANT_DIR/iso/boot/vmlinuz-mutant" ]; then
    cp -f "$MUTANT_DIR/build/vmlinuz-mutant" "$MUTANT_DIR/iso/boot/"
fi

echo "[2/3] Checking cached UEFI boot image..."
if [ ! -f "$MUTANT_DIR/iso/efiboot.img" ] || [ "$MUTANT_DIR/build/BOOTAA64.EFI" -nt "$MUTANT_DIR/iso/efiboot.img" ]; then
    rm -f "$MUTANT_DIR/iso/efiboot.img"
    dd if=/dev/zero of="$MUTANT_DIR/iso/efiboot.img" bs=1M count=96 status=none
    mkfs.vfat "$MUTANT_DIR/iso/efiboot.img" >/dev/null
    mmd -i "$MUTANT_DIR/iso/efiboot.img" ::/EFI ::/EFI/BOOT
    mcopy -i "$MUTANT_DIR/iso/efiboot.img" "$MUTANT_DIR/build/BOOTAA64.EFI" ::/EFI/BOOT/
fi

echo "[3/3] Generating MutantLinux-arm64.iso..."
rm -f "$MUTANT_DIR/MutantLinux-arm64.iso"
xorriso -as mkisofs \
  -o "$MUTANT_DIR/MutantLinux-arm64.iso" \
  -V "MUTANTLINUX" \
  -e efiboot.img \
  -no-emul-boot \
  -append_partition 2 0xef "$MUTANT_DIR/iso/efiboot.img" \
  "$MUTANT_DIR/iso/" >/dev/null 2>&1

END_TIME=$(date +%s)
echo "Done in $((END_TIME - START_TIME))s! ISO ready: $MUTANT_DIR/MutantLinux-arm64.iso"
