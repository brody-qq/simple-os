#!/bin/bash

# based on this https://wiki.osdev.org/GCC_Cross-Compiler

. common.sh

set -e
set -x

check_md5 () {
    local FILE="$1"
    local EXPECTED_MD5="$2"
    local DOWNLOADED_MD5=$(md5sum "$FILE" | cut -f1 -d' ')
    if [[ "$DOWNLOADED_MD5" != "$EXPECTED_MD5" ]]; then
        echo "ERROR: md5sum for $FILE did not match the expected md5sum"
        exit 1
    fi
}

mkdir -p "$TOOLCHAIN_DIR/src/" "$TOOLCHAIN_DIR/build/"

DEST_DIR="$TOOLCHAIN_DIR/src/"

# NOTE this uses -f flag since it returns error if DEST_DIR is empty
rm -rf "$DEST_DIR"/* 

BINUTILS=binutils-2.41.tar.xz
DEST="$DEST_DIR/$BINUTILS"

curl "https://ftp.gnu.org/gnu/binutils/$BINUTILS" -o "$DEST"
check_md5 "$DEST" "256d7e0ad998e423030c84483a7c1e30"
tar -xf "$DEST" --directory "$DEST_DIR"

GCC_NAME="gcc-13.2.0"
GCC="${GCC_NAME}.tar.xz"
DEST="$DEST_DIR/$GCC"

curl "https://ftp.gnu.org/gnu/gcc/$GCC_NAME/$GCC" -o "$DEST"
check_md5 "$DEST" "e0e48554cc6e4f261d55ddee9ab69075"
tar -xf "$DEST" --directory "$DEST_DIR"


# TODO change this to x86-64, and remember to add -mno-red-zone to gcc compiles (https://wiki.osdev.org/Libgcc_without_red_zone)
export PREFIX="$TOOLCHAIN_DIR/build/"
export TARGET=x86_64-elf
export PATH="$PREFIX/bin:$PATH"

JOBS_ARG="-j4"

cd "$TOOLCHAIN_DIR/src/"
 
mkdir build-binutils
cd build-binutils
../binutils-2.41/configure --target="$TARGET" --prefix="$PREFIX" --with-sysroot --disable-nls --disable-werror
make "$JOBS_ARG"
make "$JOBS_ARG" install

# TODO if gdb doesn't work try download gdb source and run these lines
#../gdb.13.2/configure --target="$TARGET" --prefix="$PREFIX" --disable-werror
#make all-gdb
#make install-gdb

cd "$TOOLCHAIN_DIR/src/"
 
which -- "$TARGET-as" || echo "$TARGET-as" is not in the PATH
 
mkdir build-gcc
cd build-gcc
../gcc-13.2.0/configure --target="$TARGET" --prefix="$PREFIX" --disable-nls --enable-languages=c,c++ --without-headers
make "$JOBS_ARG" all-gcc
make "$JOBS_ARG" all-target-libgcc
make "$JOBS_ARG" install-gcc
make "$JOBS_ARG" install-target-libgcc

