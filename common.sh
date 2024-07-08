#!/bin/bash

BASE_DIR="$(git rev-parse --show-toplevel)/"
# NOTE this is here to get around the fact that the zip file won't have a git repo present...
if [[ "$?" != "0" ]]; then
    echo 'git repo not present, using current directory'
    BASE_DIR="$(pwd)/"
    if [[ ! -f "$BASE_DIR/ensure-toolchain.sh" ]]; then
        echo 'error: current directory must be the base directory of the project'
        exit 1
    fi
fi

TOOLCHAIN_DIR="$BASE_DIR/toolchain/"
TOOLCHAIN_BINS="$TOOLCHAIN_DIR/build/bin/"
BUILD_DIR="$BASE_DIR/build/"
KERNEL_SRC_DIR="$BASE_DIR/kernel/"
INCLUDE_SRC_DIR="$BASE_DIR/include/"

ISO_DIR="$BUILD_DIR/isodir/"
ISO_FILE="$ISO_DIR/os.iso"

LOGS_DIR="$BASE_DIR/logs/"

FS_MOUNT="$BUILD_DIR/fs_mnt/"
DISK_IMAGE="$BUILD_DIR/disk.img"

DEV_STORE_FILE="$BUILD_DIR/_dev.img"

USERSPACE_DIR="$BASE_DIR/userspace/"

function exit_if_not_root () {
    if [[ ! "$(id -u)" = '0' ]]; then
        echo 'error: must be root'
        exit 1
    fi
}

