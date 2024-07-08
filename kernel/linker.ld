
/* code is from https://wiki.osdev.org/Bare_Bones */ /* TODO remove this comment when it is no longer relevant */

ENTRY(_start)

SECTIONS
{
    . = 2M;
    kernel_image_start = .;

    .text BLOCK(4K) : ALIGN(4K)
    {
        /* NOTE multiboot header must be withint first 8KB of kernel image at 4 byte alignment */
        KEEP(*(.multiboot))
        *(.text*)
    }

    .rodata BLOCK(4K) : ALIGN(4K)
    {
        *(.rodata*)
    }

    .data BLOCK(4K) : ALIGN(4K)
    {
        *(.data*)
    }

    .bss BLOCK(4K) : ALIGN(4K)
    {
        *(COMMON)
        *(.bss)
        *(.stack)
        *(.page_tables)
    }

    kernel_image_end = .;
}

