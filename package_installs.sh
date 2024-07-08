#!/bin/bash

set -u

if [[ $(id -u) != 0 ]]; then
    echo "$0: must be root user to run this program"
    exit 1
fi

# general tools
TOOLS="vim qemu-system tmux curl"

# required to build binutils
BINUTILS_BUILD="bison flex libgmp3-dev libmpc-dev libmpfr-dev texinfo"

# required to get grub-mkrescue to work
GRUB_MKRESCUE="xorriso mtools"

apt install $TOOLS $BINUTILS_BUILD $GRUB_MKRESCUE

