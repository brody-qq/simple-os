
. common.sh

set -eu

exit_if_not_root

DEV=$(losetup --partscan --find "$DISK_IMAGE" --show)
mount "${DEV}p1" "$FS_MOUNT"

echo "$DEV" > "$DEV_STORE_FILE"

