#pragma once
#include "kernel/debug.cpp"
#include "include/math.h"
#include "kernel/vector.h"
#include "kernel/cpu.h"
#include "kernel/vspace.h"
#include "kernel/stack.h"
#include "include/syscall.h"

const u64 MAX_PROC_NAME_LEN = 256;
typedef void (*process_entry_ptr)();

// TODO merge this with RegisterState?
struct __attribute__((__packed__)) ProcessRegisterState
{
    RegisterState reg_state;
    u64 rsp;
    u64 rip;
    SegmentSelector cs;
    SegmentSelector ss;
};

struct Blocker
{
    enum Type
    {
        PROCESS
    };
    u32 type;
    union 
    {
        struct
        {
            pid_t pid;
        } process;
    } data;
};

struct Process
{
    ProcessRegisterState saved_state;

    Vector<Blocker> m_blockers;
    const char *exe_path = 0;
    vaddr start_rip = 0;
    bool can_be_orphaned;
    pid_t pid;
    char name[MAX_PROC_NAME_LEN+1];
    bool is_kernel_process; // TODO should kernel process and user process be separate types?
    VObject proc_stack_vobj;
    VSpace *m_vspace;
    struct VObjectAllocation
    {
        VObject *vobj;
        vaddr alloced_addr;
    };
    // TODO make sure these are cleaned up properly on process exit
    Vector<VObjectAllocation> vobjs = {};

    VObject *exe_img_vobj = 0;
    VObject *std_img_vobj = 0;

    Process *queue_next = 0;
    Process *queue_prev = 0;

    // TODO make type for buffers that are mapped in user & kernel space
    Stack proc_stack_uspace;
    Stack proc_stack_kspace;
    u64 proc_stack_offset = 0;
    u64 proc_stack_argv_len = 0;
    u64 proc_stack_argv_offset = 0;

    Stack interrupt_stack_uspace;
    u64 interrupt_stack_offset;

    u64 m_ticks_left = 0;

    // TODO should this use a pid and some code for looking up pids and making sure they are in a valid state?
    //      then children & siblings would be vectors of pids
    Process *parent = 0;
    Process *children = 0;
    // TODO make type to hold next & prev pointers (use for siblin list & queue list, and for other linked lists in codebase)
    Process *sibling_next = 0;
    Process *sibling_prev = 0;

    const char *m_pwd = 0;
    int pwd_len = 0;
    int m_argc = 0;
    char **m_argv_kspace = 0;

    static const u64 DEFAULT_STACK_SIZE = 256*KB;
    static const u64 DEFAULT_STACK_ALIGNMENT = 64;

    // TODO add BLOCKED state?
    enum class State : u8
    {
        NOT_YET_STARTED,
        RUNNING,
        TERMINATED
    };
    State state = State::NOT_YET_STARTED;

    Process(void (*)(), bool, const char *, bool, const char *);
    Process(const char *, char **, bool, bool, const char *);
    ~Process();

    void setup_vspace(bool);

    void user_process_start();
    void kernel_process_start();

    void user_resume();
    void kernel_resume();

    void save_register_state(InterruptStackFrame *, RegisterState *);

    void resume();

    void exit();

    void switch_context();

    void unblock_process(pid_t);

    bool is_blocked();
    void add_blocker_process(pid_t);
    void add_child(Process *);
    void remove_child(Process *);
    void setup_interrupt_entry();

    vaddr alloc_mem_in_vspace(u64 size, u64 align);
    void free_mem_in_vspace(vaddr addr);

    void set_pwd(const char *pwd);
    void copy_argv_to_stack(char **src_argv);

    const char *get_pwd();
    int pwd_length();
};

void unlink_proc_queue(Process *);
void unlink_proc_siblings(Process *);

struct Scheduler
{
    // start of queue is the next process to be run
    Process *wait_queue_start = 0;
    Process *wait_queue_end = 0;

    Process *current = 0;

    // TODO this should be part of a linked list type...
    void unlink_proc(Process *);

    void add_to_queue(Process *);
    void remove_from_queue(Process *);

    Process *next_process_to_run();

    void schedule();
    bool tick();
    Process *take_from_start();
};

Process *current_process();
