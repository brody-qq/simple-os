#pragma once
#include "kernel/types.h"
#include "kernel/debug.cpp"
#include "kernel/scheduler.h"

// based on code from SerenityOS (Kernel/Arch/x86_64/Descriptor)
union __attribute__((__packed__)) SegmentDescriptor
{
    struct
    {
        u32 limit1 : 16;
        u32 base_addr1 : 16;

        u32 base_addr2 : 8;
// the interpretation of the 4 type bits depends on the S bit and the highest type bit (code/data segment bit)
        u32 type : 4;
 // S flag, if set to 0, means that this is a system segment, otherwise it's a code/data segment
        u32 code_or_data_segment : 1;
        u32 dpl : 2;
        u32 present : 1; // P flag
        u32 limit2 : 4;
        u32 _ignored : 1; // ignored by CPU, usable by system software
// if set to 1, this contains 64-bit code, should be set to 0 for 64-bit data segments
        u32 long_mode_code_segment : 1; 
        u32 db : 1; // this is only relevant for 32-bit code, should always be set to 0 for this OS
        u32 limit_granularity : 1; // granularity, if set to 1, the limit is interpreted as a count of pages instead of bytes
        u32 base_addr3 : 8;
    } bitfield;
    
    struct
    {
        u32 low;
        u32 high;
    } dwords;
    
    //u64 raw;

    // TODO make this an enum outside the class, the TSS types overlap with the TSS type
    static const u8 LDT = 0b0010;
    static const u8 TSS64_AVAILABLE = 0b1001;
    static const u8 TSS64_BUSY = 0b1011;
    static const u8 CALL_GATE64 = 0b1100;
    static const u8 INTERRUPT_GATE64 = 0b1110;
    static const u8 TRAP_GATE64 = 0b1111;

    static const u8 DATA_READ_WRITE = 0b0010;
    static const u8 DATA_READ_ONLY = 0b0000;
    static const u8 NONCONFORMING_CODE_EXECUTE_ONLY = 0b1000;
    static const u8 NONCONFORMING_CODE_EXECUTE_READ = 0b1010;

    u64 get_base_addr()
    {
        u64 addr = bitfield.base_addr1;
        addr |= (bitfield.base_addr2 << 16);
        addr |= (bitfield.base_addr3 << 24);
        return addr;
    }
    void set_base_addr(u64 addr)
    {
        bitfield.base_addr1 = addr & 0xffff;
        bitfield.base_addr2 = (addr >> 16) & 0xff;
        bitfield.base_addr3 = (addr >> 24) & 0xff;
    }

    u64 get_limit()
    {
        u64 limit = bitfield.limit1;
        limit |= (bitfield.limit2 << 16);
        return limit;
    }
    void set_limit(u64 limit)
    {
        bitfield.limit1 = limit & 0xffff;
        bitfield.limit2 = (limit >> 16) & 0xf;
    }
};


// NOTE: intel manual says these must be stored with alignment (addr % 4 ==2)
//       however this seems to only apply to the memory address you are writing
//       to when using instructions sidt, sgdt, etc.
// NOTE: the name 'pseudo segment descriptor' comes from the Intel manual, AMD
//       manual does not seem to give these a specific name
struct __attribute__((__packed__)) PseudoSegmentDescriptor
{
    u16 limit; // when added to base_addr. this should give the LAST address of the table
    u64 base_addr; // virtual address of GDT or IDT
};

union __attribute__((__packed__)) SegmentSelector
{
    struct 
    {
        u16 index : 13;
        u16 ti : 1;
        u16 rpl : 2;
    } bitfield;
    u16 raw;

    SegmentSelector() : raw(0) {}
    SegmentSelector(u16 val) : raw(val) {}
};

// AMD manual 2 page 377
struct __attribute__((__packed__)) TSS
{
    u32 ignored1 = 0;

// stacks on CPL switches via interrupt gates
    u32 rsp0_low = 0;
    u32 rsp0_high = 0;

    u32 rsp1_low = 0;
    u32 rsp1_high = 0;

    u32 rsp2_low = 0;
    u32 rsp2_high = 0;

    u64 reserved1 = 0;

// IST stacks
    u32 ist1_low = 0;
    u32 ist1_high = 0;

    u32 ist2_low = 0;
    u32 ist2_high = 0;

    u32 ist3_low = 0;
    u32 ist3_high = 0;

    u32 ist4_low = 0;
    u32 ist4_high = 0;

    u32 ist5_low = 0;
    u32 ist5_high = 0;

    u32 ist6_low = 0;
    u32 ist6_high = 0;

    u32 ist7_low = 0;
    u32 ist7_high = 0;

    u64 reserved2 = 0;

// address of I/O permission bitmap
    u16 reserved3 = 0;
    u16 iomap_base_addr = 0;

    void set_cpl0_stack(vaddr stack_addr)
    {
        rsp0_low = (u32)stack_addr;
        rsp0_high = (u32)(stack_addr >> 32);
    }

    vaddr get_cpl0_stack()
    {
        return ((u64)rsp0_high << 32) | (u64)rsp0_low;
    }

    void set_ist1_stack(vaddr stack_addr)
    {
        ist1_low = (u32)stack_addr;
        ist1_high = (u32)(stack_addr >> 32);
    }

    vaddr get_ist1_stack()
    {
        return ((u64)ist1_high << 32) | (u64)ist1_low;
    }
};
static_assert(sizeof(TSS) == 0x68, "TSS is wrong size");


// AMD manual 2 page 102
struct __attribute__((__packed__)) InterruptGate
{
    u16 offset1;
    u16 code_segment_selector;

    struct {
        u8 ist_index : 3;
        u8 zero1 : 5;
    };

    struct {
        u8 type : 4;
        u8 zero2 : 1;
        u8 dpl : 2; // according to osdev wiki, interrupt gate DPL is irrelevant for hardware interrupts, but relevant for software interrupts ('int N' instruction)
        u8 present : 1;
    } type_attr;

    u16 offset2;
    u32 offset3;
    u32 zero3;
    
    static const u8 INTERRUPT_GATE64 = 0b1110;
    static const u8 TRAP_GATE64 = 0b1111;

    InterruptGate()
    {
        offset1 = 0;
        code_segment_selector = 0;
        ist_index = 0;
        zero1 = 0;
        type_attr.type = 0;
        type_attr.zero2 = 0;
        type_attr.dpl = 0;
        type_attr.present = 0;
        offset2 = 0;
        offset3 = 0;
        zero3 = 0;
    }

    InterruptGate(u64 offset, u16 code_segment, u8 ist, u8 type, u8 dpl)
    {
        offset1 = (u16)offset;
        offset2 = (u16)(offset >> 16);
        offset3 = (u32)(offset >> 32);

        code_segment_selector = code_segment;

        ASSERT(ist < 8);
        ist_index = ist;

        type_attr.dpl = dpl;
        type_attr.present = 1;
        ASSERT(type == INTERRUPT_GATE64 || type == TRAP_GATE64);
        type_attr.type = type;

        zero1 = 0;
        type_attr.zero2 = 0;
        zero3 = 0;
    }
};
static_assert(sizeof(InterruptGate) == 16, "64 bit interrupt gates must be 16 bytes large");

typedef void (*interrupt_handler)();

union __attribute__((__packed__)) RFlags
{
    struct
    {
        u32 carry : 1; // 0
        u32 _reserved1 : 1; // intel manual says this is reserved and will always be set to 1
        u32 parity : 1;
        u32 _reserved2 : 1;
        u32 auxiliary : 1;
        u32 _reserved3 : 1;
        u32 zero : 1; // 6
        u32 sign : 1;
        u32 trap : 1;
        u32 interrupt : 1;
        u32 direction : 1; // 10
        u32 overflow : 1;
        u32 iopl : 2;
        u32 nested_task : 1; // 14
        u32 _reserved4 : 1;
        u32 resume : 1;
        u32 virtual_8086_mode : 1;
        u32 alignment : 1; // 18
        u32 virtual_interrupt : 1;
        u32 virtual_interrupt_pending : 1;
        u32 id : 1; // 21
        u32 _reserved5 : 10; // 22:31
        u32 _reserved6;
    } bitfield;
    u64 raw;

    RFlags() : raw(2) {}
    RFlags(u64 val) : raw(val)
    {
        ASSERT(val & 2); // bit 1 should always be set
    }
};
static_assert(sizeof(RFlags) == 8, "RFlags should be 8 bytes large");

// from SerenityOS
// AMD manual 2 page 284
// TODO can this be re-used on context switches?
// TODO separate the interr
// NOTE: this assumes the interrupt does not modify the floating point registers
// TODO what about ds, es, gs, fs
struct __attribute__((__packed__)) RegisterState
{
    RFlags rflags;
    u64 rdi;
    u64 rsi;

    u64 rbp;
    u64 rsp; // TODO is this redundant because of hte stack frame rsp?

    u64 rbx;
    u64 rdx;
    u64 rcx;
    u64 rax;
    u64 r8;
    u64 r9;
    u64 r10;
    u64 r11;
    u64 r12;
    u64 r13;
    u64 r14;
    u64 r15;
};
// TODO is this assert for % 16 == 0 necessary?
//static_assert(sizeof(RegisterState) % 16 == 0, "if RegisterState is not a multiple of 16, then the interrupt handler stack will not be aligned on 16 byte boundary (see EH_ENTRY code)");
static_assert(sizeof(RegisterState) == 136, "If this changes, then the #define below this must also be changed");
#define REGISTER_STATE_SIZE 136

struct __attribute__((packed)) InterruptStackFrame
{
    u32 error_code;
    u32 padding;
    u64 rip;
    u64 cs;
    u64 rflags;
    u64 rsp;
    u64 ss;
};

struct Process;
void kill_process(Process *) __attribute__((__used__));
