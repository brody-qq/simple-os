/* this is heavily based off code from https://wiki.osdev.org/Bare_Bones */ /* TODO remove this comment when I eventually update boot.S */

/* constants for multiboot header */
.set PAGE_ALIGN,    0x1
.set MEMINFO,       0x2
.set FLAGS,         PAGE_ALIGN | MEMINFO
.set MAGIC,         0x1BADB002
.set CHECKSUM,      -(MAGIC + FLAGS)

.set KB,    1024
.set MB,    KB * 1024
.set GB,    MB * 1024
.set TB,    GB * 1024

/* multiboot header */
/* NOTE: this MUST be early in the kernel image, otherwise GRUB won't find it */
.section .multiboot
.align 4
.long MAGIC
.long FLAGS
.long CHECKSUM

/* from serenityOS */
/* for MULTIBOOT_MEMORY_INFO */
.long 0x00000000    /* header_addr */
.long 0x00000000    /* load_addr */
.long 0x00000000    /* load_end_addr */
.long 0x00000000    /* bss_end_addr */
.long 0x00000000    /* entry_addr */

/* for MULTIBOOT_VIDEO_MODE */
.long 0x00000000    /* mode_type */
.long 1280          /* width */
.long 1024          /* height */
.long 32            /* depth */


.code32

/* NOTE: stack must be 16 byte aligned for floating point instructions */
.section .stack, "aw", @nobits
.align 4096 
stack_bottom:
    .skip 32 * KB
stack_top:

/* TODO make a '.global NAME\nNAME:' a macro? this has bitten me multiple times already ... */
/*

memory map:

physical address space:
    - physical address space is identity mapped, so that kernel image doesn't need to 
      be manually loaded and have relocations applied to it
    - physical address space size is set based off of the memory map provided by GRUB 
      multiboot header, or to a max of 32*GB
      
    - intially (during boot.S) a small chunk of memory is identity mapped (enough to
      hold the kernel image and some other structures allocated early in kernel_main)
    - after boot.S, the page tables to identity map the entire available physical
      address space are setup

virtual memory:
    - all addresses over the max physical address space size (32GB) are NOT identity
      mapped
    - this region (above 32GB) is used to dynamically allocate memory for the kernel
 
x64 4-level paing allows for 48-bit addresses, so the max virtual address is 2**48 -1

boot page tables:
    # covers vrange [0, max_kernel_size)
    boot_pml4t:
        boot_pdpt:
            boot_pd0:
                boot_pts: (max_kernel_size >> (12+9)) page tables

pmap (added in kernel_main since this will not fit into a 32-bit kernel image):
    # covers vrange [0, kernel_pspace_size)
    kernel_pml4t:
        g_pmap_pdpts:
            g_pmap_pds: (enough 2MB pages to identity map kernel_space_size)

modifying page tables later is done via the pmap region
*/


/* ---------- boot identidy map tables ---------- */
.section .page_tables, "aw", @nobits
.align 4*KB

.global max_kernel_size
.equ max_kernel_size, 64*MB

.global max_kernel_pt_count
.equ max_kernel_pt_count, (max_kernel_size >> (12+9))

.global max_kernel_page_count
.equ max_kernel_page_count, (max_kernel_pt_count * 512)

.global boot_pml4t
boot_pml4t:
    .skip 4*KB

.global boot_pdpt
boot_pdpt:
    .skip 4*KB

.global boot_pd0
boot_pd0:
    .skip 4*KB

.global boot_pts
boot_pd0_pts:
    .skip 4*KB * max_kernel_pt_count

/* ------------------------------------------------- */

/* TODO verify that max_kernel_size will fit in a single page directory (512 * 512 * 4KB) */

# TODO start next time by implementing C code to:
#   - load kernel ELF image (is this necessary w/ current setup since pre-kernel & kernel are same program image???)
#   - setup physical maps (pmap) the kernel image space for which is located below
#   - enter kernel

.section .text
.global _start
.type _start, @function

.global reload_cr3
.type reload_cr3, @function

.extern kernel_main
.type kernel_main, @function

.extern multiboot_info
.type multiboot_info, @object

/*
NOTE:
    - first segment selector of GDT (index 0) is unused and must be null
    - for long mode (and flat addressing 32-bit) we only need 1 code and 1 data segment
    - the GDT selector is used for lgdt and it's size is the offset of the last address, not one-past the last address
*/
.global gdt64
gdt64:
    .quad 0
.global gdt64_code_seg
gdt64_code_seg:
    .quad (1 << 43) | (1 << 44) | (1 << 47) | (1 << 53) /* executable, code segment, present, 64-bit */

# TODO find out the actual name for this type of selector from intel manual...
.global gdt64_selector
gdt64_selector:
    .short . - gdt64 - 1    /* final address of gdt */
    .quad gdt64

.set gdt64_code_seg_ind_val, gdt64_code_seg - gdt64
.global gdt64_code_seg_index
gdt64_code_seg_index:
    .short gdt64_code_seg_ind_val


_start:
    jmp real_start

.set VGA_BUFFER,    0xb8000
test_str:
    .asciz "This is a test string for the prekernel"

/* TODO serenityOS has _start jump immediately to real_start which is marked
        as .code32, not sure if there is a reason for this or not (long jump 
        to switch segments?), try see if marking _start as .code32 has the same 
        effect so the jmp is not necessary */
# TODO
# - make boot page tables
# - enable paging
# - make boot GDT
# - enable long mode
# - make boot debug mechanism (BIOS print functions)

# NOTE the following code borrows heavily from SerenityOS Kernel/Prekernel/boot.S. There are only
#      so many ways to implement OS booting, and if I was to 're-invent the wheel' for OS booting
#      it would have involved dozens of hours of pouring through intel documentation on CPU startup.
#      So instead I have re-used code from SerenityOS that I already understand and that puts the OS 
#      into the post-boot state that I want.
# TODO #defines for 0x3
.code32
real_start:
    cli
    cld

    /*  https://www.gnu.org/software/grub/manual/multiboot/multiboot.html
        at this point in code:
        - GRUB has left us in 32-bit protected mode with paging disabled
          (meaning memory is identity mapped)
         - all segment registers store selector for segments that:
          - starts at offset 0
          - has limit 0xffffffff
        - %eax holds multiboot magic value 0x2badb002
         - %ebx holds paddr of multiboot info struct */

    # TODO cpuid check if PAE supported
    # TODO cpuid check if long mode supported

    # TODO if using a kernel command line, here is the point to copy it to a more useful location

    # TODO SerenityOS manually clears the page tables here, but gcc asm should set all .skip regions
    #      to 0, try to verify if manually clearing the page tabels is necessary

    /* point pml4t[0] to boot_pdpt */
    movl $boot_pml4t, %edi
    movl $boot_pdpt, 0(%edi)
    orl $0x3, 0(%edi) /* 0x3 is Writable && Present */

    /* -------------------- setup boot identity map -------------------- */

    /* point boot_pdpt to boot_pd */
    movl $boot_pdpt, %edi
    movl $boot_pd0, 0(%edi)
    orl $0x3, 0(%edi) /* 0x3 is Writable && Present */

    /* point boot_pd's entries to pd0_pts */
    movl $(max_kernel_pt_count), %ecx
    movl $boot_pd0, %edi
    movl $boot_pd0_pts, %eax
1:  /* TODO make this loop a macro? it is repeated below, note that call/ret does not work at this point in startup */
    movl %eax, 0(%edi)
    orl $0x3, 0(%edi)
    addl $8, %edi
    addl $(4*KB), %eax
    loop 1b

    /* identity map pd0_pts */
    movl $(max_kernel_page_count), %ecx
    movl $boot_pd0_pts, %edi
    xorl %eax, %eax
1:
    movl %eax, 0(%edi)
    orl $0x3, 0(%edi)
    addl $8, %edi
    addl $(4*KB), %eax
    loop 1b
    

    movl $boot_pml4t, %eax
    movl %eax, %cr3
    
    /* enables PAE (physical address extenrions) & PSE (page size extentions)
     *  - PAE is necessary to use 64-bit mode
     *  - PSE enables 4MB pages in 32-bit mode (not sure if it serves any purpose in 64-bit mode)
     */
    movl %cr4, %eax
    orl $0x60, %eax
    movl %eax, %cr4

    mov $0xC0000080, %ecx   /* select EFER_MSR */
    rdmsr
    or $(1<<8), %eax        /* set LME (long mode enable) bit */
    wrmsr
    
    movl %cr0, %eax
    orl $0x80000000, %eax   /* enable PG (paging enable) bit */
    movl %eax, %cr0

    mov $stack_top, %esp

    /* switch to 64-bit code segment selector via long jump to enter long mode */
    mov $gdt64_selector, %eax
    lgdt (%eax)
    ljmpl $gdt64_code_seg_ind_val, $1f

# TODO what is difference between 0(%eax) and (%eax)
.code64
1:
    movl %ebx, %ebx     /* TODO: what is the purpose of this? */
    movq %rbx, multiboot_info
    
    /* clear all segment selectors (which were set by GRUB according to multiboot specification) */
    mov $0, %ax
    mov %ax, %ss
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs

    /* reload cr3 */
    /* NOTE: gdb won't step through this unless you specifically use 'si', NOT 's' :( */
    call reload_cr3

    /* NOTE: the stack pointer must be 16-byte aligned when this is called */
    call kernel_main

    /* TODO try this now we have proper code segment */
    #mov $test_str, %ebx
    #call print

    cli
1:  
    hlt
    jmp 1b

reload_cr3:
    pushq %rax
    mov %cr3, %rax
    mov %rax, %cr3
    popq %rax
    ret


.size _start, . - _start

