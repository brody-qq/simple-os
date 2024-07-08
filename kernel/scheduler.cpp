#pragma once
#include "kernel/scheduler.h"
#include "kernel/cpu.h"
#include "external/elf_abi.h"
#include "kernel/ext2.cpp"

Scheduler g_scheduler;
extern bool g_in_kernel_init;
extern VSpace g_kernel_vspace;
pid_t s_next_pid = 0;
Process *g_init_process;
extern TSS tss;
extern "C" u64 g_offset;
extern "C" u64 g_kernel_pml4t_ptr;

extern VObject g_interrupt_stack_vobj;
extern Stack g_interrupt_stack;

// numer of timer ticks before a process gets switched out with another process
const u64 TICKS_PER_SLICE = 25;

// TODO this s_block_tick is error prone and messy, can probably be removed since interrupts should be disabled
//      in all of these methods anyways as they are all entered via syscalls or early kernel startup
static bool s_block_tick = true;

// TODO this should be part of a linked list type...
void unlink_proc_queue(Process *proc)
{
    if(proc->queue_next)
        proc->queue_next->queue_prev = proc->queue_prev;

    if(proc->queue_prev)
        proc->queue_prev->queue_next = proc->queue_next;

    proc->queue_next = 0;
    proc->queue_prev = 0;
}
void unlink_proc_siblings(Process *proc)
{
    if(proc->sibling_next)
        proc->sibling_next->sibling_prev = proc->sibling_prev;

    if(proc->sibling_prev)
        proc->sibling_prev->sibling_next = proc->sibling_next;

    proc->sibling_next = 0;
    proc->sibling_prev = 0;
}

void Process::setup_vspace(bool is_kernel_process)
{
    if(is_kernel_process) {
        m_vspace = &g_kernel_vspace;
    } else {
        VRange range = {
            USER_VSPACE_START,
            USER_VSPACE_SIZE
        };
        auto vspace = (VSpace *)kmalloc(sizeof(VSpace), alignof(VSpace));
        new ((void *)vspace) VSpace(range);
        m_vspace = vspace;
        m_vspace->share_kernelspace();
    }
}

Process::Process(process_entry_ptr fn_ptr, bool can_orphan, const char *proc_name, bool kernel_proc, const char *pwd)
    : m_blockers(32),
      start_rip((vaddr)fn_ptr),
      can_be_orphaned(can_orphan),
      pid(++s_next_pid),
      is_kernel_process(kernel_proc),
      proc_stack_vobj(DEFAULT_STACK_SIZE, DEFAULT_STACK_ALIGNMENT)
{
    dbg_str("Process()\n");
    ASSERT(in_kernel_vspace);

    if(!pwd)
        pwd = "/";
    set_pwd(pwd);
    setup_vspace(is_kernel_process);

    u64 len = min(strlen_workaround(proc_name), MAX_PROC_NAME_LEN);
    memmove_workaround(name, const_cast<char *>(proc_name), len);
    memset_workaround(name + len, 0, MAX_PROC_NAME_LEN - len);

    // TODO this should allocate a guard page at bottom of stack
    proc_stack_kspace.bottom = proc_stack_vobj.map(g_kernel_vspace);
    proc_stack_kspace.set_stack_top(DEFAULT_STACK_SIZE, DEFAULT_STACK_ALIGNMENT);

    dbg_str("NAME: "); dbg_str(name);
    dbg_str(" PML4T: "); dbg_uint((u64)m_vspace->m_pml4t);
    dbg_str("\n");
}

// NOTE: this assumes the caller will kmalloc() the exe_path, then this Process object
//       will take ownership of the pointer
Process::Process(const char *exe_path, char **argv, bool can_orphan, bool kernel_proc, const char *pwd)
    : m_blockers(32),
      start_rip(0),
      can_be_orphaned(can_orphan),
      pid(++s_next_pid),
      is_kernel_process(kernel_proc),
      proc_stack_vobj(DEFAULT_STACK_SIZE, DEFAULT_STACK_ALIGNMENT)
{
    dbg_str("Process()\n");
    ASSERT(in_kernel_vspace);

    if(!pwd)
        pwd = "/";
    set_pwd(pwd);
    setup_vspace(is_kernel_process);

    int pathlen = strlen_workaround(exe_path);
    this->exe_path = (char *)kmalloc(pathlen+1, 64);
    memmove_workaround((char *)this->exe_path, (char *)exe_path, pathlen+1);

    u64 len = min(pathlen+1, (int)MAX_PROC_NAME_LEN);
    memmove_workaround(name, (char *)exe_path, len);
    name[len-1] = 0;

    // NOTE: this is done here instead of in user_process_start because otherwise
    //       Process would have to make it's own copy of argv to guarantee it is
    //       available by the time user_process_start is run
    // TODO this should allocate a guard page at bottom of stack
    proc_stack_kspace.bottom = proc_stack_vobj.map(g_kernel_vspace);
    proc_stack_kspace.set_stack_top(DEFAULT_STACK_SIZE, DEFAULT_STACK_ALIGNMENT);
    copy_argv_to_stack(argv);

    dbg_str("NAME: "); dbg_str(name);
    dbg_str(" PML4T: "); dbg_uint((u64)m_vspace->m_pml4t);
    dbg_str("\n");
}

void Process::copy_argv_to_stack(char **src_argv)
{
    ASSERT(m_argv_kspace == 0); // this should only be called on process startup
    int src_argc = 0;
    while(src_argv[src_argc] != 0) { ++src_argc; }
    m_argc = src_argc + 1; // + 1 to account for the process name

    int argv_buf_size = strlen_workaround(name) +1;
    for(int i = 0; i < src_argc; ++i) {
        char *str = src_argv[i];
        int len = strlen_workaround(str) +1;
        argv_buf_size += len;
    }

    proc_stack_argv_len = argv_buf_size + (m_argc+1)*sizeof(char *);
    proc_stack_argv_offset = proc_stack_argv_len -1;

    u64 base_ptr = proc_stack_kspace.top - proc_stack_argv_len;
    m_argv_kspace = (char **)base_ptr;
    char *ptr = (char *)(base_ptr + (m_argc+1)*8);

    int argv_i = 0;
    auto copy_str = [&](char *str) {
        int len = strlen_workaround(str) +1;
        memmove_workaround(ptr, str, len);
        m_argv_kspace[argv_i++] = ptr;
        ptr += len;
    };

    ASSERT(name);
    copy_str(name);
    for(int i = 0; i < src_argc; ++i) {
        char *src = src_argv[i];
        copy_str(src);
    }
    ASSERT((u64)ptr == proc_stack_kspace.top);
    m_argv_kspace[m_argc] = 0;
}

vaddr Process::alloc_mem_in_vspace(u64 size, u64 align)
{
    // TODO allocating like this wastes lots of memory
    VObject *vobj = (VObject *)kmalloc(sizeof(VObject), alignof(VObject));
    new ((void *)vobj) VObject(size, align);

    vaddr addr = vobj->map(*m_vspace);
    vobjs.append({vobj, addr});
    return addr;
}

void Process::free_mem_in_vspace(vaddr addr)
{
    bool found = false;
    u32 i = 0;
    VObjectAllocation v;
    for(i = 0; i < vobjs.length; ++i) {
        v = vobjs[i];
        if(v.alloced_addr == addr) {
            found = true;
            break;
        }
    }
    ASSERT(found);

    v.vobj->unmap(*m_vspace); // TODO unmap() does a 2nd redundant search for addr, make 2nd unmap() call that takes addr
    v.vobj->~VObject();
    kfree((vaddr)v.vobj);
    vobjs.unstable_remove(i);
}

void Process::setup_interrupt_entry()
{
    // NOTE: using ist here since it always forces the interrupt to switch to the ist stack, even if
    //       there is no CPL change. There are no CPL changes for kernel processes, and a stack switch
    //       MUST happen when sys_exit is called since exit() will free the process stacks
    // TODO should GDT or TR be reloaded after modifying TSS?
    tss.set_ist1_stack(interrupt_stack_uspace.top);
    g_offset = interrupt_stack_offset;
}

struct LoadElfResult
{
    VObject *elf_img_vobj;
    u64 start_rip = 0;
};
void Process::user_process_start()
{
    ASSERT(g_in_syscall_context || g_in_kernel_init);
    ASSERT(!is_kernel_process);
    ASSERT(!are_interrupts_enabled());
    dbg_str("user_process_start()\n");

    // it is necessary to load the user vspace before making allocations in that vspace
    write_cr3((u64)m_vspace->m_pml4t); // TODO there is a redundant write to cr3 in switch_context()

    // TODO this should allocate a guard page at bottom of stack
    interrupt_stack_uspace.bottom = g_interrupt_stack_vobj.map(*m_vspace);
    interrupt_stack_uspace.set_stack_top(DEFAULT_STACK_SIZE, DEFAULT_STACK_ALIGNMENT);
    interrupt_stack_offset = interrupt_stack_uspace.top - g_interrupt_stack.top;

    proc_stack_uspace.bottom = proc_stack_vobj.map(*m_vspace);
    proc_stack_uspace.set_stack_top(DEFAULT_STACK_SIZE, DEFAULT_STACK_ALIGNMENT);

    //proc_stack_kspace.bottom = proc_stack_vobj.map(g_kernel_vspace);
    //proc_stack_kspace.set_stack_top(DEFAULT_STACK_SIZE, DEFAULT_STACK_ALIGNMENT);

    proc_stack_offset = proc_stack_uspace.top - proc_stack_kspace.top;

    auto load_elf_into_uspace = [&](const char *path) -> LoadElfResult {
        // TODO make_this a load_elf function
        auto res = fs_file_size(path);
        ASSERT(res.found_file);

        u8 *elf_buffer = (u8 *)kmalloc(res.size, 64);
        fs_read(path, (u8 *)elf_buffer, 0, res.size);
        auto elf_hdr = (Elf64_Ehdr *)elf_buffer;
        ASSERT(IS_ELF(*elf_hdr));

        for(int i = 0; i < elf_hdr->e_phnum; ++i) {
            auto phdr = (Elf64_Phdr *)(elf_buffer + elf_hdr->e_phoff + elf_hdr->e_phentsize * i);
            if(phdr->p_type != PT_DYNAMIC)
                continue;
            Elf64_Dyn *dyn = (Elf64_Dyn *)(elf_buffer + phdr->p_offset);
            while(1) {
                ASSERT(!(dyn->d_tag == DT_REL || dyn->d_tag == DT_RELA || dyn->d_tag == DT_RELR));
                if(dyn->d_tag == DT_NULL)
                    break;
                dyn++;
            }
        }

        u64 img_size = 0;
        for(int i = 0; i < elf_hdr->e_phnum; ++i) {
            auto phdr = (Elf64_Phdr *)(elf_buffer + elf_hdr->e_phoff + elf_hdr->e_phentsize * i);
            if(phdr->p_type != PT_LOAD)
                continue;
            img_size += phdr->p_memsz;
        }

        dbg_str("QWERTY 1 ");
        auto elf_img_vobj = (VObject *)kmalloc(sizeof(VObject), alignof(VObject));
        dbg_str("QWERTY 2 ");
        new (elf_img_vobj) VObject(img_size, 4096);
        dbg_str("QWERTY 3 ");
        vaddr load_addr = elf_img_vobj->map(*m_vspace);
        dbg_str("QWERTY 4 ");
        memset_workaround((void *)load_addr, 0, img_size);
        dbg_str("QWERTY 5 ");

        for(int i = 0; i < elf_hdr->e_phnum; ++i) {
            auto phdr = (Elf64_Phdr *)(elf_buffer + elf_hdr->e_phoff + elf_hdr->e_phentsize * i);
            if(phdr->p_type != PT_LOAD)
                continue;
            memmove_workaround((void *)(load_addr + phdr->p_vaddr), elf_buffer + phdr->p_offset, phdr->p_filesz);
            memset_workaround((void *)(load_addr + phdr->p_vaddr + phdr->p_filesz), 0, phdr->p_memsz - phdr->p_filesz);// TODO is this redundant because of the memset(load_addr, 0, img_size) above?
        }

        u64 start_rip = load_addr + elf_hdr->e_entry;

        kfree((vaddr)elf_buffer);

        return {elf_img_vobj, start_rip};
    };

    ASSERT(xor_(exe_path, start_rip));
    u64 exe_entry_rip = 0;
    if(exe_path) {

        dbg_str("QWERTY A ");
        auto res = load_elf_into_uspace(exe_path);
        exe_img_vobj = res.elf_img_vobj;
        exe_entry_rip = res.start_rip;

        dbg_str("QWERTY B ");
        const char *stdentry_path = "/userspace/stdentry";
        res = load_elf_into_uspace(stdentry_path);
        std_img_vobj = res.elf_img_vobj;
        start_rip = res.start_rip;
    }

    ASSERT(is_aligned(proc_stack_uspace.top, 64));
    ASSERT(is_aligned(proc_stack_kspace.top, 64));
    ASSERT(is_aligned(interrupt_stack_uspace.top, 64));
    ASSERT(is_aligned(g_interrupt_stack.top, 64));
    saved_state = {
        .reg_state = {
            .rflags = (1 << 9) | (1 << 1), // TODO what should initial process rflags be?
            .rdi = (u64)m_argc, // int argc
            .rsi = proc_stack_uspace.top - proc_stack_argv_len, // char **argv
            .rbp = 0, // this must be 0 initially for backtrace printing to work
            .rsp = 0, // this rsp is not used by switch_context
            .rbx = 0,
            .rdx = exe_entry_rip, // entry pointer
            .rcx = 0,
            .rax = 0,
            .r8 = 0,
            .r9 = 0,
            .r10 = 0,
            .r11 = 0,
            .r12 = 0,
            .r13 = 0,
            .r14 = 0,
            .r15 = 0,
        },
        .rsp = proc_stack_uspace.top - proc_stack_argv_len,
        .rip = start_rip,
        .cs = GDT_CODE0.raw,
        .ss = 0,
        // TODO switch to these when syscalls for alloc/free and print output are implemented
        //      switches back to kernelspace will not work while these are CODE0 and 0
        //.cs = GDT_CODE3.raw,
        //.ss = GDT_DATA3.raw,
    };
    state = State::RUNNING;
    dbg_str("BLAH 7\n");
    switch_context();
    __builtin_unreachable();
    UNREACHABLE();
}

void Process::user_resume()
{
    ASSERT(g_in_syscall_context);
    switch_context();
    __builtin_unreachable();
    UNREACHABLE();
}

void Process::kernel_process_start()
{
    ASSERT(g_in_syscall_context || g_in_kernel_init);
    ASSERT(is_kernel_process);
    ASSERT(!are_interrupts_enabled());
    dbg_str("kernel_process_start()\n");

    proc_stack_offset = 0;

    interrupt_stack_uspace = g_interrupt_stack;
    interrupt_stack_offset = 0;

    ASSERT(is_aligned(interrupt_stack_uspace.top, 64));
    dbg_str("asm context switch start\n");
    saved_state = {
        .reg_state = {
            .rflags = (1 << 9) | (1 << 1), // TODO what should initial process rflags be?
            .rdi = 0,
            .rsi = 0,
            .rbp = 0, // this must be 0 initially for backtrace printing to work
            .rsp = 0, // this rsp is unused by switch_context
            .rbx = 0,
            .rdx = 0,
            .rcx = 0,
            .rax = 0,
            .r8 = 0,
            .r9 = 0,
            .r10 = 0,
            .r11 = 0,
            .r12 = 0,
            .r13 = 0,
            .r14 = 0,
            .r15 = 0,
        },
        .rsp = proc_stack_kspace.top,
        .rip = start_rip,
        .cs = GDT_CODE0.raw,
        .ss = 0,
    };
    state = State::RUNNING;
    switch_context();
    __builtin_unreachable();
    UNREACHABLE();
}

void Process::kernel_resume()
{
    ASSERT(g_in_syscall_context);
    switch_context();
    __builtin_unreachable();
    UNREACHABLE();
}

void Process::switch_context()
{
    // TODO must also saved/restore fpu/sse registers

    // TODO there are 2 problems with this according to GNU documentation
    //          - in inline asm blocks, rsp should always be the same at the end of the block
    //          - in inline asm blocks, overwritten registers should be listed as clobbers or outputs tied to that register
    //      this code seems to work with no issues however (maybe because there is no C code following it, so any broken
    //      compiler asumptions about the asm are irrelevant?). But it may break for unexpected reasons because it breaks
    //      the compiler assumptions
    // NOTE there are not enough registers to do this without pushing all inputs and then popping them all into correct registers
    // TODO maybe a way around the lack of clobbers is to, instead of listing all the inputs, have one input that's a pointer to
    //      the saved_state struct, then all pushq use that pointer + an offset (offsetof() ?), then only one register plus
    //      memory & cc is getting clobbered
    dbg_str("RFLAGS: "); dbg_uint(saved_state.reg_state.rflags.raw); dbg_str("\n");
    saved_state.reg_state.rflags.bitfield.interrupt = 1;

    // TODO set io bitmap in rflags?

    u64 push_stack = saved_state.rsp - proc_stack_offset;

    // TODO the 20*8 is based on the number and size of the pushes in the asm statement
    //      if that asm gets modified this 20*8 number is invalid, find a way to make this
    //      less error prone
    u64 adjusted_pop_stack = saved_state.rsp - 20*8;

    g_in_syscall_context = false;
    g_in_kernel_init = false;
    s_block_tick = false;
    setup_interrupt_entry();
    // TODO share this code with kernel processes in switch_context()
    asm volatile(
        "movq %[push_stack], %%rsp\n"

// ---------------------------------------

        // iretq return stack frame
        "pushq %[stack_segment]\n" // 1
        "pushq %[pop_stack]\n" // TODO does pushq push to $rsp or to $rsp+1 ?
        "pushq %[saved_rflags]\n"
        "pushq %[code_segment]\n"
        "pushq %[start_rip]\n" // 5

        "pushq %[saved_rsi]\n"
        "pushq %[saved_rdi]\n"

        "pushq %[saved_rbp]\n"

        "pushq %[saved_rax]\n"
        "pushq %[saved_rbx]\n" // 10
        "pushq %[saved_rcx]\n"
        "pushq %[saved_rdx]\n"
        "pushq %[saved_r8]\n"
        "pushq %[saved_r9]\n"
        "pushq %[saved_r10]\n" // 15
        "pushq %[saved_r11]\n"
        "pushq %[saved_r12]\n"
        "pushq %[saved_r13]\n"
        "pushq %[saved_r14]\n"
        "pushq %[saved_r15]\n" // 20

// ---------------------------------------

        "movq %[adjusted_pop_stack], %%rsp\n"
        "movq %[pml4t], %%cr3\n" /* TODO this is inefficient in the case where this is not switching to a different pml4t (e.g. for kernel processes) */

// ---------------------------------------

        "popq %%r15\n"
        "popq %%r14\n"
        "popq %%r13\n"
        "popq %%r12\n"
        "popq %%r11\n"
        "popq %%r10\n"
        "popq %%r9\n"
        "popq %%r8\n"
        "popq %%rdx\n"
        "popq %%rcx\n"
        "popq %%rbx\n"
        "popq %%rax\n"

        "popq %%rbp\n"

        "popq %%rdi\n"
        "popq %%rsi\n"

        "iretq\n"
        : 
        :   [saved_rdi] "X"(saved_state.reg_state.rdi),
            [saved_rsi] "X"(saved_state.reg_state.rsi),
            [saved_rbp] "X"(saved_state.reg_state.rbp),
            [saved_rax] "X"(saved_state.reg_state.rax),
            [saved_rbx] "X"(saved_state.reg_state.rbx),
            [saved_rcx] "X"(saved_state.reg_state.rcx),
            [saved_rdx] "X"(saved_state.reg_state.rdx),
            [saved_r8] "X"(saved_state.reg_state.r8),
            [saved_r9] "X"(saved_state.reg_state.r9),
            [saved_r10] "X"(saved_state.reg_state.r10),
            [saved_r11] "X"(saved_state.reg_state.r11),
            [saved_r12] "X"(saved_state.reg_state.r12),
            [saved_r13] "X"(saved_state.reg_state.r13),
            [saved_r14] "X"(saved_state.reg_state.r14),
            [saved_r15] "X"(saved_state.reg_state.r15),
            [saved_rflags] "X"(saved_state.reg_state.rflags.raw),
            [stack_segment] "m"(saved_state.ss.raw),
            [pop_stack] "m"(saved_state.rsp),
            [adjusted_pop_stack] "r"(adjusted_pop_stack),
            [push_stack] "r"(push_stack),
            [code_segment] "m"(saved_state.cs.raw),
            [start_rip] "m"(saved_state.rip),
            [pml4t] "r"((u64)m_vspace->m_pml4t) // must use a register since the stack gets switched right before this value is used
        : /* "rbp", "rsp", "rsi", "rdi", "rax", "rbx", "rcx", "rdx", "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15", "memory", "cc" */
    );
    __builtin_unreachable();
    UNREACHABLE();
}

void Process::save_register_state(InterruptStackFrame *stack_frame, RegisterState *regs)
{
    ASSERT(g_in_syscall_context);
    saved_state.reg_state = *regs;
    saved_state.rsp = stack_frame->rsp;
    saved_state.rip = stack_frame->rip;
    saved_state.ss = stack_frame->ss;
    saved_state.cs = stack_frame->cs;
}

void Process::resume()
{
    ASSERT(g_in_syscall_context || g_in_kernel_init);
    switch(state)
    {
        case State::NOT_YET_STARTED:
        {
            if(is_kernel_process) {
                kernel_process_start();
                __builtin_unreachable();
                UNREACHABLE();
            } else {
                user_process_start();
                __builtin_unreachable();
                UNREACHABLE();
            }
        } break;

        case State::RUNNING:
        {
            if(is_kernel_process) {
                kernel_resume();
                __builtin_unreachable();
                UNREACHABLE();
            } else {
                user_resume();
                __builtin_unreachable();
                UNREACHABLE();
            }
        } break;

        default:
        {
            UNREACHABLE();
        } break;
    }
}

void Process::set_pwd(const char *pwd)
{
    ASSERT(pwd[0] == '/');

    if(m_pwd)
        kfree((vaddr)m_pwd);

    int src_len = strlen_workaround(pwd);
    pwd_len = src_len;
    if(pwd[pwd_len-1] != '/')
        pwd_len++;

    char *ptr = (char *)kmalloc(pwd_len+1, 64);
    memmove_workaround(ptr, (void *)pwd, src_len);
    ptr[pwd_len-1] = '/'; // if(!ends_with_slash)
    ptr[pwd_len] = 0;
    m_pwd = ptr;
}

const char *Process::get_pwd()
{
    ASSERT(m_pwd);
    return m_pwd;
}

int Process::pwd_length()
{
    ASSERT(m_pwd);
    return pwd_len;
}

// TODO have processes clean up their own vspace mappings/allocations on exit
void Process::exit()
{
 // NOTE: this method clears the process stacks, so if it is called using a process stack it will page fault
    ASSERT(g_interrupt_stack.currently_using_stack());
    ASSERT(g_in_syscall_context);
    ASSERT((proc_stack_kspace.bottom && proc_stack_uspace.bottom && !is_kernel_process) || 
           (proc_stack_kspace.bottom && !proc_stack_uspace.bottom && is_kernel_process));

    dbg_str("exit()\n");

    for(u32 i = vobjs.length-1; i <= 0; --i) {
        VObjectAllocation v = vobjs[i];
        v.vobj->unmap(*m_vspace);
        v.vobj->~VObject();
        kfree((vaddr)v.vobj);
        vobjs.unstable_remove(i);
    }

    // ASSERT((exe_img_vobj && std_img_vobj) || (!exe_img_vobj && !std_img_vobj));
    ASSERT(!xor_(exe_img_vobj, std_img_vobj));
    if(exe_img_vobj) {
        exe_img_vobj->unmap(*m_vspace);
        kfree((vaddr)exe_img_vobj);

        std_img_vobj->unmap(*m_vspace);
        kfree((vaddr)std_img_vobj);
    }

    if(!is_kernel_process) {
        proc_stack_vobj.unmap(g_kernel_vspace);
        proc_stack_vobj.unmap(*m_vspace);
        // VObject destructor will free VObject physical pages

        g_interrupt_stack_vobj.unmap(*m_vspace);

        // NOTE the vspace must be freed after the VObject, since the pml4t destructor will free the physical
        //      pages of the VObject if it's called first

    // unshare kernelspace pages so the kernel page tables don't get cleaned up
    // also switch to kernel pml4t since unshared_kernelspace() will unmap the kernel page tables, which we are currently using
        if(!in_kernel_vspace())
            write_cr3((u64)kernel_pml4t());
        m_vspace->unshare_kernelspace();
        m_vspace->~VSpace();
        kfree((vaddr)m_vspace);
        m_vspace = 0;
    } else {
        proc_stack_vobj.unmap(g_kernel_vspace);
    }

    if(m_pwd)
        kfree((vaddr)m_pwd);
    if(exe_path)
        kfree((vaddr)exe_path);

    ASSERT(parent); // process should always have parent unless it is init process, and the init process should never exit()
    parent->unblock_process(pid);
    parent->remove_child(this);
    
    if(children) {
        Process *child = children;
        ASSERT(!child->sibling_prev);
        while(child) {
            Process *child_next = child->sibling_next;
            if(child->can_be_orphaned) {
                child->parent = g_init_process;
                unlink_proc_siblings(child);
                g_init_process->add_child(child);
            } else {
                kill_process(child);
            }
            child = child_next;
        }
    }

    state = State::TERMINATED;
}

// TODO how much of process cleanup should be in exit() and how much should be in ~Process()
Process::~Process()
{
}

void Process::add_child(Process *child)
{
    ASSERT(child->parent = this);
    child->sibling_next = children;
    if(children)
        children->sibling_prev = child;
    children = child;
}

void Process::remove_child(Process *child)
{
    ASSERT(child->parent == this);
    ASSERT(children);
    if(children == child)
        children = child->sibling_next;
    unlink_proc_siblings(child);
}

void Process::unblock_process(pid_t pid_of_blocker)
{
    int found_index = -1;
    for(u32 i = 0; i < m_blockers.length; ++i) {
        Blocker blocker = m_blockers[i];

        if(blocker.type != Blocker::Type::PROCESS)
            continue;

        if(blocker.data.process.pid == pid_of_blocker) {
            found_index = i;
            break;
        }
    }

    // NOTE: this is out of the loop since unstable_remove modifies the vector under the loop
    if(found_index != -1)
        m_blockers.unstable_remove(found_index);
}

bool Process::is_blocked()
{
    return m_blockers.length != 0;
}

void Process::add_blocker_process(pid_t blocker_pid)
{
    Blocker blocker;
    blocker.type = Blocker::Type::PROCESS;
    blocker.data.process.pid = blocker_pid;
    m_blockers.append(blocker);
}

void Scheduler::add_to_queue(Process *proc)
{
    bool old_s_block_tick = s_block_tick;
    s_block_tick = true;

    unlink_proc_queue(proc);

    if(!wait_queue_start) {
        ASSERT(!wait_queue_end);
        wait_queue_start = proc;
        wait_queue_end = proc;
        return;
    }

    proc->queue_prev = wait_queue_end;
    if(wait_queue_end) {
        wait_queue_end->queue_next = proc;
    }
    wait_queue_end = proc;

    s_block_tick = old_s_block_tick;
}

void Scheduler::remove_from_queue(Process *proc)
{
    bool old_s_block_tick = s_block_tick;
    s_block_tick = true;

    ASSERT(proc);
    if(proc == current)
        current = 0;
    
    if(proc == wait_queue_start) {
        wait_queue_start = wait_queue_start->queue_next;
    }

    if(proc == wait_queue_end) {
        wait_queue_end = wait_queue_end->queue_prev;
    }

    unlink_proc_queue(proc);

    s_block_tick = old_s_block_tick;
}

Process *Scheduler::take_from_start()
{
    ASSERT(wait_queue_start);
    Process *proc = wait_queue_start;
    wait_queue_start = wait_queue_start->queue_next;
    if(!wait_queue_start) {
        wait_queue_end = 0;
    }
    unlink_proc_queue(proc);
    return proc;
}

Process *Scheduler::next_process_to_run()
{
    bool old_s_block_tick = s_block_tick;
    s_block_tick = true;

    // TODO keep a separate queue of blocked and non-blocked processes?
    // TODO if a process is blocked by another process, instead of skipping it, run the blocking process
    ASSERT(wait_queue_start);
    Process *proc;
        dbg_str("1\n");
    for(proc = wait_queue_start; proc != wait_queue_end; proc = proc->queue_next) {
        if(!proc->is_blocked())
            break;
    }

        dbg_str("3\n");
    remove_from_queue(proc);
        dbg_str("4\n");

    s_block_tick = old_s_block_tick;
    return proc;
}

void Scheduler::schedule()
{
    s_block_tick = true;
    ASSERT(g_in_syscall_context || g_in_kernel_init);

    dbg_str("schedule()\n");
    if(current) {
        add_to_queue(current);
        dbg_str("1\n");
        current = 0;
    }
    Process *proc = next_process_to_run();
    dbg_str("2\n");
    current = proc;
    current->m_ticks_left = TICKS_PER_SLICE;
    proc->resume();
    __builtin_unreachable();
    UNREACHABLE();
}

bool Scheduler::tick()
{
    if(!s_block_tick) {
        ASSERT(current && (current->state == Process::State::NOT_YET_STARTED || current->state == Process::State::RUNNING));
        current->m_ticks_left--;
        return current->m_ticks_left <= 0;
    }
    return false;
}

Process *current_process()
{
    return g_scheduler.current;
}
