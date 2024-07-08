
. common.sh

set -eu

exit_if_not_root

DEV="$(cat "$DEV_STORE_FILE")"

umount "$FS_MOUNT"
losetup -d "$DEV"

rm "$DEV_STORE_FILE"

