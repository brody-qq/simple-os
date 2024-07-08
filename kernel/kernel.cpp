#include "kernel/types.h"
#include "kernel/kernel_defs.h"
#include "kernel/vga.cpp"
#include "kernel/asm.cpp"
#include "kernel/debug.cpp"
#include "external/multiboot.h"
#include "include/math.h"
#include "kernel/utils.h"
#include "kernel/range.cpp"
#include "kernel/scheduler.h"
#include "kernel/cpu.cpp"
#include "kernel/scheduler.cpp"
#include "kernel/pic.cpp"
#include "kernel/pit.cpp"
#include "kernel/page_tables.cpp"
#include "kernel/physical_allocator.cpp"
#include "kernel/vspace.cpp"
#include "kernel/kmalloc.cpp"
#include "kernel/ata.cpp"
#include "kernel/ext2.cpp"
#include "include/syscall.h"
#include "kernel/ps2_keyboard.cpp"
#include "kernel/tty.cpp"

extern "C" {
multiboot_info_t *multiboot_info;
}
// note on linker.ld variables: https://stackoverflow.com/questions/55622174/is-accessing-the-value-of-a-linker-script-variable-undefined-behavior-in-c
extern "C" paddr kernel_image_start[];
extern "C" paddr kernel_image_end[];

// TODO switch these to the page table types? will this break linking with boot.S ?
extern "C" u64 boot_pml4t[512];
extern "C" u64 boot_pdpt[512];
extern "C" u64 boot_kernel_pd[512];
extern "C" u64 boot_kernel_pt0[512];
extern "C" u64 boot_kernel_pts[512];

const u64 max_kernel_size = 64*MB; // TODO this value should be shared between boot.S and this code
const u64 end_of_boot_id_mapped_space = 64*MB; // TODO use this as the end point of boot.S tables, not max_kernel_size

// -------------------------------------

// TODO move to own file
u64 pspace_size = 0; // set after reading multiboot memory map tables

PRange g_kernel_image;

// TODO compiler error for non const expression
//static_assert(is_power_of_2(kernel_pspace_size));
//static_assert(is_power_of_2(kernel_vspace_size));

u64 g_pmap_page_count = 0;
u64 g_pmap_pd_count = 0;
u64 g_pmap_pdpt_count = 0;
u64 g_pmap_pml4t_count = 0;

// NOTE: these 2 tables are too big to be statically allocated in the kernel image
//       so they must be dynamically allocated
PDPT *g_pmap_pdpts;
PDMaps2MBPages *g_pmap_pds;

PML4T g_kernel_pml4t;

// TODO expand this to work for any level of page table
// TODO rewrite this function... it works but code is bad
void print_pdpt(PDPT& pdpt)
{
    dbg_str("PDPT addr: "); dbg_uint((u64)&pdpt); dbg_str("\n");
    for(int i = 0; i < PDPT::entry_count; ++i) {
        u64 pdpte = *(u64 *)&pdpt[i];
        dbg_str("    pdpt index: "); dbg_int(i);
        dbg_str(" val: "); dbg_uint(pdpte);
        dbg_str(" masked: "); dbg_uint(pdpte & 0xfffffffffffff000);
        dbg_str("\n");

        u64 pd_addr = pdpte & 0xfffffffffffff000;
        dbg_str("        PD addr: "); dbg_uint(pd_addr); dbg_str("\n");
        for(int j = 0; j < PDMaps2MBPages::entry_count; ++j) {
            u64 pde_addr = (pd_addr + j*8);
            u64 pde = *(u64 *)pde_addr;
            dbg_str("pde: "); dbg_uint(pde); dbg_str("\n");
            dbg_str("        pd index: "); dbg_int(j);
            dbg_str(" val: "); dbg_uint(pde);
            dbg_str("\n");
        }
    }
}
void print_pd(PDMaps2MBPages& pd)
{
    dbg_str("PD addr: "); dbg_uint((u64)&pd); dbg_str("\n");
    for(int i = 0; i < PDMaps2MBPages::entry_count; ++i) {
        PDEMaps2MBPage& pde = pd[i];
        dbg_str("    entry: "); dbg_int(i);
        dbg_str(" val: "); dbg_uint(*(u64 *)&pde);
        dbg_str("\n");
    }
}

PRange init_pmap(const PRange& aligned_usable_range, PML4T& pml4t)
{
    ASSERT(is_aligned(aligned_usable_range.addr, 4096));
    ASSERT(is_aligned(aligned_usable_range.one_past_end(), 4096));
    dbg_str("init_pmap\n");

    g_pmap_page_count = aligned_usable_range.one_past_end() / (2*MB);
    g_pmap_pd_count = max((u64)1, g_pmap_page_count / (u64)512);
    g_pmap_pdpt_count = max((u64)1, g_pmap_pd_count / 512);
    g_pmap_pml4t_count = max((u64)1, g_pmap_pdpt_count / 512);

    u64 pmap_pdpts_base = (u64)aligned_usable_range.addr;
    u64 pmap_pds_base = pmap_pdpts_base + g_pmap_pdpt_count*sizeof(PDPT);
    u64 pmap_tables_one_past_end = pmap_pds_base + g_pmap_pd_count*sizeof(PD);
    u64 pmap_length = pmap_tables_one_past_end - pmap_pdpts_base;
    ASSERT(is_aligned(pmap_pdpts_base, 4096));
    ASSERT(is_aligned(pmap_pds_base, 4096));
    ASSERT(is_aligned(pmap_tables_one_past_end, 4096));
    ASSERT((pmap_tables_one_past_end - pmap_pds_base) / 4096 == g_pmap_pd_count);
    ASSERT(g_pmap_pdpt_count == 1); // the rest of this function is written on the assumption of 1 pdpt
    ASSERT(g_pmap_pml4t_count == 1);
    ASSERT(pmap_tables_one_past_end < end_of_boot_id_mapped_space);

    PRange used_range = {
        pmap_pdpts_base,
        pmap_length
    };

    g_pmap_pdpts = (PDPT *)pmap_pdpts_base;
    *g_pmap_pdpts = PDPT();
    g_pmap_pds = (PDMaps2MBPages *)pmap_pds_base;
    for(PDMaps2MBPages *ptr = g_pmap_pds; ptr < (PDMaps2MBPages *)pmap_tables_one_past_end; ++ptr) {
        *ptr = PDMaps2MBPages();
    }

    pml4t[0].clear();
    pml4t[0].bitfield.present = 1;
    pml4t[0].bitfield.writable = 1;
    pml4t[0].set_phys_addr((u64)g_pmap_pdpts);

    dbg_str("size of pmap tables: ");
    dbg_uint(pmap_tables_one_past_end - pmap_pdpts_base);
    dbg_str("\n");

    dbg_str("pmap tables page count: ");
    dbg_uint(g_pmap_page_count);
    dbg_str("\n");

    dbg_str("pmap tables pd count: ");
    dbg_uint(g_pmap_pd_count);
    dbg_str("\n");

    dbg_str("pmap tables pdpt count: ");
    dbg_uint(g_pmap_pdpt_count);
    dbg_str("\n");

    dbg_str("pmap tables pml4t count: ");
    dbg_uint(g_pmap_pml4t_count);
    dbg_str("\n");

    // TODO zero memory, this works on qemu on first run, but qemu doesn't seem to zero memory if you restart
    // -------------------------------------------------------------------------------
    u64 page_addr = 0;
    u64 pd_addr = pmap_pds_base;
    u64 pd_addr_start = pd_addr;
    while(page_addr < aligned_usable_range.one_past_end()) {
        //dbg_str("pageaddr: "); dbg_uint(page_addr); dbg_str("\n");
        //dbg_str("pd_addr / 4096: "); dbg_uint((pd_addr - pd_addr_start) / 4096); dbg_str("\n");
        PDPT& pdpt = g_pmap_pdpts[0];

        u64 pdpt_i = pdpt_index(page_addr);
        PDPTE& pdpte = pdpt[pdpt_i];
        if(!pdpte.bitfield.present) {
            pdpte.clear();
            pdpte.bitfield.present = 1;
            pdpte.bitfield.writable = 1;
            pdpte.set_phys_addr(pd_addr);
            pd_addr += 4096;
            ASSERT(((pd_addr - pd_addr_start) / 4096) <= g_pmap_pd_count);
        }

        PDMaps2MBPages& pd = *(PDMaps2MBPages *)pdpte.get_phys_addr();
        u64 pd_i = pd_index(page_addr);
        PDEMaps2MBPage& pde = pd[pd_i];
        pde.clear();
        pde.bitfield.present = 1;
        pde.bitfield.writable = 1;
        pde.bitfield.page_size = 1;
        pde.set_phys_addr(page_addr);
        page_addr += 2*MB;
    }
    ASSERT(pml4t_index(page_addr) == 0); // make sure pmap fits into index 0 of pml4t

    ASSERT((page_addr / (2*MB)) == g_pmap_page_count);
    ASSERT(((pd_addr - pd_addr_start) / 4096) == g_pmap_pd_count);

    ASSERT(g_pmap_pdpts[0][0].get_phys_addr() == (u64)g_pmap_pds);
    ASSERT(pml4t[0].get_phys_addr() == (u64)g_pmap_pdpts);

    write_cr3((u64)&g_kernel_pml4t);

    return used_range;
}

// -------------------------------------------------------------------

extern Scheduler g_scheduler;

void process_test_d()
{
    for(int i = 0; i < 70000; ++i) {
        dbg_str("proc_test D\n");
        vga_print("proc_test D\n");
    }
    sys_exit();
}
void process_test_c()
{
    for(int i = 0; i < 70000; ++i) {
        dbg_str("proc_test C\n");
        vga_print("proc_test C\n");
    }
    sys_exit();
}
void process_test_b()
{
    create_kernel_process(process_test_c, EXEC_CAN_BE_ORPHANED, "test C");
    create_kernel_process(process_test_d, EXEC_CAN_BE_ORPHANED, "test D");
    for(int i = 0; i < 30000; ++i) {
        dbg_str("proc_test B\n");
        vga_print("proc_test B\n");
    }
    sys_exit();
}
void process_test_a()
{
    create_kernel_process(process_test_b, EXEC_IS_BLOCKING, "test B");
    for(int i = 0; i < 30000; ++i) {
        dbg_str("proc_test A\n");
        vga_print("proc_test A\n");
    }
    sys_exit();
}
/*
void init_process()
{
    create_kernel_process(process_test_a, EXEC_IS_BLOCKING, "test A");
    while(1) {
        dbg_str("init process\n");
        vga_print("init process\n");
    }
}
*/
extern TTY g_tty;
void init_process()
{
    vga_print("init process\n");

    const char *str = "1\n2\n\n3\n\n\n0123456789012345\n";
    g_tty.write_str(str, strlen_workaround(str));
    str = "12345678901234567890123456789012345678901234567890_____4567890abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890"; 
    g_tty.write_str(str, strlen_workaround(str));

    str = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"; 
    g_tty.write_str(str, strlen_workaround(str));

    str = "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"; 
    g_tty.write_str(str, strlen_workaround(str));

    str = "cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"; 
    g_tty.write_str(str, strlen_workaround(str));

    str = "dddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddd"; 
    g_tty.write_str(str, strlen_workaround(str));

    str = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"; 
    g_tty.write_str(str, strlen_workaround(str));

    str = "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"; 
    g_tty.write_str(str, strlen_workaround(str));

    str = "cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"; 
    g_tty.write_str(str, strlen_workaround(str));

    str = "dddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddd"; 
    g_tty.write_str(str, strlen_workaround(str));

    str = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"; 
    g_tty.write_str(str, strlen_workaround(str));

    str = "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"; 
    g_tty.write_str(str, strlen_workaround(str));

    str = "cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"; 
    g_tty.write_str(str, strlen_workaround(str));

    str = "dddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddd"; 
    g_tty.write_str(str, strlen_workaround(str));

    str = "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"; 
    g_tty.write_str(str, strlen_workaround(str));

    str = "cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"; 
    g_tty.write_str(str, strlen_workaround(str));

    str = "dddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddd"; 
    g_tty.write_str(str, strlen_workaround(str));

    str = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"; 
    g_tty.write_str(str, strlen_workaround(str));

    str = "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"; 
    g_tty.write_str(str, strlen_workaround(str));

    str = "cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"; 
    g_tty.write_str(str, strlen_workaround(str));

    str = "dddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddd"; 
    g_tty.write_str(str, strlen_workaround(str));

    u32 max_len = g_tty.CMD_LEN;
    char cmd_line[max_len + 1];
    memset_workaround(cmd_line, 0, max_len);
    const char * cmdline = "cmdline> ";
    u32 cmdline_len = strlen_workaround(cmdline);

    u32 cmd_start_i = 0;
    u32 cmd_onepastend = 0;
    u32 cursor_i = 0;
    auto reset_cmd_line = [&]() {
        cmd_start_i = cmdline_len;
        cmd_onepastend = cmdline_len;
        cursor_i = cmdline_len;

        memmove_workaround(cmd_line, (void *)cmdline, cmdline_len);
        memset_workaround(cmd_line + cmdline_len, 0, max_len - cmdline_len);
        g_tty.set_cursor(cursor_i);
        g_tty.set_cmdline(cmd_line);

        g_tty.flush_to_vga();
    };
    reset_cmd_line();
    while(1) {
        PollKeyboardResult res;
        sys_poll_keyboard(&res);
        if(res.has_result) {
            KeyEvent e = res.event;
            if(e.pressed) {
                switch(e.key) {
                    case KEY_PAGE_UP: {
                        g_tty.scroll_up(1);
                        g_tty.flush_to_vga();
                    } break;
                    case KEY_PAGE_DOWN: {
                        g_tty.scroll_down(1);
                        g_tty.flush_to_vga();
                    } break;
                    case KEY_LEFT: {
                        if(cursor_i > cmd_start_i) {
                            cursor_i--;
                            g_tty.set_cursor(cursor_i);
                            g_tty.flush_to_vga();
                        }
                    } break;
                    case KEY_RIGHT: {
                        if(cursor_i < cmd_onepastend) {
                            cursor_i++;
                            g_tty.set_cursor(cursor_i);
                            g_tty.flush_to_vga();
                        }
                    } break;
                    case KEY_BACKSPACE: {
                        if(cursor_i > cmd_start_i) {
                            cursor_i--;
                            cmd_onepastend--;
                            u32 len = max_len - cursor_i -1;
                            memmove_workaround(cmd_line + cursor_i, cmd_line + cursor_i+1, len);
                            cmd_line[max_len-1] = 0;
                            g_tty.set_cursor(cursor_i);
                            g_tty.set_cmdline(cmd_line);
                            g_tty.flush_to_vga();
                        }
                    } break;
                    case KEY_ENTER: {
                        g_tty.flush_cmdline();
                        reset_cmd_line();
                        g_tty.set_cmdline(cmd_line);
                        g_tty.flush_to_vga();
                    } break;

                    default: {
                        if(is_ascii_event(e)) {
                            if(cursor_i < max_len) {
                                char c = keyevent_to_ascii(e);
                                u32 len = max_len - cursor_i -1;
                                memmove_workaround(cmd_line + cursor_i + 1, cmd_line + cursor_i, len);
                                cmd_line[cursor_i] = c;
                                cmd_onepastend = min(cmd_onepastend+1, max_len);
                                cursor_i = min(cursor_i+1, max_len);
                                g_tty.set_cursor(cursor_i);
                                g_tty.set_cmdline(cmd_line);
                                g_tty.flush_to_vga(); // TODO make a function to flush only the cmdline to vga
                            }
                        }
                    } break;
                } 
            }
        }
    }
 // kernel will crash if there is only 1 process and that process ends
}

/*
void process_test_b_user()
{
    for(int i = 0; i < 1000; ++i) {
        u8 *ptr = (u8 *)sys_alloc(4096, 64);
        const char *str = "user process B\n";
        u64 len = strlen_workaround(str);
        memmove_workaround(ptr, (char *)str, len);
        sys_print(str, len);
        sys_free(ptr);
    }
    sys_exit();
}
void process_test_a_user()
{
    sys_exec(process_test_b_user, EXEC_CAN_BE_ORPHANED, "test B");
    for(int i = 0; i < 1000; ++i) {
        u8 *ptr = (u8 *)sys_alloc(4096, 64);
        const char *str = "user process A\n";
        u64 len = strlen_workaround(str);
        memmove_workaround(ptr, (char *)str, len);
        sys_print(str, len);
        sys_free(ptr);
    }
    sys_exit();
}
void init_process_user()
{
    sys_exec(process_test_a_user, EXEC_IS_BLOCKING, "test A");
    while(1) {
        u8 *ptr = (u8 *)sys_alloc(4096, 64);
        const char *str = "init process user\n";
        u64 len = strlen_workaround(str);
        memmove_workaround(ptr, (char *)str, len);
        sys_print(str, len);
        sys_free(ptr);
    }
}
*/

//void process_test_a()
//{
    //g_in_kernel_init = false;
    //dbg_str("proc_test A\n");
    //vga_print("proc_test A\n");
    //sys_yield();
    //dbg_str("proc_test A resumed\n");
    //vga_print("proc_test A resumed\n");
    //sys_exit();
    //UNREACHABLE();
//}

//void process_test_c();
//void process_test_b()
//{
    //dbg_str("proc_test B\n");
    //vga_print("proc_test B\n");
    //sys_yield();
    //dbg_str("proc_test B resumed\n");
    //vga_print("proc_test B resumed\n");
    //sys_exec(process_test_c);
    //dbg_str("proc_test B resumed 2\n");
    //vga_print("proc_test B resumed 2\n");
    //sys_exit();
    //UNREACHABLE();
//}

//void process_test_c()
//{
    //dbg_str("proc_test C\n");
    //vga_print("proc_test C\n");
    //sys_yield();
    //dbg_str("proc_test C resumed\n");
    //vga_print("proc_test C resumed\n");
    //sys_exit();
    //UNREACHABLE();
//}
// ------------------------------------

// TODO add clear_line to vga_print()
// TODO add print backtrace to debug.cpp
// TODO add print page tables to debug.cpp
// TODO check how widely available 1GB pages are, if they are widely available use them for the pmap
// TODO allocate kernel stack with guard pages? (or wait until make kernel processes are implemented to do this?)

// NOTE: at this point, the kernel is in the current state:
//  - 64-bit mode
//  - using statically allocated page tables from boot.S that identity map the first max_kernel_size of memory (TODO skip this part and go straight to pmap?)
//  - using statically allocated stack from boot.S that does NOT end with a guard page (TODO fix this?)
//  - CPU floating point state has not been setup
//  - IDT, TSS, have not been setup
//  - GDT only contains one kernel mode code descriptor
//  - interrupts are disabled (via cli)
//  - PIC, APIC, IO-APIC have not been set up
//  - no devices are set up (aside from devices setup by BIOS or GRUB)

// from SerenityOS to get around linker complaining about lack of __cxa_atexit and __dso_handle
void* __dso_handle __attribute__((visibility("hidden")));
extern "C" int __cxa_atexit(void (*)(void*), void*, void*);
extern "C" int __cxa_atexit(void (*)(void*), void*, void*)
{
    UNREACHABLE();
    return 0;
}

VObject g_interrupt_stack_vobj;
Stack g_interrupt_stack;

// TODO set this to false inside first process
bool g_in_kernel_init = true;
extern "C" void kernel_main()
{
    // TODO share the _str, _int and _uint logic between vga and serial debug out
    vga_initialize();
    init_serial();
    // TODO make funciton to print to serial & vga
    dbg_str("start of kernel_main()\n");
    vga_print("start of kernel_main()\n");

    // TODO
    //  - parse mutliboot mmap to find out which memory regions are usable
    //  - setup PhysicalPage types, array, list & freelist (make ll_* functions to deal with linked lists)
    //  - PhysicalPage array should only represent pages that are usable, it's only purpose is for tracking allocations
    //  - setup basic allocator to allocate one at a time from freelist
    //  - when above is setup, we can setup pmap (make types for page table entries, implement using large pages to save space)

    dbg_str("kernel_image_start: ");
    dbg_uint((u64)kernel_image_start);
    dbg_str("\n");
    dbg_str("kernel_image_end: ");
    dbg_uint((u64)kernel_image_end);
    dbg_str("\n");

    vga_print("init gdt\n");
    dbg_str("init gdt\n");
    init_gdt();

    dbg_str("init idt\n");
    vga_print("init idt\n");
    init_idt();

    dbg_str("init PIC\n");
    vga_print("init PIC\n");
    g_pic.initialize(PIC::PIC1_BASE_VECTOR, PIC::PIC2_BASE_VECTOR);

    g_kernel_image = { (u64)kernel_image_start, (u64)(kernel_image_end - kernel_image_start) };

    PRange max_usable_range;
    multiboot_mmap_entry *mmap = (multiboot_mmap_entry *)(size_t)multiboot_info->mmap_addr;
    u32 mmap_len = multiboot_info->mmap_length / sizeof(multiboot_mmap_entry);
    for(u32 i = 0; i < mmap_len; ++i) {
        multiboot_mmap_entry entry = mmap[i];
        if(entry.type != MULTIBOOT_MEMORY_AVAILABLE)
            continue;
        if((u64)entry.len < max_usable_range.length)
            continue;

        max_usable_range.addr = (paddr)entry.addr;
        max_usable_range.length = (u64)entry.len;

        dbg_str("usable region:\n");
        print_prange({(u64)entry.addr, (u64)entry.len});
        dbg_str("max_usable_range:\n");
        print_prange(max_usable_range);
    }

    if((u64)kernel_image_end > max_usable_range.addr) {
        max_usable_range = max_usable_range.subtract({(paddr)kernel_image_end, 0});
    }
    
    // TODO is kernel_image_end the last address or one past the last addres?
    ASSERT(max_usable_range.addr >= (paddr)kernel_image_end);
    ASSERT(max_usable_range.one_past_end() <= MAX_KERNEL_PSPACE_SIZE);

    paddr aligned_start_page = round_up_align(max_usable_range.addr, 4096);
    paddr aligned_end_page = round_down_align(max_usable_range.one_past_end(), 2*MB); // align to 2mb since the pmap uses 2mb pages
    PRange aligned_usable_range = {
        aligned_start_page,
        aligned_end_page - aligned_start_page
    };
    pspace_size = aligned_usable_range.one_past_end();

    ASSERT(is_aligned(aligned_usable_range.addr, 4096));
    ASSERT(is_aligned(aligned_usable_range.one_past_end(), 2*MB));
    ASSERT((aligned_usable_range.length % 4096) == 0);

    dbg_str("max_usable_range:\n");
    print_prange(max_usable_range);
    dbg_str("aligned_usable_range:\n");
    print_prange(aligned_usable_range);

    // TODO currently the boot code does this:
    //  - statically allocate space for boot page tables in boot.S
    //  - identity map first 64MB of address space in boot.S
    //  - jump into kernel_main
    //  - init_pmap() then dynamically allocates enough space to identity map the first 32GB of address space
    //  - init_pmap() then identity maps the first 32GB of address space
    //  - the statically allocated space in boot.S now goes unused
    // there is obviously some redundancy and wasted memory in the above steps, see if I can skip using the 
    // statically allocated boot page tables in boot.S and go straight to dynamically allocating the pmap for
    // and identity mapping the first 32GB of memory
    
    // TODO zero memory
    dbg_str("init pmap\n");
    vga_print("init pmap\n");
    g_kernel_pml4t.clear();
    PRange used_by_pmap = init_pmap(aligned_usable_range, *kernel_pml4t());
    aligned_usable_range = aligned_usable_range.subtract({used_by_pmap});

    dbg_str("init phys_pages_arr\n");
    vga_print("init phys_pages_arr\n");
    g_phys_page_allocator.init(aligned_usable_range);

    dbg_str("init kernel vspace\n");
    vga_print("init kernel vspace\n");
    VSpace::init_kernel_vspace();
    g_kernel_vspace_is_initialized = true;

    // TODO kmalloc heap:
    //  - allocate enough pages (alloc_page())
    //  - allocate range of kernel vaddr space (everything above max_phys_addr is free)
    //  - map the vrange to the allocated pages (make function for this, this will be a very common operation for processes)
    //  - return vaddr



    // TODO remove this or put into separate test file when done testing kmalloc()
    // TODO make some proper, thorough unit tests for kmalloc, since everything else is built on top of this
    // ---------------------------------------------------------------------------------------------------------------------------
    /*
    dbg_str("kmalloc() test start\n");
    vga_print("kmalloc() test start\n");

    u64 start_free_space = g_kernel_vspace.get_free_space();
    u64 start_phys_pages_freelist = g_phys_page_allocator.count_freelist_entries();
    for(int i = 2; i < 1000; ++i) {
        //u64 mallocsize = 4096*3;
        int str1_len = i;
        u64 addr1_align = 16;
        char *addr1 = (char *)kmalloc(str1_len, addr1_align);
        ASSERT(is_aligned((u64)addr1, addr1_align));
        char str1[] = {'a', 'b', 'c', 'd'};
        for(int j = 0; j < str1_len; ++j) {
            addr1[j] = str1[j % 4];
        }
        addr1[str1_len -1] = 0;
        dbg_str(addr1); dbg_str("\n");

        int str2_len = i*2;
        u64 addr2_align = 32;
        char *addr2 = (char *)kmalloc(str2_len, addr2_align);
        ASSERT(is_aligned((u64)addr2, addr2_align));
        char str2[] = {'h', 'j', 'k', 'l'};
        for(int j = 0; j < str2_len; ++j) {
            addr2[j] = str2[j % 4];
        }
        addr2[str2_len -1] = 0;
        dbg_str(addr2); dbg_str("\n");

        int str3_len = i*4;
        u64 addr3_align = 64;
        char *addr3 = (char *)kmalloc(str3_len, addr3_align);
        ASSERT(is_aligned((u64)addr3, addr3_align));
        char str3[] = {'q', 'w', 'e', 'r'};
        for(int j = 0; j < str3_len; ++j) {
            addr3[j] = str3[j % 4];
        }
        addr3[str3_len -1] = 0;
        dbg_str(addr3); dbg_str("\n");

        dbg_str("1\n");
        kfree((vaddr)addr1);
        dbg_str("2\n");
        kfree((vaddr)addr3);
        dbg_str("3\n");
        kfree((vaddr)addr2);
    };
    u64 end_free_space = g_kernel_vspace.get_free_space();
    u64 end_phys_pages_freelist = g_phys_page_allocator.count_freelist_entries();
    dbg_uint(start_free_space); dbg_str("  "); dbg_uint(end_free_space); dbg_str("\n");
    ASSERT(end_free_space == start_free_space);
    dbg_uint(start_phys_pages_freelist); dbg_str("  "); dbg_uint(end_phys_pages_freelist); dbg_str("\n");
    ASSERT(end_phys_pages_freelist == start_phys_pages_freelist);

    start_phys_pages_freelist = g_phys_page_allocator.count_freelist_entries();
    start_free_space = g_kernel_vspace.get_free_space();
    vaddr a1 = kmalloc(23464, 2);
    vaddr a2 = kmalloc(1111111, 512); // 11MB
    vaddr a3 = kmalloc(9*MB, 1);
    vaddr a4 = kmalloc(4*KB, 256);
    vaddr a5 = kmalloc(101, 2);
    vaddr a6 = kmalloc(898989, 1); // 1 mb
    vaddr a7 = kmalloc(55555, 4);
    vaddr a8 = kmalloc(51, 128);
    vaddr a9 = kmalloc(73, 64);
    vaddr a0 = kmalloc(7*MB, 1);
    kfree(a4);
    kfree(a7);
    kfree(a2);
    kfree(a3);
    kfree(a8);
    kfree(a1);
    kfree(a0);
    kfree(a9);
    kfree(a6);
    kfree(a5);
    end_free_space = g_kernel_vspace.get_free_space();
    dbg_uint(start_free_space); dbg_str("  "); dbg_uint(end_free_space); dbg_str("\n");
    ASSERT(end_free_space == start_free_space);
    end_phys_pages_freelist = g_phys_page_allocator.count_freelist_entries();
    dbg_uint(start_phys_pages_freelist); dbg_str("  "); dbg_uint(end_phys_pages_freelist); dbg_str("\n");
    ASSERT(end_phys_pages_freelist == start_phys_pages_freelist);

    dbg_str("kmalloc() test end\n");
    vga_print("kmalloc() test end\n");
    */
    // ---------------------------------------------------------------------------------------------------------------------------

/*
    dbg_str("start interrupt test\n");
    vga_print("start interrupt test\n");
    asm volatile("int $0x0");
    asm volatile("int $0x1");
    asm volatile("int $0x2");
    asm volatile("int $0x3");
    asm volatile("int $0x4");
    asm volatile("int $0x5");
    asm volatile("int $0x6");
    asm volatile("int $0x7");
    //asm volatile("int $0x8"); // this is an abort
    //asm volatile("int $0xa"); // error code exceptions don't work with int N instruction
    //asm volatile("int $0xb"); // error code exceptions don't work with int N instruction
    //asm volatile("int $0xc"); // error code exceptions don't work with int N instruction
    //asm volatile("int $0xd"); // error code exceptions don't work with int N instruction
    //asm volatile("int $0xe"); // error code exceptions don't work with int N instruction
    asm volatile("int $0x10");
    //asm volatile("int $0x11"); // error code exceptions don't work with int N instruction
    asm volatile("int $0x12");
    asm volatile("int $0x13");
    //asm volatile("int $0x15"); // error code exceptions don't work with int N instruction
    asm volatile("int $0x1c");
    asm volatile("int $0x1d");
    asm volatile("int $0x1e");
    asm volatile("int $0x4e");
    // test syscall
    asm volatile("pushq %rcx\n"
                 "movq $0x1, %rcx\n"
                 "int $0xff\n"
                 "popq %rcx\n");
    dbg_str("end interrupt test\n");
    vga_print("end interrupt test\n");
    */

// ----------------------------------------------------------------------------------------------
    dbg_str("init ide devices\n");
    vga_print("init ide devices\n");
    init_ide_devices();
    //u8 buffer[1024];
    //read_sectors_pio(buffer, 2, 1);

    //u8 buffer2[1024];
    //for(int i = 0; i < 1024; ++i) {
    //    buffer2[i] = 0xcc;
    //}
    //write_sectors_pio(buffer2, 2, 256);
// ----------------------------------------------------------------------------------------------
    dbg_str("init ext2\n");
    vga_print("init ext2\n");
    init_ext2();
// ----------------------------------------------------------------------------------------------
// TEST EXT2
/*
    u16 result = fs_create("/emptydir/test.txt", FS_TYPE_REG_FILE);
    ASSERT(result == FS_STATUS_OK);
    u64 assumedblocksize = 4096;
    u64 size = assumedblocksize * (12 + 1024 +1);
    u8 *buf = (u8 *)kmalloc(size, 4096);
    for(u64 i = 0; i < size; ++i)
        buf[i] = 'c';
    result = fs_write("/emptydir/test.txt", buf, 0, size);
    ASSERT(result == FS_STATUS_OK);
    result = fs_read("/emptydir/test.txt", buf, 0, size);
    ASSERT(result == FS_STATUS_OK);
    kfree((vaddr)buf);

    const char *str1 = "abcd";
    result = fs_write("/emptydir/test.txt", (u8 *)str1, 0x100, 4);
    ASSERT(result == FS_STATUS_OK);
    result = fs_write("/emptydir/test.txt", (u8 *)str1, size, 4);
    ASSERT(result == FS_STATUS_OK);
    result = fs_write("/emptydir/test.txt", (u8 *)str1, 0x400000, 4);
    ASSERT(result == FS_STATUS_OK);

    u64 size2 = assumedblocksize * 100;
    u8 *buf2 = (u8 *)kmalloc(size2, 4096);
    for(u64 i = 0; i < size2; ++i)
        buf[i] = 'f';
    result = fs_write("/emptydir/test.txt", (u8 *)buf2, 0x20, size2);
    ASSERT(result == FS_STATUS_OK);
    result = fs_truncate("/emptydir/test.txt", 0x200);
    ASSERT(result == FS_STATUS_OK);
    result = fs_write("/emptydir/test.txt", (u8 *)buf2, 0x300, size2);
    ASSERT(result == FS_STATUS_OK);
    result = fs_truncate("/emptydir/test.txt", 0);
    ASSERT(result == FS_STATUS_OK);
    result = fs_write("/emptydir/test.txt", (u8 *)buf2, 0, assumedblocksize);
    ASSERT(result == FS_STATUS_OK);
    result = fs_write("/emptydir/test.txt", (u8 *)buf2, 1 + assumedblocksize * 2, assumedblocksize);
    ASSERT(result == FS_STATUS_OK);
    result = fs_write("/emptydir/test.txt", (u8 *)str1, 0x1fff, 4);
    ASSERT(result == FS_STATUS_OK);
    result = fs_write("/emptydir/test.txt", (u8 *)str1, 0x2fff, 4);
    ASSERT(result == FS_STATUS_OK);
    result = fs_write("/emptydir/test.txt", (u8 *)str1, 0x4000, 4);
    ASSERT(result == FS_STATUS_OK);
    kfree((vaddr)buf2);

// path lookups
    auto inode1 = lookup_path("/");
    auto inode2 = lookup_path("/boot/");
    auto inode3 = lookup_path("/boot");
    auto inode4 = lookup_path("/boot/grub/grub.cfg");
    auto inode5 = lookup_path("/boot/boot.elf");
    auto inode6 = lookup_path("/../");
    auto inode7 = lookup_path("/..");
    auto inode8 = lookup_path("/./");
    auto inode9 = lookup_path("/.");
    auto inode10 = lookup_path("/boot/../");
    auto inode11 = lookup_path("/boot/..");
    auto inode12 = lookup_path("/boot/./");
    auto inode13 = lookup_path("/boot/.");
    auto inode14 = lookup_path("///./boot/.///..//.//././///boot//boot.elf");
    auto inode15 = lookup_path("/boot////");
    dbg_uint(inode1.inode_num); dbg_str("\n");
    dbg_uint(inode2.inode_num); dbg_str("\n");
    dbg_uint(inode3.inode_num); dbg_str("\n");
    dbg_uint(inode4.inode_num); dbg_str("\n");
    dbg_uint(inode5.inode_num); dbg_str("\n");
    dbg_uint(inode6.inode_num); dbg_str("\n");
    dbg_uint(inode7.inode_num); dbg_str("\n");
    dbg_uint(inode8.inode_num); dbg_str("\n");
    dbg_uint(inode9.inode_num); dbg_str("\n");
    dbg_uint(inode10.inode_num); dbg_str("\n");
    dbg_uint(inode11.inode_num); dbg_str("\n");
    dbg_uint(inode12.inode_num); dbg_str("\n");
    dbg_uint(inode13.inode_num); dbg_str("\n");
    dbg_uint(inode14.inode_num); dbg_str("\n");
    dbg_uint(inode15.inode_num); dbg_str("\n");

// read
    const char *path = "/boot/grub/grub.cfg";
    u64 filesize = fs_file_size(path).size;
    u8 *filebuffer = (u8 *)kmalloc(filesize, 4096);
    result = fs_read(path, filebuffer, 0, filesize);
    ASSERT(result == FS_STATUS_OK);
    result = fs_read(path, filebuffer, filesize, filesize);
    ASSERT(result == FS_STATUS_BAD_ARG);
    kfree((vaddr)filebuffer);

// read with offset
    u64 offset = 10;
    filesize = fs_file_size(path).size - offset;
    filebuffer = (u8 *)kmalloc(filesize, 4096);
    fs_read(path, filebuffer, offset, filesize);
    kfree((vaddr)filebuffer);

// create file & dir
    result = fs_create("/test_create_file.txt", FS_TYPE_REG_FILE);
    ASSERT(result == FS_STATUS_OK);
    result = fs_create("/testdir/", FS_TYPE_DIR);
    ASSERT(result == FS_STATUS_OK);
    //UNREACHABLE(); // bail out here and check if empty dir /boot/testdir is recognized by linux filesystem
    result = fs_create("/testdir/testfile.txt", FS_TYPE_REG_FILE);
    ASSERT(result == FS_STATUS_OK);

    result = fs_create("/testdir/testfile.txt", FS_TYPE_REG_FILE);
    ASSERT(result == FS_STATUS_FILE_ALREADY_EXISTS);
    result = fs_create("/testdir/testfile.txt", FS_TYPE_DIR);
    ASSERT(result == FS_STATUS_FILE_ALREADY_EXISTS);

// TODO writes
    const char *text = "abcdefghijklmnopqrstuvwxyz";
    result = fs_write("/testdir/testfile.txt", (u8 *)text, 0, 26);
    ASSERT(result == FS_STATUS_OK);
    filebuffer = (u8 *)kmalloc(26, 4096);
    result = fs_read("/testdir/testfile.txt", filebuffer, 0, 26);
    ASSERT(result == FS_STATUS_OK);
    kfree((vaddr)filebuffer);

// TODO truncate
    result = fs_truncate("/testdir/testfile.txt", 12);
    ASSERT(result == FS_STATUS_OK);
    filebuffer = (u8 *)kmalloc(12, 4096);
    result = fs_read("/testdir/testfile.txt", filebuffer, 0, 12);
    ASSERT(result == FS_STATUS_OK);
    kfree((vaddr)filebuffer);

    result = fs_truncate("/testdir/testfile.txt", 0);
    ASSERT(result == FS_STATUS_OK);
    result = fs_write("/testdir/testfile.txt", (u8 *)text, 10, 26);
    ASSERT(result == FS_STATUS_OK);
    filebuffer = (u8 *)kmalloc(36, 4096);
    result = fs_read("/testdir/testfile.txt", filebuffer, 0, 36);
    ASSERT(result == FS_STATUS_OK);
    kfree((vaddr)filebuffer);

    result = fs_delete("/testdir/");
    ASSERT(result == FS_STATUS_DIR_NOT_EMPTY);
    result = fs_truncate("/testdir/testfile.txt", 10*MB);
    ASSERT(result == FS_STATUS_BAD_ARG);
    result = fs_delete("/testdir/testfile.txt");
    ASSERT(result == FS_STATUS_OK);
    result = fs_delete("/testdir/");
    ASSERT(result == FS_STATUS_OK);

    u8 buf3[10] = {0};
    result = fs_delete("/file/does/not/exist/");
    ASSERT(result == FS_STATUS_FILE_NOT_FOUND);
    ASSERT(fs_file_size("/file/does/not/exist/").found_file == false);
    result = fs_truncate("/file/does/not/exist/", 10);
    ASSERT(result == FS_STATUS_FILE_NOT_FOUND);
    result = fs_write("/file/does/not/exist/", buf3, 0, 10);
    ASSERT(result == FS_STATUS_FILE_NOT_FOUND);
    result = fs_read("/file/does/not/exist/", buf3, 0, 10);
    ASSERT(result == FS_STATUS_FILE_NOT_FOUND);

    result = fs_truncate("/emptydir", 0);
    ASSERT(result == FS_STATUS_WRONG_FILE_TYPE);

    result = fs_create("/newdir/", FS_TYPE_REG_FILE);
    ASSERT(result == FS_STATUS_BAD_ARG);

// create enough files to cause ext2 to allocate a new directory block
    char str[] = {'/', 'e', 'm', 'p', 't', 'y', 'd', 'i', 'r', '/', 't', 'e', 's', 't', 'f', 'i', 'l', 'e', '_', 't', 'e', 's', 't', 'f', 'i', 'l', 'e', '_', 't', 'e', 's', 't', 'f', 'i', 'l', 'e', '_', 't', 'e', 's', 't', 'f', 'i', 'l', 'e', '_', 't', 'e', 's', 't', 'f', 'i', 'l', 'e', '_', 't', 'e', 's', 't', 'f', 'i', 'l', 'e', '_', 't', 'e', 's', 't', 'f', 'i', 'l', 'e', '_', 't', 'e', 's', 't', 'f', 'i', 'l', 'e', '_', 't', 'e', 's', 't', 'f', 'i', 'l', 'e', '_', 't', 'e', 's', 't', 'f', 'i', 'l', 'e', '_', 't', 'e', 's', 't', 'f', 'i', 'l', 'e', '_', 't', 'e', 's', 't', 'f', 'i', 'l', 'e', '_', 't', 'e', 's', 't', 'f', 'i', 'l', 'e', '_', 't', 'e', 's', 't', 'f', 'i', 'l', 'e', '_', 't', 'e', 's', 't', 'f', 'i', 'l', 'e', '_', 't', 'e', 's', 't', 'f', 'i', 'l', 'e', '_', 't', 'e', 's', 't', 'f', 'i', 'l', 'e', '_', 't', 'e', 's', 't', 'f', 'i', 'l', 'e', '_', 't', 'e', 's', 't', 'f', 'i', 'l', 'e', '_', 't', 'e', 's', 't', 'f', 'i', 'l', 'e', '_', 't', 'e', 's', 't', 'f', 'i', 'l', 'e', '#', '#', '.', 't', 'x', 't', '\0'};
    char printstr[] = {'i', ':', ' ', '#', ' ', 'j', ':', ' ', '#', '\0'};
    for(int i = 0; i < 5; ++i) {
        if(i == 2) {
            // this is the iteration that causes directory to allocate a new block
            [[maybe_unused]] int breakpoint = 0;
        }
        for(int j = 0; j < 10; ++j) {
            char digit1 = '0' + i;
            char digit2 = '0' + j;
            str[198] = digit1;
            str[199] = digit2;
            printstr[3] = digit1;
            printstr[8] = digit2;
            vga_print("iteration "); vga_print(printstr); vga_print("\n");
            result = fs_create((const char *)str, FS_TYPE_REG_FILE);
            ASSERT(result == FS_STATUS_OK);
        }
    }

// deletes
    for(int i = 0; i < 5; ++i) {
        for(int j = 0; j < 10; ++j) {
            char digit1 = '0' + i;
            char digit2 = '0' + j;
            str[198] = digit1;
            str[199] = digit2;
            printstr[3] = digit1;
            printstr[8] = digit2;
            vga_print("iteration "); vga_print(printstr); vga_print("\n");
            result = fs_delete((const char *)str);
            ASSERT(result == FS_STATUS_OK);
        }
    }

    // create, mv, delete moved file
    fs_create("/path1", FS_TYPE_DIR);
    fs_mv("/path1", "/path2");
    auto res = fs_stat("/path1");
    ASSERT(!res.found_file);
    res = fs_stat("/path2");
    ASSERT(res.found_file);
    ASSERT(res.is_dir);
    fs_delete("/path2");
    res = fs_stat("/path2");
    ASSERT(!res.found_file);

    fs_create("/path1", FS_TYPE_DIR);
    fs_create("/path1/file1", FS_TYPE_REG_FILE);
    fs_mv("/path1/file1", "/path1/file2");
    res = fs_stat("/path1/file2");
    ASSERT(res.found_file);
    ASSERT(!res.is_dir);
    fs_mv("/path1/file2", "/file3");
    res = fs_stat("/path1/file2");
    ASSERT(!res.found_file);
    res = fs_stat("/file3");
    ASSERT(res.found_file);
    ASSERT(!res.is_dir);
    fs_delete("/file3");
    res = fs_stat("/file3");
    ASSERT(!res.found_file);

// TODO make sure all the above tests did not cause memory leaks

    u8 testbitmap1[1] = {0b00000111};
    [[maybe_unused]] auto res1 = bitmap_find_zero_and_set(testbitmap1, 4);
    u8 testbitmap2[1] = {0b00001111};
    [[maybe_unused]] auto res2 = bitmap_find_zero_and_set(testbitmap2, 4);
    u8 testbitmap3[1] = {0b00011111};
    [[maybe_unused]] auto res3 = bitmap_find_zero_and_set(testbitmap3, 4);

    u8 testbitmap4[3] = {0xff, 0xff, 0b00000111};
    [[maybe_unused]] auto res4 = bitmap_find_zero_and_set(testbitmap4, 2*8 + 4);
    u8 testbitmap5[3] = {0xff, 0xff, 0b00001111};
    [[maybe_unused]] auto res5 = bitmap_find_zero_and_set(testbitmap5, 2*8 + 4);
    u8 testbitmap6[3] = {0xff, 0xff, 0b00011111};
    [[maybe_unused]] auto res6 = bitmap_find_zero_and_set(testbitmap6, 2*8 + 4);

    u8 testbitmap7[3] = {0xfe, 0xff, 0b00000111};
    [[maybe_unused]] auto res7 = bitmap_find_zero_and_set(testbitmap7, 2*8 + 4);

    u8 testbitmap8[3] = {0xff, 0xff, 0b00000111};
    bitmap_unset(testbitmap8, 2*8 + 2);
    u8 testbitmap9[3] = {0b10101010, 0xff, 0b00000111};
    bitmap_unset(testbitmap9, 1);
    u8 testbitmap10[3] = {0xff, 0b11111110, 0b00000111};
    bitmap_unset(testbitmap10, 8 + 1);

    dbg_str("T E S T\n");
    const char *p = "/";
    int size1 = fs_list_dir_buffer_size(p);
    char *buf1 = (char *)kmalloc(size, 64);
    char *buf_one_past_end = nullptr;
    fs_list_dir(p, buf1, size1, &buf_one_past_end);
    char *ptr = buf1;
    while(ptr < buf_one_past_end) {
        dbg_str(ptr);
        dbg_str("\n");
        ptr += strlen_workaround(ptr) + 1;
    }
    UNREACHABLE();
    */
// ----------------------------------------------------------------------------------------------
    dbg_str("init interrupt stack\n");
    vga_print("init interrupt stack\n");
    VObject::initialize_interrupt_stack(Process::DEFAULT_STACK_SIZE, Process::DEFAULT_STACK_ALIGNMENT);
    g_interrupt_stack.bottom = g_interrupt_stack_vobj.map(g_kernel_vspace);
    g_interrupt_stack.set_stack_top(Process::DEFAULT_STACK_SIZE, Process::DEFAULT_STACK_ALIGNMENT);

    dbg_str("init tty\n");
    vga_print("init tty\n");
    TTY::init_tty();

    dbg_str("init process\n");
    vga_print("init process\n");
// ----------------------------------------------------------------------------------------------
    // TODO operator new
//    g_init_process = (Process *)kmalloc(sizeof(Process), alignof(Process));
//    new ((void *)g_init_process) Process(init_process, false, "init process", true, "/");
//    g_scheduler.add_to_queue(g_init_process);
//
//    g_ps2_keyboard.initialize();
//    g_pit.initialize();
//
//    g_scheduler.schedule();
// ----------------------------------------------------------------------------------------------
    //g_init_process = (Process *)kmalloc(sizeof(Process), alignof(Process));
    //new ((void *)g_init_process) Process(init_process_user, false, "init process user", false, "/");
    //g_scheduler.add_to_queue(g_init_process);

    //g_ps2_keyboard.initialize();
    //g_pit.initialize();

    //g_scheduler.schedule();
// ----------------------------------------------------------------------------------------------
    char argv_buf[] = {
        'a', 'r', 'g', '0', 0,
        'a', 'r', 'g', '1', 0,
        'a', 'r', 'g', '2', 0,
    };
    char *argv[] = {
        argv_buf + 0,
        argv_buf + 5,
        argv_buf + 10,
        0
    };
    const char *init_path = "/userspace/sh";
    g_init_process = (Process *)kmalloc(sizeof(Process), alignof(Process));
    new ((void *)g_init_process) Process(init_path, argv, false, false, "/");
    g_scheduler.add_to_queue(g_init_process);

    g_ps2_keyboard.initialize();
    g_pit.initialize();

    g_scheduler.schedule();
// ----------------------------------------------------------------------------------------------
    __builtin_unreachable();
    UNREACHABLE();

    // TODO move memory allocation stuff to own file(s)

    // TODO now that dynamic memory allocation is set up:
    //  - dynamically allocated types are possible (strings, vectors, etc.)
    //  - operator new & delete can be set up
    //  - allocations larger than 4KB are possible 
    //    (this of course could be done via a phys page allocator, but the current one does not support this)

    // TODO setup CPU floating point state

    // TODO setup IST stack for double fault handler?

    // TODO add guard page to bottom of kernel stack (and add double fault handler which uses IST stack to print debug message)

    // TODO scrub bytes for kmalloc() and kfree()

    // TODO make funciton to print to serial & vga from one function

    // TODO (for all structs/classes) switch to convention of prefixing member variables with m_

    // TODO make GRUB skip the os selection menu

    // TODO add counter to vspace allocator & phys page allocator to easily & efficiently track memory usage


    // TODO - on context switch, must set tss.rsp0 (set_cpl0_stack()) to the current value of rsp, to handle stack switches on iterrupts
    //        properly
    //      - use LSS instruction for stack switching as advised by AMD manual

}
