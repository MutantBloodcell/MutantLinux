---
title: MutantLinux 1.0 ARM64
emoji: 🧬
colorFrom: green
colorTo: blue
sdk: docker
app_port: 7860
pinned: true
---

# MutantLinux 1.0 ARM64
Independent ARM64 operating system.

## System Requirements

| Component | Minimum | Recommended |
| :--- | :--- | :--- |
| **Architecture** | ARM64 (`aarch64` / ARMv8-A) or `qemu-system-aarch64` on x86_64/ARM64 hosts | ARM64 hardware with KVM or multi-core host CPU |
| **Processor** | 1 vCPU (`cortex-a72` compatible) | 2+ CPU cores |
| **Memory (RAM)** | **512 MB** (CLI Mode) / **1 GB** (MutantDE GUI; uses ~70 MB idle) | **2 GB** |
| **Storage / Disk** | **4 GB** virtual disk (`.qcow2` / `.ext4`) or drive | **4 GB+** SSD / UFS storage |
| **Boot Firmware** | UEFI ARM64 (`BOOTAA64.EFI` / EDK2 / AAVMF) | UEFI ARM64 with VirtIO block & GPU |
| **Display** | Framebuffer (`/dev/fb0`) or `virtio-gpu-pci` (`800x600`) | `virtio-gpu-pci` (`1024x768`+) |
| **Network** | Optional (required for `mpkg` store & AI self-healing) | VirtIO Ethernet (`virtio-net-pci`) or Wi-Fi |

## Bare-Metal Installation (ARM64 UEFI Devices)

### Method 1: Flash ISO to USB Drive / SD Card
1. Download `MutantLinux-arm64.iso` from **GitHub Releases**.
2. Flash the ISO directly to a USB drive or SD card (replace `/dev/sdX` with your target device):
   ```bash
   sudo dd if=MutantLinux-arm64.iso of=/dev/sdX bs=4M status=progress conv=fsync
   ```
3. Insert the drive into your ARM64 device, enter UEFI Boot Menu, and select the USB/SD UEFI entry (`EFI/BOOT/BOOTAA64.EFI`).

### Method 2: Direct Disk Deployment (`.qcow2` to NVMe / eMMC / SSD)
1. Download `MutantLinux-arm64.qcow2` from **GitHub Releases**.
2. Convert and write the root filesystem directly onto your target partition (e.g., `/dev/nvme0n1p2`):
   ```bash
   qemu-img convert -f qcow2 -O raw MutantLinux-arm64.qcow2 rootfs.raw
   sudo dd if=rootfs.raw of=/dev/nvme0n1p2 bs=4M status=progress conv=fsync
   ```
3. Copy `vmlinuz-mutant` and `BOOTAA64.EFI` onto your EFI System Partition (`/EFI/BOOT/BOOTAA64.EFI`) and pass `root=/dev/nvme0n1p2 rootfstype=ext4 rw` in your bootloader entry.

## Virtual Machine Quick Start
* **Local QEMU VM (RealVNC):** `bash test-vm.sh`
* **Local Browser VM (noVNC):** `bash web-vm.sh`
* **Cloud VM (Codespaces):** `bash cloud-vm.sh`
