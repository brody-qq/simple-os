#!/bin/bash

. common.sh

exit_if_not_root

DEBUG_ARGS=''
if [[ $1 = '--debug' ]]; then
    DEBUG_ARGS='-s -S'
fi

set -eu

#mkdir -p "$LOGS_DIR"
#TIMESTAMP="$(date +%s)"
#LOG_FILE="$LOGS_DIR/qemu_serial_dbg_$TIMESTAMP.log"

SERIAL_LOG=serial.log
QEMU_LOG=_qemu.log

qemu-system-x86_64 \
    -drive file="$DISK_IMAGE",format=raw,index=0,media=disk \
    -serial file:"$SERIAL_LOG" \
    -d guest_errors,cpu_reset,int \
    -D "$QEMU_LOG" \
    -m 128M \
    $DEBUG_ARGS

# TODO -d, -m, -monitor
# TODO qemu-system-x86_64 -d help
#      qemu-system-x86_64 -d trace:help

