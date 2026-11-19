FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update -qq && \
    apt-get install -y -qq qemu-system-arm qemu-efi-aarch64 websockify novnc e2fsprogs && \
    rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY rootfs /app/rootfs
COPY kernel-bin /app/kernel-bin
COPY start-space.sh /app/start-space.sh

# Ensure /sbin/init points to /init and pack rootfs into a disk image (zero RAM unpacking)
RUN ln -sf /init /app/rootfs/sbin/init && \
    chmod +x /app/start-space.sh && \
    dd if=/dev/zero of=/app/rootfs.ext4 bs=1M count=768 status=none && \
    mkfs.ext4 -F -d /app/rootfs /app/rootfs.ext4 >/dev/null && \
    rm -rf /app/rootfs

EXPOSE 10000
CMD ["/app/start-space.sh"]
