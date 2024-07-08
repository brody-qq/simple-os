#!/bin/bash

. common.sh

set -eu

exit_if_not_root

rm -f "$DISK_IMAGE"
dd if=/dev/zero of="$DISK_IMAGE" bs=128M count=1
chown 0:0 "$DISK_IMAGE"

DEV=$(losetup --partscan --find "$DISK_IMAGE" --show)

parted -s "$DEV" \
    mklabel msdos \
    mkpart primary ext2 1MiB 100% \
    -a minimal \
    set 1 boot on

mke2fs "${DEV}p1"

mkdir -p "$FS_MOUNT"
mount "${DEV}p1" "$FS_MOUNT"

#rsync -aH --inplace --update "$BUILD_DIR/boot.elf" "$FS_MOUNT"
mkdir -p "$FS_MOUNT/boot/"
mkdir -p "$FS_MOUNT/boot/grub/"
mkdir -p "$FS_MOUNT/emptydir/" #for testing purposes
mkdir -p "$FS_MOUNT/userspace/"
cp "$BUILD_DIR/boot.elf" "$FS_MOUNT/boot/"
cp "$KERNEL_SRC_DIR/grub.cfg" "$FS_MOUNT/boot/grub/"
cp -r "$BUILD_DIR/userspace/" "$FS_MOUNT/"
chown -R 0:0 "$FS_MOUNT"
# chmod -R g+rX,o+rX /Base/* mnt/

grub-install --boot-directory="$FS_MOUNT"/boot --target=i386-pc --modules="ext2 part_msdos" "$DEV"

umount "$FS_MOUNT"
losetup -d "$DEV"

