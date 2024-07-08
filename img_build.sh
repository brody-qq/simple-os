#!/bin/bash

. common.sh

set -e

exit_if_not_root

FLAGS='-O3'
if [[ "$1" = '--debug' ]]; then
   FLAGS='-g -O0'
fi

mkdir -p "$BUILD_DIR"
rm -rf "$BUILD_DIR"/*

cd "$BASE_DIR"

# these are taken straight from https://wiki.osdev.org/Creating_a_64-bit_kernel#Compiling
# TODO remove -mno-sse -mno-mmx -mnosse2 ? all x64 processors have at least sse2, sse3 is after 2004 & AVX is after 2011, AVX2 is after 2013 (just make sure there are no sse/avx instructions before setting the fpu/see state)
# TODO is -mcmodel=large necessary here? kernel image will be smaller than 2GB and loaded into the first few MB of memory, -mcmodel=small should be more efficient ( https://eli.thegreenplace.net/2012/01/03/understanding-the-x64-code-models )
# https://stackoverflow.com/a/56998513
# NOTE command "./$TOOLCHAIN_BINS/x86_64-elf-g++ -Q --help=target" will show which see/avx options are enabled by default
OSDEV_X86_64_ARGS="-ffreestanding -mno-red-zone -mno-mmx -mno-sse -mno-sse2"

# TODO move boot code to boot/ directory
"$TOOLCHAIN_BINS/x86_64-elf-as" "$KERNEL_SRC_DIR/boot.S" \
                                -o "$BUILD_DIR/boot.o" \
                                $FLAGS

"$TOOLCHAIN_BINS/x86_64-elf-g++" -c "$INCLUDE_SRC_DIR/syscall.cpp" \
                                 -o "$BUILD_DIR/syscall.o" \
                                 $OSDEV_X86_64_ARGS \
                                 -Wall \
                                 -Wextra \
                                 -Werror \
                                 -fno-exceptions \
                                 -fno-rtti \
                                 -I"$BASE_DIR" \
                                 -std=c++20 \
                                 $FLAGS

"$TOOLCHAIN_BINS/x86_64-elf-g++" -c "$INCLUDE_SRC_DIR/key_event.cpp" \
                                 -o "$BUILD_DIR/key_event.o" \
                                 $OSDEV_X86_64_ARGS \
                                 -Wall \
                                 -Wextra \
                                 -Werror \
                                 -fno-exceptions \
                                 -fno-rtti \
                                 -I"$BASE_DIR" \
                                 -std=c++20 \
                                 -fPIE \
                                 $FLAGS

"$TOOLCHAIN_BINS/x86_64-elf-g++" -c "$INCLUDE_SRC_DIR/math.cpp" \
                                 -o "$BUILD_DIR/math.o" \
                                 $OSDEV_X86_64_ARGS \
                                 -Wall \
                                 -Wextra \
                                 -Werror \
                                 -fno-exceptions \
                                 -fno-rtti \
                                 -I"$BASE_DIR" \
                                 -std=c++20 \
                                 -fPIE \
                                 $FLAGS

"$TOOLCHAIN_BINS/x86_64-elf-g++" -c "$INCLUDE_SRC_DIR/stdlib_workaround.cpp" \
                                 -o "$BUILD_DIR/stdlib_workaround.o" \
                                 $OSDEV_X86_64_ARGS \
                                 -Wall \
                                 -Wextra \
                                 -Werror \
                                 -fno-exceptions \
                                 -fno-rtti \
                                 -I"$BASE_DIR" \
                                 -std=c++20 \
                                 -fPIE \
                                 $FLAGS


# ---------------------------------------------------------------
echo DEBUG 1
"$TOOLCHAIN_BINS/x86_64-elf-g++" -c "$KERNEL_SRC_DIR/kernel.cpp" \
                                 -o "$BUILD_DIR/kernel.o" \
                                 $OSDEV_X86_64_ARGS \
                                 -Wall \
                                 -Wextra \
                                 -Werror \
                                 -fno-exceptions \
                                 -fno-rtti \
                                 -I"$BASE_DIR" \
                                 -std=c++20 \
                                 $FLAGS

echo DEBUG 2
"$TOOLCHAIN_BINS/x86_64-elf-g++" -T "$KERNEL_SRC_DIR/linker.ld" \
                                 -o "$BUILD_DIR/boot.elf" \
                                 $OSDEV_X86_64_ARGS \
                                 -nostdlib \
                                 "$BUILD_DIR/boot.o" \
                                 "$BUILD_DIR/kernel.o" \
                                 "$BUILD_DIR/syscall.o" "$BUILD_DIR/key_event.o" "$BUILD_DIR/math.o" "$BUILD_DIR/stdlib_workaround.o" \
                                 -lgcc \
                                 -no-pie \
                                 -fno-pic \
                                 $FLAGS

if ! grub-file --is-x86-multiboot "$BUILD_DIR/boot.elf"; then
    echo "$0: error: the multiboot header of boot.elf was invalid"
    exit 1
fi

# ---------------------------------------------------------------

mkdir -p "$BUILD_DIR/userspace/"

USERSPACE_BUILD ()
{
    local FILE="$1"
    local PROG_NAME="${FILE%%.cpp}"
    echo DEBUG $PROG_NAME
    "$TOOLCHAIN_BINS/x86_64-elf-g++" "$USERSPACE_DIR/$FILE" \
                                    -o "$BUILD_DIR/userspace/$PROG_NAME" \
                                    $OSDEV_X86_64_ARGS \
                                    -Wall \
                                    -Wextra \
                                    -Werror \
                                    -fno-exceptions \
                                    -fno-rtti \
                                    -I"$BASE_DIR" \
                                    -I"$USERSPACE_DIR" \
                                    -std=c++20 \
                                    -lgcc \
                                    -pie \
                                    -fpie \
                                    -nostdlib \
                                    -nodefaultlibs \
                                    -emain \
                                    -Wno-unused-parameter \
                                    $FLAGS \
                                    "$BUILD_DIR/syscall.o" "$BUILD_DIR/key_event.o" "$BUILD_DIR/math.o" "$BUILD_DIR/stdlib_workaround.o" \
                                    #-static \ # this overrides -pie and -fpie :(
}

USERSPACE_PROGRAMS=( cat.cpp cp.cpp ls.cpp mkdir.cpp mv.cpp pwd.cpp rm.cpp rmdir.cpp sh.cpp stdentry.cpp test.cpp touch.cpp write.cpp )

for F in "${USERSPACE_PROGRAMS[@]}"; do
    USERSPACE_BUILD "$F"
done

# ---------------------------------------------------------------

bash fs_build.sh

