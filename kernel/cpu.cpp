#pragma once
#include "kernel/cpu.h"
#include "kernel/types.h"
#include "kernel/debug.cpp"
#include "kernel/pic.cpp"
#include "kernel/operator_new.cpp"
#include "kernel/kmalloc.h"
#include "include/syscall.h"
#include "include/stdlib_workaround.h"
#include "kernel/ps2_keyboard.h"
#include "kernel/circular_buffer.h"
#include "kernel/tty.cpp"
#include "kernel/ext2.cpp"

// interrupt code is loosely based on https://github.com/SerenityOS/serenity/blob/master/Kernel/Arch/x86_64/Interrupts.cpp

// CPU setup & CPU data structure in this file & cpu.h is based on info from the following:
//  https://github.com/SerenityOS/serenity/blob/master/Kernel/Arch/x86_64/Processor.cpp
//  https://github.com/SerenityOS/serenity/blob/master/Kernel/Arch/x86_64/DescriptorTable.h
//  https://github.com/SerenityOS/serenity/blob/master/Kernel/Arch/x86_64/RegisterState.h
//  https://wiki.osdev.org/GDT_Tutorial
//  https://wiki.osdev.org/Global_Descriptor_Table
//  https://wiki.osdev.org/Task_State_Segment
//  and the AMD & Intel manuals

extern Scheduler g_scheduler;
extern PS2Keyboard g_ps2_keyboard;

SegmentSelector GDT_CODE0;
SegmentSelector GDT_CODE3;
SegmentSelector GDT_DATA0;
SegmentSelector GDT_DATA3;
SegmentSelector GDT_TSS_PART1;
SegmentSelector GDT_TSS_PART2;

// according to AMD manual GDT should be aligned on a quadword boundary for performance
// reasons
alignas(64) SegmentDescriptor gdt[256];
u64 gdt_length = 0;

PseudoSegmentDescriptor gdtr;
void flush_gdt(PseudoSegmentDescriptor descriptor)
{
    asm volatile("lgdt %0" : : "m"(descriptor) : "memory");
}

void write_ds(u16 val)
{
    asm volatile("mov %%ax, %%ds" : : "a"(val));
}

void write_task_register(u16 tss_selector)
{
    asm volatile("ltr %0" : : "r"(tss_selector));
}

TSS tss;

void init_gdt()
{
    // according to osdev:
    //  - code segments should have flags 0b1010 (g=1, db=0, l=1)
    //  - data segments should have flags 0b1100 (g=1, db=1, l=0)
    // 
    // according to AMD manuals:
    //  - for code segments, all except for P, DPL, L, C and D are ignored in long mode
    //    and D must = 0 if L = 1
    //  - for data segments, all except for P are ignored in long mode, privelege checks are done
    //    via the code segment DPL and the page permissions
    // 
    // SerenityOS seems to follow the osdev recommendations, so test this out...

    // NOTE 
    //  - segment limit & granularity flag is relevant on 64 bit system segment descriptors
    //  - IDT entires are 16-bytes large on x64
    //  - TSS descriptors are 16-bytes large on x64 (note that the GDT index always assumes that entries are 8 bytes, so the 16-byte entries must be accounted for manually when indexing the GDT
    //  - LGDT and LIDT use PseudoSegmentDescriptor type, all other segment regsiters take a SegmentSelector (u16)

    GDT_CODE0.raw = 0x08;
    GDT_CODE3.raw = 0x10 | 0x3;
    GDT_DATA0.raw = 0x18;
    GDT_DATA3.raw = 0x20 | 0x3;
    GDT_TSS_PART1.raw = 0x28;
    GDT_TSS_PART2.raw = 0x30;

    SegmentDescriptor code0;
    code0.dwords.low = 0;
    code0.dwords.high = 0;
    code0.set_base_addr(0);
    code0.set_limit(0xfffff);
    code0.bitfield.type = SegmentDescriptor::NONCONFORMING_CODE_EXECUTE_READ;
    code0.bitfield.code_or_data_segment = 1;
    code0.bitfield.dpl = 0;
    code0.bitfield.present = 1;
    code0.bitfield.long_mode_code_segment = 1;
    code0.bitfield.db = 0;
    code0.bitfield.limit_granularity = 1;

    SegmentDescriptor code3 = code0;
    code3.bitfield.dpl = 0x3;

    SegmentDescriptor data0;
    data0.dwords.low = 0;
    data0.dwords.high = 0;
    data0.set_base_addr(0);
    data0.set_limit(0xfffff);
    data0.bitfield.type = SegmentDescriptor::DATA_READ_WRITE;
    data0.bitfield.code_or_data_segment = 1;
    data0.bitfield.dpl = 0;
    data0.bitfield.present = 1;
    data0.bitfield.long_mode_code_segment = 0;
    data0.bitfield.db = 0;
    data0.bitfield.limit_granularity = 1;

    SegmentDescriptor data3 = data0;
    data3.bitfield.dpl = 0x3;

    // TSS descriptors are a special case since they are 16-byte descriptors
    // See AMD manual 2 page 100
    SegmentDescriptor tss_part1;
    tss_part1.dwords.low = 0;
    tss_part1.dwords.high = 0;
    tss_part1.set_base_addr((u64)&tss);
    tss_part1.set_limit(sizeof(tss) - 1);
    tss_part1.bitfield.dpl = 0;
    tss_part1.bitfield.present = 1;
    tss_part1.bitfield.code_or_data_segment = 0;
    tss_part1.bitfield.type = SegmentDescriptor::TSS64_AVAILABLE;

    SegmentDescriptor tss_part2;
    tss_part2.dwords.low = ((u64)&tss) >> 32;
    tss_part2.dwords.high = 0;

    // null segment
    gdt[0].dwords.low = 0;
    gdt[0].dwords.high = 0;

    gdt[1] = code0;
    gdt[2] = code3;
    gdt[3] = data0;
    gdt[4] = data3;
    gdt[5] = tss_part1;
    gdt[6] = tss_part2;

    gdt_length = 7;
    gdtr.limit = (gdt_length * 8) -1;
    gdtr.base_addr = (u64)gdt;

    flush_gdt(gdtr);
    dbg_str("before write_task_register()\n");
    write_task_register(GDT_TSS_PART1.raw);
    dbg_str("after write_task_register()\n");

    //write_ds(GDT_DATA0);

    // TODO maybe add IST entries for double fault, MCE, NMI ( https://www.kernel.org/doc/Documentation/x86/kernel-stacks )
    //      for the case where the kernel stack overflows

}

alignas(64) InterruptGate idt[256];

extern "C" {
    u64 g_offset;
}

extern "C" void pre_interrupt_hook()
{
    dbg_str("PRE INTERRUPT HANDLER\n");
}

extern "C" void post_interrupt_hook()
{
    dbg_str("POST INTERRUPT HANDLER\n");
}

#define INTERRUPT_HANDLER_PRE_WITH_CODE         \
    "pushq %r15\n"                              \
    "pushq %r14\n"                              \
    "pushq %r13\n"                              \
    "pushq %r12\n"                              \
    "pushq %r11\n"                              \
    "pushq %r10\n"                              \
    "pushq %r9\n"                               \
    "pushq %r8\n"                               \
    "pushq %rax\n"                              \
    "pushq %rcx\n"                              \
    "pushq %rdx\n"                              \
    "pushq %rbx\n"                              \
    "pushq %rsp\n"                              \
    "pushq %rbp\n"                              \
    "pushq %rsi\n"                              \
    "pushq %rdi\n"                              \
    "pushfq\n"                                  \
    "call pre_interrupt_hook\n"                 \
                                                \
    "movq $g_offset, %rax\n"                    \
    "movq (%rax), %rax\n"                       \
    "subq %rax, %rsp\n"                         \
    "subq %rax, %rbp\n"


#define INTERRUPT_HANDLER_PRE_NO_CODE           \
    "pushq $0x0\n"    /* error code padding */  \
    INTERRUPT_HANDLER_PRE_WITH_CODE


#define INTERRUPT_HANDLER_POST                  \
    "movq $g_offset, %rax\n"                    \
    "movq (%rax), %rax\n"                       \
    "addq %rax, %rsp\n"                         \
    "addq %rax, %rbp\n"                         \
                                                \
    "call post_interrupt_hook\n"                \
    "popfq\n"                                   /* TODO does this interfere with the interrupt mechanism unsetting the IF flag? */ \
    "popq %rdi\n"                               \
    "popq %rsi\n"                               \
    "popq %rbp\n"                               \
    "addq $0x8, %rsp\n"  /* skip restore rsp */ \
    "popq %rbx\n"                               \
    "popq %rdx\n"                               \
    "popq %rcx\n"                               \
    "popq %rax\n"                               \
    "popq %r8\n"                                \
    "popq %r9\n"                                \
    "popq %r10\n"                               \
    "popq %r11\n"                               \
    "popq %r12\n"                               \
    "popq %r13\n"                               \
    "popq %r14\n"                               \
    "popq %r15\n"                               \
    "addq $0x8, %rsp\n"  /* skip error code */  \
    "iretq\n"


// NOTE: __attribute__((naked)) prevents compiler from inserting prologue and epilogue 
//       stack instructions to the function. This is essential since the stack pointer
//       needs to be at exactly the top of the interrupt stack frame when iretq is called.
//       If __attribte__((naked)) is not called, then %ebp gets pushed to the stack which
//       throws this off
#define EXCEPTION_HANDLER_ENTRY_NO_CODE(vec, title)                                                     \
    extern "C" void title##_entry();                                                                    \
    extern "C" void title##_handler(InterruptStackFrame *, RegisterState *, u64) __attribute__((used)); \
    __attribute__((naked)) void title##_entry()                                                         \
    {                                                                                                   \
        asm(                                                                                            \
            INTERRUPT_HANDLER_PRE_NO_CODE                                                               \
                                                                                                        \
            "lea " STRINGIFY(REGISTER_STATE_SIZE) "(%rsp), %rdi\n" /* 1st arg */                        \
            "movq %rsp, %rsi\n"                                    /* 2nd arg */                        \
            "movq $" #vec ", %rdx\n"                               /* 3rd arg */                        \
            "cld\n"                                                                                     \
            "call " #title "_handler\n"                                                                 \
                                                                                                        \
            INTERRUPT_HANDLER_POST                                                                      \
        );                                                                                              \
    }

#define EXCEPTION_HANDLER_ENTRY_WITH_CODE(vec, title)                                                   \
    extern "C" void title##_entry();                                                                    \
    extern "C" void title##_handler(InterruptStackFrame *, RegisterState *, u64) __attribute__((used)); \
    __attribute__((naked)) void title##_entry()                                                         \
    {                                                                                                   \
        asm(                                                                                            \
            INTERRUPT_HANDLER_PRE_WITH_CODE                                                             \
                                                                                                        \
            "lea " STRINGIFY(REGISTER_STATE_SIZE) "(%rsp), %rdi\n" /* 1st arg */                        \
            "movq %rsp, %rsi\n"                                    /* 2nd arg */                        \
            "movq $" #vec ", %rdx\n"                               /* 3rd arg */                        \
            "cld\n"                                                                                     \
            "call " #title "_handler\n"                                                                 \
                                                                                                        \
            INTERRUPT_HANDLER_POST                                                                      \
        );                                                                                              \
    }

#define GENERIC_INTERRUPT_HANDLER_ENTRY(vec)                                                                      \
    extern "C" void isr_entry_##vec();                                                                            \
    extern "C" void generic_interrupt_handler(InterruptStackFrame *, RegisterState *, u64) __attribute__((used)); \
    __attribute__((naked)) void isr_entry_##vec()                                                                 \
    {                                                                                                             \
        asm(                                                                                                      \
            INTERRUPT_HANDLER_PRE_NO_CODE                                                                         \
                                                                                                                  \
            "lea " STRINGIFY(REGISTER_STATE_SIZE) "(%rsp), %rdi\n" /* 1st arg */                                  \
            "movq %rsp, %rsi\n"                                    /* 2nd arg */                                  \
            "movq $" #vec ", %rdx\n"                               /* 3rd arg */                                  \
            "cld\n"                                                                                               \
            "call generic_interrupt_handler\n"                                                                    \
                                                                                                                  \
            INTERRUPT_HANDLER_POST                                                                                \
        );                                                                                                        \
    }

EXCEPTION_HANDLER_ENTRY_NO_CODE(0x0, divide_by_zero);
void divide_by_zero_handler([[maybe_unused]] InterruptStackFrame *stack_frame, [[maybe_unused]] RegisterState *regs, [[maybe_unused]] u64 vector)
{
    dbg_str("in divide_by_zero()\n");
    vga_print("in divide_by_zero()\n");
    UNREACHABLE();
}

EXCEPTION_HANDLER_ENTRY_NO_CODE(0x1, debug_exception);
void debug_exception_handler([[maybe_unused]] InterruptStackFrame *stack_frame, [[maybe_unused]] RegisterState *regs, [[maybe_unused]] u64 vector)
{
    dbg_str("in debug_exception_handler()\n");
    vga_print("in debug_exception_handler()\n");
    UNREACHABLE();
}

EXCEPTION_HANDLER_ENTRY_NO_CODE(0x2, non_maskable_interrupt);
void non_maskable_interrupt_handler([[maybe_unused]] InterruptStackFrame *stack_frame, [[maybe_unused]] RegisterState *regs, [[maybe_unused]] u64 vector)
{
    dbg_str("in non_maskable_interrupt()\n");
    vga_print("in non_maskable_interrupt()\n");
    UNREACHABLE();
}

EXCEPTION_HANDLER_ENTRY_NO_CODE(0x3, breakpoint_exception);
void breakpoint_exception_handler([[maybe_unused]] InterruptStackFrame *stack_frame, [[maybe_unused]] RegisterState *regs, [[maybe_unused]] u64 vector)
{
    dbg_str("in breakpoint_exception()\n");
    vga_print("in breakpoint_exception()\n");
    UNREACHABLE();
}

EXCEPTION_HANDLER_ENTRY_NO_CODE(0x4, overflow_exception);
void overflow_exception_handler([[maybe_unused]] InterruptStackFrame *stack_frame, [[maybe_unused]] RegisterState *regs, [[maybe_unused]] u64 vector)
{
    dbg_str("in overflow_exception()\n");
    vga_print("in overflow_exception()\n");
    UNREACHABLE();
}

EXCEPTION_HANDLER_ENTRY_NO_CODE(0x5, bound_range);
void bound_range_handler([[maybe_unused]] InterruptStackFrame *stack_frame, [[maybe_unused]] RegisterState *regs, [[maybe_unused]] u64 vector)
{
    dbg_str("in bound_range()\n");
    vga_print("in bound_range()\n");
    UNREACHABLE();
}

EXCEPTION_HANDLER_ENTRY_NO_CODE(0x6, invalid_opcode);
void invalid_opcode_handler([[maybe_unused]] InterruptStackFrame *stack_frame, [[maybe_unused]] RegisterState *regs, [[maybe_unused]] u64 vector)
{
    dbg_str("in invalid_opcode()\n");
    vga_print("in invalid_opcode()\n");
    UNREACHABLE();
}

EXCEPTION_HANDLER_ENTRY_NO_CODE(0x7, device_not_available);
void device_not_available_handler([[maybe_unused]] InterruptStackFrame *stack_frame, [[maybe_unused]] RegisterState *regs, [[maybe_unused]] u64 vector)
{
    dbg_str("in device_not_available()\n");
    vga_print("in device_not_available()\n");
    UNREACHABLE();
}

EXCEPTION_HANDLER_ENTRY_WITH_CODE(0x8, double_fault);
void double_fault_handler([[maybe_unused]] InterruptStackFrame *stack_frame, [[maybe_unused]] RegisterState *regs, [[maybe_unused]] u64 vector)
{
    dbg_str("in double_fault()\n");
    vga_print("in double_fault()\n");
    UNREACHABLE();
}

EXCEPTION_HANDLER_ENTRY_WITH_CODE(0xa, invalid_tss);
void invalid_tss_handler([[maybe_unused]] InterruptStackFrame *stack_frame, [[maybe_unused]] RegisterState *regs, [[maybe_unused]] u64 vector)
{
    dbg_str("in invalid_tss()\n");
    vga_print("in invalid_tss()\n");
    UNREACHABLE();
}

EXCEPTION_HANDLER_ENTRY_WITH_CODE(0xb, segment_not_present);
void segment_not_present_handler([[maybe_unused]] InterruptStackFrame *stack_frame, [[maybe_unused]] RegisterState *regs, [[maybe_unused]] u64 vector)
{
    dbg_str("in segment_not_present()\n");
    vga_print("in segment_not_present()\n");
    UNREACHABLE();
}

EXCEPTION_HANDLER_ENTRY_WITH_CODE(0xc, stack_fault);
void stack_fault_handler([[maybe_unused]] InterruptStackFrame *stack_frame, [[maybe_unused]] RegisterState *regs, [[maybe_unused]] u64 vector)
{
    dbg_str("in stack_fault()\n");
    vga_print("in stack_fault()\n");
    UNREACHABLE();
}

EXCEPTION_HANDLER_ENTRY_WITH_CODE(0xd, general_protection_fault);
void general_protection_fault_handler([[maybe_unused]] InterruptStackFrame *stack_frame, [[maybe_unused]] RegisterState *regs, [[maybe_unused]] u64 vector)
{
    dbg_str("in general_protection_fault()\n");
    vga_print("in general_protection_fault()\n");
    UNREACHABLE();
}

EXCEPTION_HANDLER_ENTRY_WITH_CODE(0xe, page_fault);
void page_fault_handler([[maybe_unused]] InterruptStackFrame *stack_frame, [[maybe_unused]] RegisterState *regs, [[maybe_unused]] u64 vector)
{
    dbg_str("in page_fault()\n");
    vga_print("in page_fault()\n");
    UNREACHABLE();
}

EXCEPTION_HANDLER_ENTRY_NO_CODE(0x10, x87_exception_pending);
void x87_exception_pending_handler([[maybe_unused]] InterruptStackFrame *stack_frame, [[maybe_unused]] RegisterState *regs, [[maybe_unused]] u64 vector)
{
    dbg_str("in x87_exception_pending()\n");
    vga_print("in x87_exception_pending()\n");
    UNREACHABLE();
}

EXCEPTION_HANDLER_ENTRY_WITH_CODE(0x11, alignment_check);
void alignment_check_handler([[maybe_unused]] InterruptStackFrame *stack_frame, [[maybe_unused]] RegisterState *regs, [[maybe_unused]] u64 vector)
{
    dbg_str("in alignment_check()\n");
    vga_print("in alignment_check()\n");
    UNREACHABLE();
}

EXCEPTION_HANDLER_ENTRY_NO_CODE(0x12, machine_check_exception);
void machine_check_exception_handler([[maybe_unused]] InterruptStackFrame *stack_frame, [[maybe_unused]] RegisterState *regs, [[maybe_unused]] u64 vector)
{
    dbg_str("in machine_check_exception()\n");
    vga_print("in machine_check_exception()\n");
    UNREACHABLE();
}

EXCEPTION_HANDLER_ENTRY_NO_CODE(0x13, simd_exception);
void simd_exception_handler([[maybe_unused]] InterruptStackFrame *stack_frame, [[maybe_unused]] RegisterState *regs, [[maybe_unused]] u64 vector)
{
    dbg_str("in simd_exception()\n");
    vga_print("in simd_exception()\n");
    UNREACHABLE();
}

EXCEPTION_HANDLER_ENTRY_WITH_CODE(0x15, control_protection);
void control_protection_handler([[maybe_unused]] InterruptStackFrame *stack_frame, [[maybe_unused]] RegisterState *regs, [[maybe_unused]] u64 vector)
{
    dbg_str("in control_protection()\n");
    vga_print("in control_protection()\n");
    UNREACHABLE();
}

// TODO should the used exception handlers also go through the generic interrupt handler??

GENERIC_INTERRUPT_HANDLER_ENTRY(0x9);
GENERIC_INTERRUPT_HANDLER_ENTRY(0xf);
GENERIC_INTERRUPT_HANDLER_ENTRY(0x14);
GENERIC_INTERRUPT_HANDLER_ENTRY(0x16);
GENERIC_INTERRUPT_HANDLER_ENTRY(0x17);
GENERIC_INTERRUPT_HANDLER_ENTRY(0x18);
GENERIC_INTERRUPT_HANDLER_ENTRY(0x19);
GENERIC_INTERRUPT_HANDLER_ENTRY(0x1a);
GENERIC_INTERRUPT_HANDLER_ENTRY(0x1b);
GENERIC_INTERRUPT_HANDLER_ENTRY(0x1c);
GENERIC_INTERRUPT_HANDLER_ENTRY(0x1d);
GENERIC_INTERRUPT_HANDLER_ENTRY(0x1e);
GENERIC_INTERRUPT_HANDLER_ENTRY(0x1f);
void unused_exception_handler([[maybe_unused]] InterruptStackFrame *stack_frame, [[maybe_unused]] RegisterState *regs, [[maybe_unused]] u64 vector)
{
    dbg_str("unused_interupt_handler() called\ninterrupt vector: "); dbg_uint(vector); dbg_str("\n");
    //UNREACHABLE();
}

void _yield([[maybe_unused]] InterruptStackFrame *stack_frame,
            [[maybe_unused]] RegisterState *regs,
            bool called_by_timer)
{
    dbg_str("YIELD\n");
    current_process()->save_register_state(stack_frame, regs);
    if(called_by_timer)
        g_pic.eoi(0); // TODO hardcoded timer IRQ number
    g_scheduler.schedule();
    __builtin_unreachable();
    UNREACHABLE();
}

void kill_process(Process *proc)
{
    dbg_str("in kill_process()\n");
    proc->exit();
    g_scheduler.remove_from_queue(proc);
    unlink_proc_siblings(proc);
    proc->~Process();
    kfree((vaddr)proc);

    g_scheduler.schedule();
    __builtin_unreachable();
    UNREACHABLE();
}

void create_process(process_entry_ptr fn_ptr, u64 flags, const char *name, bool is_kernel_process)
{
    bool is_blocking = (flags & EXEC_IS_BLOCKING) != 0;
    bool can_be_orphaned = (flags & EXEC_CAN_BE_ORPHANED) != 0;

    void *mem = (void *)kmalloc(sizeof(Process), alignof(Process));
    Process *new_proc = new (mem) Process(fn_ptr, can_be_orphaned, name, is_kernel_process, current_process()->m_pwd);
    g_scheduler.add_to_queue(new_proc);
    // TODO parent process must track children so children can be orphaned or exited if parent exits early
    new_proc->parent = current_process();
    new_proc->parent->add_child(new_proc);
    if(is_blocking) {
        new_proc->parent->add_blocker_process(new_proc->pid);
    }
}

// TODO move this somewhere else?
bool g_in_syscall_context = false;
void create_kernel_process(process_entry_ptr fn_ptr, u64 flags, const char *name)
{
    // I am not sure how nested syscalls will behave, so avoid it for now
    // this function should have only been called from kernel processes anyways, not from
    // syscalls
    ASSERT(!g_in_syscall_context); 

    create_process(fn_ptr, flags, name, true);
    if(flags & EXEC_IS_BLOCKING) {
        sys_yield();
    }
}
void create_user_process(process_entry_ptr fn_ptr, u64 flags, const char *name)
{
    create_process(fn_ptr, flags, name, false);
}

// TODO move this
int max_normalized_path_len(char *src)
{
    return strlen_workaround(src) +1; // the +1 is to account for a '/' being added to the end
}
// TODO this can be used in ext2.cpp lookup_path() to make it require less inode traversal
bool normalize_path(char *dest, char *src)
{
    dbg_str("NORMALIZEPATH: "); dbg_str(src); dbg_str("\n");
    ASSERT(src[0] == '/'); // this function assumes an absolute path input
    struct Pair
    {
        char *start;
        char *end;
    };
    Vector<Pair> parts{5};

    int path_len = strlen_workaround(src);
    bool path_needs_normalizing = false;

    char *ptr = src;
    while(*ptr) {
        while(*ptr && *ptr == '/') { ptr++; }
        char *part_end = ptr;
        while(*part_end && *part_end != '/') { part_end++; }
        if(part_end - ptr == 0)
            break;

        if(((part_end - ptr) == 2) && strncmp_workaround(ptr, "..", 2) == 0) {
            if(parts.length > 0)
                parts.unstable_remove(parts.length-1);
            path_needs_normalizing = true;
            dbg_str(" removed part: "); dbg_str(ptr); dbg_str(" parts.length: "); dbg_uint(parts.length);

        } else if(((part_end - ptr) == 1) && strncmp_workaround(ptr, ".", 1) == 0) {
            path_needs_normalizing = true;

        } else {
            parts.append({ptr, part_end});
            dbg_str(" added part: "); dbg_str(ptr); dbg_str(" parts.length: "); dbg_uint(parts.length);
            
        }

        ptr = part_end;
    }

    if(!path_needs_normalizing)
        return false;

    char *normalize_path = dest;
    int i = 0;
    normalize_path[i++] = '/';
    if(parts.length == 0)
        return true;

    for(int parts_i = 0; parts_i < (int)parts.length; ++parts_i) {
        dbg_str(" parts_i: "); dbg_uint(parts_i); dbg_str(" i: "); dbg_uint(i); dbg_str("\n");
        auto part = parts[parts_i];
        int part_len = part.end - part.start;
        memmove_workaround(normalize_path + i, part.start, part_len);
        i += part_len;
        normalize_path[i++] = '/';
    }
    ASSERT(i <= path_len);
    normalize_path[i-1] = 0; // remove trailing '/' before running fs_stat, cause otherwise ext2 lookup code will assume the path is a directory
    dbg_str("NORMALIZE PATH 2: "); dbg_str(normalize_path); dbg_str("\n");
    auto res = fs_stat(normalize_path);
    if(res.found_file && res.is_dir) {
        normalize_path[i-1] = '/';
        normalize_path[i] = 0;
    }
    dbg_str("NORMALIZE PATH 3: "); dbg_str(normalize_path); dbg_str("\n");

    return true;
}

bool should_normalize(char *path)
{
    char *ptr = path;
    while(*ptr) {
        bool first_slash = true;
        while(*ptr && *ptr == '/') {
            ptr++;
            if(!first_slash)
                return true;
            first_slash = false;
        }

        char *part_end = ptr;

        while(*part_end && *part_end != '/') { part_end++; }

        if(part_end - ptr == 0)
            break;

        if(((part_end - ptr) == 2) && strncmp_workaround(ptr, "..", 2) == 0) {
            return true;
        } else if(((part_end - ptr) == 1) && strncmp_workaround(ptr, ".", 1) == 0) {
            return true;
        }

        ptr = part_end;
    }
    return false;
}

char *kmalloc_and_normalize_path(const char *pwd, const char *path)
{
    dbg_str("PWD: "); dbg_str(pwd); dbg_str("\n");
    dbg_str("PATH: "); dbg_str(path); dbg_str("\n");
    int path_len = abs_path_len(pwd, path);
    char *path_buf = (char *)kmalloc(path_len+1, 64);
    resolve_path(pwd, path, path_buf, path_len+1);

    dbg_str("ORIGINAL PATH: "); dbg_str(path_buf); dbg_str("\n");
    if(should_normalize(path_buf)) {
        int max_normalized_len = max_normalized_path_len(path_buf);
        char *normalized_path_buf = (char *)kmalloc(max_normalized_len, 64);

        normalize_path(normalized_path_buf, path_buf);
        kfree((vaddr)path_buf);
        path_buf = normalized_path_buf;
        path_len = strlen_workaround(normalized_path_buf);
    }
    dbg_str("NORMALIZED PATH: "); dbg_str(path_buf); dbg_str("\n");
    return path_buf;
}

// TODO move to own file
extern CircularBuffer<KeyEvent, 256> g_key_events;
void syscall_handler([[maybe_unused]] InterruptStackFrame *stack_frame,
                     [[maybe_unused]] RegisterState *regs,
                     u64 syscall_num, // rcx
                     u64 arg1, // rdx
                     u64 arg2, // r8 
                     u64 arg3, // r9
                     u64 arg4) // r10
                     //u64 arg5) // r11
{
    ASSERT(current_process()); // this handler should only be called from a process
    ASSERT(!g_in_syscall_context);
    g_in_syscall_context = true;

    dbg_str("IN SYSCALL FOR PROCESS: "); dbg_str(current_process()->name); dbg_str("\n");

    switch(syscall_num)
    {
        case SYSCALL_YIELD:
        {
            dbg_str("SYSCALL YIELD\n");
            // we are in process vspace
            _yield(stack_frame, regs, false);
            __builtin_unreachable();
            UNREACHABLE();
        } break;

        // TODO how to handle process return codes
        // NOTE syscall exit uses a separate stack, since otherwise the process would have to
        //      kfree() it's own kernel stack while in the middle of using that stack
        case SYSCALL_EXIT:
        {
            dbg_str("SYSCALL EXIT\n");
            Process *curr = current_process();
            kill_process(curr);
            __builtin_unreachable();
            UNREACHABLE();
        } break;

        // TODO make distinction between blocking & non-blocking exec
        case SYSCALL_EXEC:
        {
            dbg_str("SYSCALL EXEC\n");
            const char *path = (const char *)arg1;
            char **argv = (char **)arg2;
            u64 flags = arg3;

// TODO this will fail if the pointer causes a pagefault

            // TODO this is identical to the code for SYSCALL_SET_PWD
            const char *pwd = current_process()->get_pwd();
            char *path_buf = kmalloc_and_normalize_path(pwd, path);

            regs->rax = SYS_SUCCESS;
            bool error = false;

            auto res = fs_stat(path_buf);
            if(!res.found_file) {
                regs->rax = SYS_FILE_NOT_FOUND;
                error = true;
            } else if(res.is_dir) {
                regs->rax = SYS_BAD_PATH;
                error = true;
            }

            Process *new_proc;
            if(!error) {
                new_proc = (Process *)kmalloc(sizeof(Process), alignof(Process));
                new ((void *)new_proc) Process(path_buf, argv, false, false, pwd);
                g_scheduler.add_to_queue(new_proc);
            }

            kfree((vaddr)path_buf);

            if(!error) {
                new_proc->parent = current_process();
                new_proc->parent->add_child(new_proc);
                if(flags & EXEC_IS_BLOCKING) {
                    new_proc->parent->add_blocker_process(new_proc->pid);
                }

                if(flags & EXEC_IS_BLOCKING) {
                    _yield(stack_frame, regs, false);
                    __builtin_unreachable();
                    UNREACHABLE();
                }
            }
        } break;

        case SYSCALL_PRINT:
        {
            dbg_str("SYSCALL PRINT\n");
            //dbg_str("CURRENT PML4T: "); dbg_uint((u64)current_pml4t());
            char *str = (char *)arg1;
            u64 len_arg = arg2;

// TODO this will fail if the pointer causes a pagefault
            u64 len = min(len_arg, strlen_workaround(str));
            char *buf = (char *)kmalloc(len +1, 64);
            memmove_workaround(buf, str, len);
            buf[len] = 0;

            vga_print(buf);
            // TODO kernel seems to fail on this kfree sometimes because of a bogus value
            //      in buffer_alloc_range.addr. This happens inconsistently
            kfree((vaddr)buf);
        } break;

        case SYSCALL_ALLOC:
        {
            dbg_str("SYSCALL ALLOC\n");
            u64 size = arg1;
            u64 align = arg2;

            Process *curr = current_process();
            vaddr addr = curr->alloc_mem_in_vspace(size, align);

            // return value
            regs->rax = (u64)addr;
        } break;

        case SYSCALL_FREE:
        {
            dbg_str("SYSCALL FREE\n");
            vaddr addr = (vaddr)arg1;

            Process *curr = current_process();
            curr->free_mem_in_vspace(addr);
        } break;

        case SYSCALL_POLL_KEYBOARD:
        {
            dbg_str("SYSCALL POLL KEYBOARD\n");
            // TODO make sure event is a valid pointer since it comes from userspace
            PollKeyboardResult *res = (PollKeyboardResult *)arg1;

            auto pop_res = g_key_events.pop_start();
            if(pop_res.has_obj) {
                res->has_result = true;
                res->event = pop_res.obj;
            } else {
                res->has_result = false;
            }
        } break;

        case SYSCALL_TTY_WRITE:
        {
            dbg_str("SYSCALL TTY WRITE\n");
            const char *str = (const char *)arg1;
            int len = arg2;
            dbg_str(str);
            dbg_str("\n");
            g_tty.write_str(str, len);
        } break;

        case SYSCALL_TTY_INFO:
        {
            dbg_str("SYSCALL TTY INFO\n");
            // TODO this is a user pointer and may be invalid
            TTYInfo *info = (TTYInfo *)arg1;
            info->cmdline_size = g_tty.CMD_LEN;
            info->scrollback_buffer_size = g_tty.SCROLLBACK_BUFFER_SIZE;
        } break;

        case SYSCALL_TTY_SET_CURSOR:
        {
            dbg_str("SYSCALL TTY SET CURSOR\n");
            u32 cursor_i = (u32)arg1;
            g_tty.set_cursor(cursor_i);
        } break;

        case SYSCALL_TTY_SET_CMDLINE:
        {
            dbg_str("SYSCALL TTY SET CMDLINE\n");
            // TODO this is a user pointer and may be invalid
            char *cmdline = (char *)arg1;
            g_tty.set_cmdline(cmdline);
        } break;

        case SYSCALL_TTY_FLUSH:
        {
            dbg_str("SYSCALL TTY FLUSH\n");
            g_tty.flush_to_vga();
        } break;

        case SYSCALL_TTY_SCROLL:
        {
            dbg_str("SYSCALL TTY SCROLL\n");
            auto amount = (int)arg1;

            // TODO move this logic to TTY class
            if(amount < 0)
                g_tty.scroll_down(-amount);
            else
                g_tty.scroll_up(amount);
        } break;

        case SYSCALL_TTY_FLUSH_CMDLINE:
        {
            dbg_str("SYSCALL TTY FLUSH CMDLINE\n");
            g_tty.flush_cmdline();
        } break;

        case SYSCALL_PWD_LENGTH:
        {
            dbg_str("SYSCALL PWD LENGTH\n");
            regs->rax = (u64)current_process()->pwd_length();
        } break;

        case SYSCALL_GET_PWD:
        {
            dbg_str("SYSCALL GET PWD\n");
            // TODO user pointer could fault
            char *dest = (char *)arg1;
            int len = (int)arg2;

            int pwd_len = current_process()->pwd_length();
            const char *pwd = current_process()->get_pwd();
            memmove_workaround(dest, (char *)pwd, min(len, pwd_len));
        } break;

        case SYSCALL_SET_PWD:
        {
            dbg_str("SYSCALL SET PWD\n");
            // TODO user pointer could fault
            char *path = (char *)arg1;

            char *path_buf = kmalloc_and_normalize_path(current_process()->get_pwd(), path);

            regs->rax = SYS_SUCCESS;
            bool error = false;

            auto stat = fs_stat(path_buf);
            if(!stat.found_file) {
                regs->rax = SYS_FILE_NOT_FOUND;
                error = true;
            } else if(!stat.is_dir) {
                regs->rax = SYS_BAD_PATH;
                error = true;
            }

            if(!error) {
                current_process()->set_pwd(path_buf);
            }

            kfree((vaddr)path_buf);
        } break;

        case SYSCALL_STAT:
        {
            dbg_str("SYSCALL STAT\n");
            // TODO user pointers can cause fault
            const char *path = (char *)arg1;
            FileStatResult *result = (FileStatResult *)arg2;

            char *path_buf = kmalloc_and_normalize_path(current_process()->get_pwd(), path);

            *result = fs_stat(path_buf);

            kfree((vaddr)path_buf);
        } break;

        case SYSCALL_LIST_DIR_BUF_SIZE: {
            dbg_str("SYSCALL LIST DIR BUF SIZE\n");
            // TODO user pointer could fault
            const char *path = (char *)arg1;

            char *path_buf = kmalloc_and_normalize_path(current_process()->get_pwd(), path);

            regs->rax = SYS_SUCCESS;
            bool error = false;

            auto stat = fs_stat(path_buf);
            if(!stat.found_file) {
                regs->rax = SYS_FILE_NOT_FOUND;
                error = true;
            } else if(!stat.is_dir) {
                regs->rax = SYS_BAD_PATH;
                error = true;
            }

            if(!error) {
                int result = fs_list_dir_buffer_size(path_buf);
                regs->rax = (u64)result; // TODO make sure this doesnt overlap SYS_ return codes
            }

            kfree((vaddr)path_buf);
        }break;

        case SYSCALL_LIST_DIR: {
            dbg_str("SYSCALL LIST DIR\n");
            // TODO user pointer could fault
            const char *path = (char *)arg1;
            char *buf = (char *)arg2;
            int buf_size = (int)arg3;
            char **buf_one_past_end = (char **)arg4;

            char *path_buf = kmalloc_and_normalize_path(current_process()->get_pwd(), path);

            bool error = false;

            auto stat = fs_stat(path_buf);
            if(!stat.found_file) {
                error = true;
                *buf_one_past_end = nullptr;
            } else if(!stat.is_dir) {
                error = true;
                *buf_one_past_end = nullptr;
            }

            if(!error) {
                fs_list_dir(path_buf, buf, buf_size, buf_one_past_end);
            }

            kfree((vaddr)path_buf);
        }break;

        case SYSCALL_FS_READ: {
            dbg_str("SYSCALL FS READ\n");
            // TODO user pointer could fault
            const char *path = (char *)arg1;
            char *buf = (char *)arg2;
            int offset = (int)arg3;
            int size = (int)arg4;

            char *path_buf = kmalloc_and_normalize_path(current_process()->get_pwd(), path);

            bool error = false;

            auto stat = fs_stat(path_buf);
            if(!stat.found_file) {
                error = true;
                regs->rax = SYS_FILE_NOT_FOUND;
            } else if(stat.is_dir) {
                error = true;
                regs->rax = SYS_BAD_PATH;
            }

            if(!error) {
                regs->rax = fs_read(path_buf, (u8 *)buf, offset, size);
            }

            kfree((vaddr)path_buf);
        }break;

        case SYSCALL_FS_WRITE: {
            dbg_str("SYSCALL FS WRITE\n");
            // TODO user pointer could fault
            const char *path = (char *)arg1;
            char *buf = (char *)arg2;
            int offset = (int)arg3;
            int size = (int)arg4;

            char *path_buf = kmalloc_and_normalize_path(current_process()->get_pwd(), path);

            bool error = false;

            auto stat = fs_stat(path_buf);
            if(!stat.found_file) {
                error = true;
                regs->rax = SYS_FILE_NOT_FOUND;
            } else if(stat.is_dir) {
                error = true;
                regs->rax = SYS_BAD_PATH;
            }

            if(!error) {
                regs->rax = fs_write(path_buf, (u8 *)buf, offset, size);
            }

            kfree((vaddr)path_buf);
        }break;

        case SYSCALL_FS_CREATE_FILE: {
            dbg_str("SYSCALL FS CREATE FILE\n");
            // TODO user pointer could fault
            const char *path = (char *)arg1;

            char *path_buf = kmalloc_and_normalize_path(current_process()->get_pwd(), path);

            bool error = false;

            auto stat = fs_stat(path_buf);
            if(stat.found_file) {
                error = true;
                regs->rax = SYS_FILE_ALREADY_EXISTS;
            }

            if(!error) {
                regs->rax = fs_create(path_buf, FS_TYPE_REG_FILE);
            }

            kfree((vaddr)path_buf);
        }break;

        case SYSCALL_FS_CREATE_DIR: {
            dbg_str("SYSCALL FS CREATE DIR\n");
            // TODO user pointer could fault
            const char *path = (char *)arg1;

            char *path_buf = kmalloc_and_normalize_path(current_process()->get_pwd(), path);

            bool error = false;

            auto stat = fs_stat(path_buf);
            if(stat.found_file) {
                error = true;
                regs->rax = SYS_FILE_ALREADY_EXISTS;
            }

            if(!error) {
                regs->rax = fs_create(path_buf, FS_TYPE_DIR);
            }

            kfree((vaddr)path_buf);
        }break;

        case SYSCALL_FS_RM_FILE: {
            dbg_str("SYSCALL FS RM FILE\n");
            // TODO user pointer could fault
            const char *path = (char *)arg1;

            char *path_buf = kmalloc_and_normalize_path(current_process()->get_pwd(), path);

            bool error = false;

            auto stat = fs_stat(path_buf);
            if(!stat.found_file) {
                error = true;
                regs->rax = SYS_FILE_NOT_FOUND;
            } else if (stat.is_dir) {
                error = true;
                regs->rax = SYS_BAD_PATH;
            }

            if(!error) {
                regs->rax = fs_delete(path_buf);
            }

            kfree((vaddr)path_buf);
        }break;

        case SYSCALL_FS_RM_DIR: {
            dbg_str("SYSCALL FS RM DIR\n");
            // TODO user pointer could fault
            const char *path = (char *)arg1;

            char *path_buf = kmalloc_and_normalize_path(current_process()->get_pwd(), path);

            bool error = false;

            auto stat = fs_stat(path_buf);
            if(!stat.found_file) {
                error = true;
                regs->rax = SYS_FILE_NOT_FOUND;
            } else if (!stat.is_dir) {
                error = true;
                regs->rax = SYS_BAD_PATH;
            }

            if(!error) {
                regs->rax = fs_delete(path_buf);
            }

            kfree((vaddr)path_buf);
        }break;

        case SYSCALL_FS_MV: {
            dbg_str("SYSCALL FS MV\n");
            // TODO user pointer could fault
            const char *src_path = (char *)arg1;
            const char *dst_path = (char *)arg2;

            char *src_path_buf = kmalloc_and_normalize_path(current_process()->get_pwd(), src_path);
            char *dst_path_buf = kmalloc_and_normalize_path(current_process()->get_pwd(), dst_path);

            bool error = false;

            auto stat = fs_stat(src_path_buf);
            if(!stat.found_file) {
                error = true;
                regs->rax = SYS_FILE_NOT_FOUND;
            }

            if(!error) {
                regs->rax = fs_mv(src_path_buf, dst_path_buf);
            }

            kfree((vaddr)src_path_buf);
            kfree((vaddr)dst_path_buf);
        }break;

        case SYSCALL_FS_TRUNC: {
            dbg_str("SYSCALL FS TRUNC\n");
            // TODO user pointer could fault
            char *path = (char *)arg1;
            int new_size = (int)arg2;

            char *path_buf = kmalloc_and_normalize_path(current_process()->get_pwd(), path);

            bool error = false;

            auto stat = fs_stat(path_buf);
            if(!stat.found_file) {
                error = true;
                regs->rax = SYS_FILE_NOT_FOUND;
            } else if(stat.is_dir) {
                error = true;
                regs->rax = SYS_BAD_PATH;
            }

            if(!error) {
                regs->rax = fs_truncate(path_buf, new_size);
            }

            kfree((vaddr)path_buf);
        }break;

        case SYSCALL_FS_IS_SAME_PATH: {
            dbg_str("SYSCALL FS IS SAME PATH\n");
            // TODO user pointer could fault
            const char *path1 = (char *)arg1;
            const char *path2 = (char *)arg2;

            char *path1_buf = kmalloc_and_normalize_path(current_process()->get_pwd(), path1);
            char *path2_buf = kmalloc_and_normalize_path(current_process()->get_pwd(), path2);

            int len1 = strlen_workaround(path1_buf);
            int len2 = strlen_workaround(path2_buf);
            bool is_same = (len1 == len2) && (strncmp_workaround(path1_buf, path2_buf, len1) == 0);
            regs->rax = is_same ? 0 : 1;

            kfree((vaddr)path1_buf);
            kfree((vaddr)path2_buf);
        }break;

        case SYSCALL_FS_IS_DIR_PATH: {
            dbg_str("SYSCALL FS IS DIR PATH\n");
            // TODO user pointer could fault
            const char *path = (char *)arg1;

            char *path_buf = kmalloc_and_normalize_path(current_process()->get_pwd(), path);
            auto stat = fs_stat(path_buf);
            if (stat.found_file) {
                if (stat.is_dir)
                    regs->rax = 0;
                else
                    regs->rax = 1;
            } else {
                int len = strlen_workaround(path_buf);
                regs->rax = (path_buf[len-1] == '/') ? 0 : 1;
            }
            kfree((vaddr)path_buf);
        }break;

        default:
        {
            dbg_str("invalid syscall_num: "); dbg_uint(syscall_num); dbg_str("\n");
            UNREACHABLE();
        } break;
    }

    g_in_syscall_context = false;
}

// PIC interrupt vectors
#define DECLARE_GENERIC_HANDLERS(NUM) \
    GENERIC_INTERRUPT_HANDLER_ENTRY(0x ## NUM ## 0); \
    GENERIC_INTERRUPT_HANDLER_ENTRY(0x ## NUM ## 1); \
    GENERIC_INTERRUPT_HANDLER_ENTRY(0x ## NUM ## 2); \
    GENERIC_INTERRUPT_HANDLER_ENTRY(0x ## NUM ## 3); \
    GENERIC_INTERRUPT_HANDLER_ENTRY(0x ## NUM ## 4); \
    GENERIC_INTERRUPT_HANDLER_ENTRY(0x ## NUM ## 5); \
    GENERIC_INTERRUPT_HANDLER_ENTRY(0x ## NUM ## 6); \
    GENERIC_INTERRUPT_HANDLER_ENTRY(0x ## NUM ## 7); \
    GENERIC_INTERRUPT_HANDLER_ENTRY(0x ## NUM ## 8); \
    GENERIC_INTERRUPT_HANDLER_ENTRY(0x ## NUM ## 9); \
    GENERIC_INTERRUPT_HANDLER_ENTRY(0x ## NUM ## a); \
    GENERIC_INTERRUPT_HANDLER_ENTRY(0x ## NUM ## b); \
    GENERIC_INTERRUPT_HANDLER_ENTRY(0x ## NUM ## c); \
    GENERIC_INTERRUPT_HANDLER_ENTRY(0x ## NUM ## d); \
    GENERIC_INTERRUPT_HANDLER_ENTRY(0x ## NUM ## e); \
    GENERIC_INTERRUPT_HANDLER_ENTRY(0x ## NUM ## f); \

DECLARE_GENERIC_HANDLERS(2);
DECLARE_GENERIC_HANDLERS(3);
DECLARE_GENERIC_HANDLERS(4);
DECLARE_GENERIC_HANDLERS(5);
DECLARE_GENERIC_HANDLERS(6);
DECLARE_GENERIC_HANDLERS(7);
DECLARE_GENERIC_HANDLERS(8);
DECLARE_GENERIC_HANDLERS(9);
DECLARE_GENERIC_HANDLERS(a);
DECLARE_GENERIC_HANDLERS(b);
DECLARE_GENERIC_HANDLERS(c);
DECLARE_GENERIC_HANDLERS(d);
DECLARE_GENERIC_HANDLERS(e);
DECLARE_GENERIC_HANDLERS(f);


// TODO move this somewhere else
u64 g_ticks_since_startup = 0;
void handle_timer_tick([[maybe_unused]] InterruptStackFrame *stack_frame,
                       [[maybe_unused]] RegisterState *regs)
{
    dbg_str("TIMER TICK: "); dbg_uint(g_ticks_since_startup);
    if(current_process()) {
        dbg_str(" PROCESS: "); dbg_str(current_process()->name);
    }
    dbg_str("\n");

    g_ticks_since_startup++;
    if(g_scheduler.tick()) {
        dbg_str("    TIMER TICK SWITCH "); dbg_uint(g_ticks_since_startup); dbg_str("\n");
        g_in_syscall_context = true;
        _yield(stack_frame, regs, true);
        __builtin_unreachable();
        UNREACHABLE();
    }
}

void generic_interrupt_handler([[maybe_unused]] InterruptStackFrame *stack_frame,
                               [[maybe_unused]] RegisterState *regs,
                               [[maybe_unused]] u64 vector)
{
    dbg_str("in generic_interrupt_handler()\ninterrupt vector: "); dbg_uint(vector); dbg_str("\n");

    // TODO make this be a proper table lookup instead of if/else cases
    if(vector >= PIC::BASE_VECTOR && vector < PIC::VECTORS_ONE_PAST_END) {
        u8 irq = vector - PIC::BASE_VECTOR;

        if(g_pic.handle_spurious_interrupt(vector))
            return;

        dbg_str("IRQ: "); dbg_uint(irq); dbg_str("\n");
        switch(irq) {
            case 0: {
                handle_timer_tick(stack_frame, regs);
            } break;
            case 1: {
                g_ps2_keyboard.handle_irq();
            } break;
            default: {
                UNREACHABLE(); // TODO unimplemented dynamic dispatch for PIC interrupts
            } break;
        }

        g_pic.eoi(vector);
    } else if(vector == 0xff) {
        syscall_handler(stack_frame, regs, regs->rcx, regs->rdx, regs->r8, regs->r9, regs->r10);
    } else [[unlikely]] {
        unused_exception_handler(stack_frame, regs, vector);
    }
}

void register_interrupt_handler(u8 index, interrupt_handler handler)
{
    idt[index] = InterruptGate((u64)handler, GDT_CODE0.raw, 1, InterruptGate::INTERRUPT_GATE64, 0);
}

// TODO according to osdev wiki the interrupt gate DPL must be 3 to be callable via 'int' instruction
//      from user mode. Verify if this is true or not after processes are working
void register_user_callable_interrupt_handler(u8 index, interrupt_handler handler)
{
    idt[index] = InterruptGate((u64)handler, GDT_CODE0.raw, 1, InterruptGate::INTERRUPT_GATE64, 3);
}

PseudoSegmentDescriptor idtr;
void flush_idt()
{
    asm volatile("lidt %0" : : "m"(idtr));
}

void init_idt()
{
    // TODO:
    //  trap state & regs state
    //  interrupt entry (incl. account for entries with & without error code)
    //  separate interrupt entry for external interrupts (handle_interrupt())
    //  interrupt exit

    idtr.base_addr = (u64)idt;
    idtr.limit = ARRAY_SIZE(idt) * sizeof(InterruptGate) -1;

    // 0x0 #DE | fault | no ec | divide by 0
    register_interrupt_handler(0x0, divide_by_zero_entry);

    // 0x1 #DB | trap or fault | no ec | debug exception
    register_interrupt_handler(0x1, debug_exception_entry);

    // 0x2 #NMI | NA | no ec | nmi
    register_interrupt_handler(0x2, non_maskable_interrupt_entry);

    // 0x3 #BP | trap | no ec | breakpoint
    register_interrupt_handler(0x3, breakpoint_exception_entry);

    // 0x4 #OF | trap | no ec | overflow exception
    register_interrupt_handler(0x4, overflow_exception_entry);

    // 0x5 #BR | fault | no ec | bound range
    register_interrupt_handler(0x5, bound_range_entry);

    // 0x6 #UD | fault | no ec | invalid opcode
    register_interrupt_handler(0x6, invalid_opcode_entry);

    // 0x7 #NM | fault | no ec | device not available
    register_interrupt_handler(0x7, device_not_available_entry);

    // 0x8 #DF | abort | ec | double fault
    register_interrupt_handler(0x8, double_fault_entry);

    // 0xa #TS | fault | ec | invalid tss
    register_interrupt_handler(0xa, invalid_tss_entry);

    // 0xb #NP | fault | ec | segment not present
    register_interrupt_handler(0xb, segment_not_present_entry);

    // 0xc #SS | fault | ec | stack fault
    register_interrupt_handler(0xc, stack_fault_entry);

    // 0xd #GP | fault | ec | general prtection
    register_interrupt_handler(0xd, general_protection_fault_entry);

    // 0xe #PF | fault | ec | page fault
    register_interrupt_handler(0xe, page_fault_entry);

    // 0x10 #MF | fault | no ec | pending x87 floating point
    register_interrupt_handler(0x10, x87_exception_pending_entry);

    // 0x11 #AC | fault | ec | alignment check
    register_interrupt_handler(0x11, alignment_check_entry);

    // 0x12 #MC | abort | no ec | machine check
    register_interrupt_handler(0x12, machine_check_exception_entry);

    // 0x13 #XF/#XM | fault | no ec | simd exception (NOTE this is called #XF by AMD and #XM by Intel)
    register_interrupt_handler(0x13, simd_exception_entry);

    // 0x15 #CP | fault | ec | control protection
    register_interrupt_handler(0x15, control_protection_entry);

    // unused CPU exceptions in range [0x00, 0x32)
    // to simplify the implementation, these use the logic of generic_interrupt_handler() for unused interrupts
    register_interrupt_handler(0x9, isr_entry_0x9); // 0x9 coprocessor segment overrun, reserved on modern CPUs
    register_interrupt_handler(0xf, isr_entry_0xf);
    register_interrupt_handler(0x14, isr_entry_0x14);  // 0x14 #VE virtualization exception in Intel64
    register_interrupt_handler(0x16, isr_entry_0x16);
    register_interrupt_handler(0x17, isr_entry_0x17);
    register_interrupt_handler(0x18, isr_entry_0x18);
    register_interrupt_handler(0x19, isr_entry_0x19);
    register_interrupt_handler(0x1a, isr_entry_0x1a);
    register_interrupt_handler(0x1b, isr_entry_0x1b);
    register_interrupt_handler(0x1c, isr_entry_0x1c);  // 0x1c #HV hypervisor injection in AMD64
    register_interrupt_handler(0x1d, isr_entry_0x1d);  // 0x1d #VC VMM communication in AMD64
    register_interrupt_handler(0x1e, isr_entry_0x1e);  // 0x1e #SX security exception in AMD64
    register_interrupt_handler(0x1f, isr_entry_0x1f);

    // TODO there's probably a smarter way of doing this...
#define REGISTER_HANDLERS(NUM) \
    register_interrupt_handler(0x ## NUM ## 0, isr_entry_0x ## NUM ## 0); \
    register_interrupt_handler(0x ## NUM ## 1, isr_entry_0x ## NUM ## 1); \
    register_interrupt_handler(0x ## NUM ## 2, isr_entry_0x ## NUM ## 2); \
    register_interrupt_handler(0x ## NUM ## 3, isr_entry_0x ## NUM ## 3); \
    register_interrupt_handler(0x ## NUM ## 4, isr_entry_0x ## NUM ## 4); \
    register_interrupt_handler(0x ## NUM ## 5, isr_entry_0x ## NUM ## 5); \
    register_interrupt_handler(0x ## NUM ## 6, isr_entry_0x ## NUM ## 6); \
    register_interrupt_handler(0x ## NUM ## 7, isr_entry_0x ## NUM ## 7); \
    register_interrupt_handler(0x ## NUM ## 8, isr_entry_0x ## NUM ## 8); \
    register_interrupt_handler(0x ## NUM ## 9, isr_entry_0x ## NUM ## 9); \
    register_interrupt_handler(0x ## NUM ## a, isr_entry_0x ## NUM ## a); \
    register_interrupt_handler(0x ## NUM ## b, isr_entry_0x ## NUM ## b); \
    register_interrupt_handler(0x ## NUM ## c, isr_entry_0x ## NUM ## c); \
    register_interrupt_handler(0x ## NUM ## d, isr_entry_0x ## NUM ## d); \
    register_interrupt_handler(0x ## NUM ## e, isr_entry_0x ## NUM ## e); \
    register_interrupt_handler(0x ## NUM ## f, isr_entry_0x ## NUM ## f);

    REGISTER_HANDLERS(2);
    REGISTER_HANDLERS(3);
    REGISTER_HANDLERS(4);
    REGISTER_HANDLERS(5);
    REGISTER_HANDLERS(6);
    REGISTER_HANDLERS(7);
    REGISTER_HANDLERS(8);
    REGISTER_HANDLERS(9);
    REGISTER_HANDLERS(a);
    REGISTER_HANDLERS(b);
    REGISTER_HANDLERS(c);
    REGISTER_HANDLERS(d);
    REGISTER_HANDLERS(e);
    REGISTER_HANDLERS(f);

    // syscall handler, this must be registered with interrupt gate DPL=3 so that the
    // interrupt can be callable from user mode via the 'int' instruction
    // TODO change this back to using ist=0 when properly implemented userspace
    register_user_callable_interrupt_handler(0xff, isr_entry_0xff);

    flush_idt();
}
