#pragma once
#include "kernel/vspace.h"
#include "kernel/range.h"

VSpace g_kernel_vspace = {};
bool g_kernel_vspace_is_initialized = false;
extern bool g_in_kernel_init;

AllocList::AllocList()
{
    ASSERT(g_in_kernel_init);
};

AllocList::AllocList(VRange span) : alloc_range(span)
{
    ASSERT(is_aligned(span.addr, 4096));
    ASSERT(is_aligned(span.one_past_end(), 4096));
    ASSERT(g_phys_page_allocator.m_is_initialized);

    m_first = take_from_hdr_freelist();
    m_first->next = 0;
    m_first->prev = 0;
    m_first->alloc_range = span;

    m_last = m_first;
}

VRange AllocList::take_range(u64 wanted_size, u64 alignment)
{
    ASSERT(m_first);
    ASSERT(m_last);

    ASSERT(is_aligned(wanted_size, 4096));
    ASSERT(is_aligned(alignment, 4096));

    dbg_str("\nTAKE_RANGE START: "); dbg_uint(m_first->alloc_range.addr); dbg_str("\n");
    u64 worst_case_size = wanted_size + alignment-1;
    for(auto hdr = m_first; hdr; hdr = hdr->next) {
        ASSERT(hdr->alloc_range.length != 0);
        if(hdr->alloc_range.length >= worst_case_size) {
            u64 aligned_addr = round_up_align(hdr->alloc_range.addr, alignment);
            ASSERT(aligned_addr + wanted_size <= hdr->alloc_range.one_past_end());
            VRange taken_range = {
                aligned_addr,
                wanted_size
            };

            AllocHeader hdr_copy = *hdr;
            add_to_hdr_freelist(hdr);

            // 2 unused ranges, before & after the taken_range
            u64 unused_size_before = aligned_addr - hdr_copy.alloc_range.addr;
            if(unused_size_before > 0) {
                VRange unused = {
                    hdr_copy.alloc_range.addr,
                    unused_size_before
                };
                return_range(unused);
            }

            u64 unused_size_after = hdr_copy.alloc_range.one_past_end() - taken_range.one_past_end();
            if(unused_size_after > 0) {
                VRange unused = {
                    taken_range.one_past_end(),
                    unused_size_after
                };
                return_range(unused);
            }

            dbg_str("TAKE_RANGE END: "); dbg_uint(m_first->alloc_range.addr); dbg_str("\n\n");
            return taken_range;
        }
    }

    UNREACHABLE();
}

void AllocList::return_range(VRange returned_range)
{
    // TODO can be skipped if check if range will be coalesced first
    auto new_hdr = take_from_hdr_freelist();

    new_hdr->alloc_range = returned_range;

    if(!m_first) {
        ASSERT(!m_last);
        m_first = m_last = new_hdr;
        m_first->next = m_first->prev = 0;
        return;
    }

    if(new_hdr->alloc_range.addr < m_first->alloc_range.addr) {
        new_hdr->next = m_first;
        new_hdr->prev = 0;

        m_first->prev = new_hdr;
        m_first = new_hdr;
        coalesce(new_hdr);
        return;
    }

    if(new_hdr->alloc_range.addr > m_last->alloc_range.addr) {
        new_hdr->next = 0;
        new_hdr->prev = m_last;

        m_last->next = new_hdr;
        m_last = new_hdr;
        coalesce(new_hdr);
        return;
    }

    for(auto search_hdr = m_first; search_hdr; search_hdr = search_hdr->next) {
        ASSERT(search_hdr->alloc_range.addr != returned_range.addr);
        ASSERT(search_hdr->alloc_range.length != 0);
        if(search_hdr->alloc_range.addr > returned_range.addr) {
            new_hdr->next = search_hdr;
            new_hdr->prev = search_hdr->prev;

            if(search_hdr->prev)
                search_hdr->prev->next = new_hdr;
            search_hdr->prev = new_hdr;

            coalesce(new_hdr);
            return;
        }
    }

    UNREACHABLE();
}

void AllocList::coalesce(AllocHeader *hdr)
{
    ASSERT(hdr);
    auto prev_hdr = hdr->prev;
    auto next_hdr = hdr->next;

    if(next_hdr) {
        impl_coalesce(hdr, next_hdr);
    }

    if(prev_hdr) {
        impl_coalesce(prev_hdr, hdr);
    }
}

void AllocList::impl_coalesce(AllocHeader *curr, AllocHeader *next_hdr)
{
    ASSERT(next_hdr->prev == curr);
    ASSERT(curr->next = next_hdr);

    auto curr_end = curr->alloc_range.addr + curr->alloc_range.length;
    if(curr_end == next_hdr->alloc_range.addr) {
        curr->alloc_range.length += next_hdr->alloc_range.length;

        curr->next = next_hdr->next;
        if(next_hdr->next)
            next_hdr->next->prev = curr;

        add_to_hdr_freelist(next_hdr);
    }
}

void AllocList::add_to_hdr_freelist(AllocHeader *hdr)
{
    if(m_first == hdr)
        m_first = m_first->next;
    if(m_last == hdr)
        m_last = m_last->prev;

    // TODO these can be merged with the above 2 if statements
    if(hdr->next)
        hdr->next->prev = hdr->prev;
    if(hdr->prev)
        hdr->prev->next = hdr->next;

    // zero out header to catch bugs
    hdr->next = 0;
    hdr->prev = 0;
    hdr->alloc_range.addr = 0;
    hdr->alloc_range.length = 0;

    hdr->next = m_freelist;
    m_freelist = hdr;
}

void AllocList::expand_hdr_freelist()
{
    paddr new_page = alloc_phys_page();

    u32 hdrs_per_page = 4096 / sizeof(AllocHeader);

    // page is 4kb aligned, so no need to handle alignment of AllocHeader
    // TODO is it better performance to zero and link entire page of free hdrs first then add only the first to freelist?
    ASSERT(4096 % alignof(AllocHeader) == 0);
    AllocHeader *base_hdr = (AllocHeader *)new_page; 
    for(u32 i = 0; i < hdrs_per_page; ++i) {
        AllocHeader *hdr = base_hdr + i;
        add_to_hdr_freelist(hdr);
    }
}

AllocList::AllocHeader *AllocList::take_from_hdr_freelist()
{
    if(!m_freelist) {
        expand_hdr_freelist();
    }

    ASSERT(m_freelist);
    auto free_hdr = m_freelist;
    m_freelist = m_freelist->next;
    return free_hdr;
}

// for testing/debugging purposes
u64 AllocList::get_free_space()
{
    u64 sum = 0;
    for(auto hdr = m_first; hdr; hdr = hdr->next) {
        sum += hdr->alloc_range.length;
    }
    return sum;
}

VSpace::VSpace() : m_pml4t(0)
{
    ASSERT(g_in_kernel_init);
}

VSpace::VSpace(VRange alloc_span) : m_pml4t(0), m_vrange_allocator(alloc_span)
{
    // NOTE for processes, the pml4t must be available in both the kernel vspace and the process vspace
    //      the simplest way to do this is to put the pml4t (and all other page tables) in the pmap memory
    m_pml4t = (PML4T *)g_phys_page_allocator.allocate_page();
    m_pml4t->clear();
    m_vrange_allocator = AllocList(alloc_span);
}

VSpace::~VSpace()
{
    ASSERT(this != &g_kernel_vspace); // there is a bug somwhere if the destructor for g_kernel_vspace is called

    // clear all
// TODO free_page_tables currently does not reference count pages, so there is room for error here if this is called
//      when there are pages shared between multiple vspaces
    free_page_tables(m_pml4t);
    m_pml4t = 0;
}

extern PML4T g_kernel_pml4t;
void VSpace::init_kernel_vspace()
{
    g_kernel_vspace.m_pml4t = kernel_pml4t();
    VRange kernel_vspace = {
        KERNEL_VSPACE_START,
        KERNEL_VSPACE_SIZE
    };
    g_kernel_vspace.m_vrange_allocator = AllocList(kernel_vspace);
}

u64 VSpace::worst_case_size(u64 size, u64 alignment)
{
    u64 adjusted_align = (alignment == 0) ? 0 : alignment-1;
    u64 worst_case_size = round_up_align(size + adjusted_align, 4096);
    return worst_case_size;
}

VSpace::VBuffer VSpace::alloc_vbuffer(u64 size, u64 alignment)
{
    // this code is written on the assumption that alignment is less than 4096
    // otherwise buffer_start will be on a different page than the AllocHeader, which will
    // lead to problems for get_allocated_vrange_from_ptr()

    ASSERT(size > 0);

    dbg_str("POW2 "); dbg_uint(alignment);
    ASSERT(is_power_of_2(alignment));

    u64 round_up_alignment = round_up_align(4096, alignment);
    u64 page_alignment = (round_up_alignment / 4096) * 4096;
    u64 alloc_alignment = max((u64)4096, page_alignment - 4096);
    u64 in_page_alignment = round_up_alignment % 4096;
    //u64 header_page_alignment -= round_down_align(in_page_alignment;
    // round up to page align buffer, then add one page for the AllocHeader
    ASSERT(in_page_alignment < 4096);
    u64 alloc_size = worst_case_size(size, in_page_alignment) + 4096;
    auto full_alloc_range = m_vrange_allocator.take_range(alloc_size, alloc_alignment);

    VRange header_alloc_range = {
        full_alloc_range.addr,
        4096
    };
    u64 alloc_hdr_start = round_up_align(header_alloc_range.addr, alignof(AllocHeader));

    VRange buffer_alloc_range = full_alloc_range.subtract(header_alloc_range);
    u64 buffer_start = round_up_align(buffer_alloc_range.addr, in_page_alignment);
    u64 buffer_end = buffer_start + size;

    ASSERT(is_aligned(full_alloc_range.addr, 4096));
    ASSERT(is_aligned(full_alloc_range.one_past_end(), 4096));

    ASSERT(is_aligned(full_alloc_range.addr, alloc_alignment));
    ASSERT(full_alloc_range.length == alloc_size);

    ASSERT(is_aligned(buffer_alloc_range.addr, page_alignment));
    ASSERT(header_alloc_range.length + buffer_alloc_range.length == full_alloc_range.length);
    ASSERT(buffer_alloc_range.length >= size);

    ASSERT(is_aligned(buffer_start, alignment));
    ASSERT(buffer_end <= buffer_alloc_range.one_past_end());
    
// this makes sure the computation to get the header from a ptr (get_allocated_vrage_from_ptr()) will work
    ASSERT(round_down_align(buffer_start, 4096) == buffer_alloc_range.addr); 

    return {full_alloc_range, header_alloc_range, buffer_alloc_range, alloc_hdr_start, buffer_start};
}

void VSpace::reload_cr3_if_needed()
{
    // NOTE: all process vspaces share the kernel page tables, so must reload cr3 if any changes
    //       happen to kernel tables
    // TODO it may be necessary to implement a more general mechanism for objs shared between mutliple processes
    // TODO speed this up by using invlpg instead?
    if(m_pml4t == current_pml4t() || m_pml4t == kernel_pml4t()) {
        reload_cr3();
    }
}

vaddr VSpace::allocate_size(u64 size, u64 alignment)
{
    dbg_str("VSPACE::ALLOC_SIZE\n");
    VBuffer vbuf = alloc_vbuffer(size, alignment);
    auto full_alloc_range = vbuf.full_alloc_range;
    //auto header_alloc_range = vbuf.header_alloc_range;
    auto buffer_alloc_range = vbuf.buffer_alloc_range;
    auto alloc_hdr_start = vbuf.alloc_hdr_start;
    auto buffer_start = vbuf.buffer_start;

    map_vrange(full_alloc_range, m_pml4t);
    reload_cr3_if_needed();
    
    auto hdr = (AllocHeader *)alloc_hdr_start;
    hdr->buffer_alloc_range = buffer_alloc_range;

    return buffer_start;
}

// NOTE: each VSpace that allocates a VObject must have its' own separate header, so the header is allocated & mapped separately from the rest of the pages
vaddr VSpace::allocate_pages(const Vector<paddr>& pages, u64 alignment)
{
    dbg_str("VSPACE::ALLOC_PAGES\n");
    u64 size = pages.length * 4096; // pages.length is already based off of worst_case_size()
    VBuffer vbuf = alloc_vbuffer(size, alignment);
    //auto full_alloc_range = vbuf.full_alloc_range;
    auto header_alloc_range = vbuf.header_alloc_range;
    auto buffer_alloc_range = vbuf.buffer_alloc_range;
    auto alloc_hdr_start = vbuf.alloc_hdr_start;
    auto buffer_start = vbuf.buffer_start;

    map_vrange(header_alloc_range, m_pml4t);
    map_vrange(buffer_alloc_range, m_pml4t, pages);
    reload_cr3_if_needed();
    
    auto hdr = (AllocHeader *)alloc_hdr_start;
    hdr->buffer_alloc_range = buffer_alloc_range;

    return buffer_start;
}

vaddr VSpace::allocate_vobj(VObject *vobj)
{
    dbg_str("VSPACE::ALLOC_VOBJ\n");
    vaddr addr = allocate_pages(vobj->underlying_pages, vobj->alignment);
    mapped_vobjs.append(vobj);
    return addr;
}
void VSpace::free_vobj(VObject *vobj, vaddr addr)
{
    dbg_str("VSPACE::FREE_VOBJ\n");
    bool found = false;
    VObject *ptr = 0;
    u32 i = 0;
    for(i = 0; i < mapped_vobjs.length; ++i) {
        ptr = mapped_vobjs[i];
        if(ptr == vobj) {
            found = true;
            break;
        }
    }
    ASSERT(found);

    free_pages(vobj->underlying_pages, addr);
    mapped_vobjs.unstable_remove(i);
}

void VSpace::free_size(vaddr ptr)
{
    dbg_str("VSPACE::FREE_SIZE\n");
    dbg_str("FREE PTR: "); dbg_uint(ptr); dbg_str("\n");
    AllocedVRanges ranges = get_alloc_vranges_from_ptr(ptr);
    auto header_alloc_range = ranges.header_alloc_range;
    auto buffer_alloc_range = ranges.buffer_alloc_range;

    ASSERT(header_alloc_range.one_past_end() == buffer_alloc_range.addr);
    VRange full_alloc_range = {
        header_alloc_range.addr,
        header_alloc_range.length + buffer_alloc_range.length
    };

    unmap_vrange(full_alloc_range, m_pml4t);

    reload_cr3_if_needed();

    m_vrange_allocator.return_range(full_alloc_range);
}

void VSpace::free_pages(const Vector<paddr>& pages, vaddr ptr)
{
    dbg_str("VSPACE::FREE_PAGES\n");
    AllocedVRanges ranges = get_alloc_vranges_from_ptr(ptr);
    auto header_alloc_range = ranges.header_alloc_range;
    auto buffer_alloc_range = ranges.buffer_alloc_range;
    ASSERT(header_alloc_range.one_past_end() == buffer_alloc_range.addr);
    VRange full_alloc_range = {
        header_alloc_range.addr,
        header_alloc_range.length + buffer_alloc_range.length
    };

    unmap_vrange(header_alloc_range, m_pml4t);
    unmap_vrange(buffer_alloc_range, m_pml4t, pages);

    reload_cr3_if_needed();

    m_vrange_allocator.return_range(full_alloc_range);
}

VSpace::AllocedVRanges VSpace::get_alloc_vranges_from_ptr(vaddr ptr)
{
    // TODO is there a way to do a sanity check that ptr is valid and was allocated by allocate() ?
    vaddr base_of_allocation = round_down_align(ptr, 4096);
    VRange header_alloc_range = {
        base_of_allocation - 4096,
        4096
    };
    u64 alloc_hdr_start = round_up_align(header_alloc_range.addr, alignof(AllocHeader));
    auto hdr = (AllocHeader *)alloc_hdr_start;
    VRange buffer_alloc_range = hdr->buffer_alloc_range;

    return {header_alloc_range, buffer_alloc_range};
}

// for testing/debugging purposes
u64 VSpace::get_free_space()
{
    return m_vrange_allocator.get_free_space();
}

extern u64 g_pmap_pdpt_count;
extern u64 g_pmap_pd_count;
extern u64 g_pmap_page_count;
void VSpace::share_kernelspace()
{
    ASSERT(KERNEL_VSPACE_SIZE % GB == 0);
    ASSERT(KERNEL_VSPACE_START % GB == 0);

    u64 kernel_vspace_end = KERNEL_VSPACE_START + KERNEL_VSPACE_SIZE;
    u64 kernel_pd_count = round_up_divide(kernel_vspace_end, bytes_mapped_by_pd);
    ASSERT(kernel_pd_count < 512); // rest of function is written on assumption that all kernelspace fits in one PDPT

// TODO make kernel_pml4t() a macro
// TODO change get_phys_addr() to phys_addr()
    u64 kernel_pdpt0_addr = kernel_pml4t()->entries[0].get_phys_addr();
    m_pml4t->entries[0].clear();
    m_pml4t->entries[0].set_phys_addr(kernel_pdpt0_addr);
    m_pml4t->entries[0].bitfield.present = 1;
    m_pml4t->entries[0].bitfield.writable = 1; // TODO should this be mapped as writable?
}

void VSpace::unshare_kernelspace()
{
    ASSERT(KERNEL_VSPACE_SIZE % GB == 0);
    ASSERT(KERNEL_VSPACE_START % GB == 0);

    u64 kernel_vspace_end = KERNEL_VSPACE_START + KERNEL_VSPACE_SIZE;
    u64 kernel_pd_count = round_up_divide(kernel_vspace_end, bytes_mapped_by_pd);
    ASSERT(kernel_pd_count < 512); // rest of function is written on assumption that all kernelspace fits in one PDPT

    PML4TE& pml4te0 = m_pml4t->entries[0]; 
    ASSERT(pml4te0.bitfield.present);
    ASSERT(pml4te0.get_phys_addr() == kernel_pml4t()->entries[0].get_phys_addr());
    pml4te0.clear();
}

bool in_kernel_vspace()
{
    return current_pml4t() == kernel_pml4t();
}

PML4T *kernel_pml4t()
{
    return &g_kernel_pml4t;
}

void VObject::initialize_interrupt_stack(u64 alloc_size, u64 alignment)
{
    g_interrupt_stack_vobj.underlying_pages = Vector<paddr>();
    g_interrupt_stack_vobj.alignment = alignment;
    g_interrupt_stack_vobj.vspaces_mapped_in = {};
    // TODO this is a copy of the constructor
    u64 worst_case_vrange_size = VSpace::worst_case_size(alloc_size, alignment);
    u64 page_count = round_up_divide(worst_case_vrange_size, 4096);
    g_interrupt_stack_vobj.underlying_pages.expand_capacity(page_count);
    for(u64 i = 0; i < page_count; ++i) {
        paddr page = g_phys_page_allocator.allocate_page();
        g_interrupt_stack_vobj.underlying_pages.append(page);
    }
}

VObject::VObject() : underlying_pages(), alignment(0) {}

VObject::VObject(u64 alloc_size, u64 align) : underlying_pages(0), alignment(align)
{
    dbg_str("VOBJECT()\n");
    ASSERT(g_kernel_vspace_is_initialized);
    u64 worst_case_vrange_size = VSpace::worst_case_size(alloc_size, alignment);
    u64 page_count = round_up_divide(worst_case_vrange_size, 4096);
    underlying_pages.expand_capacity(page_count);
    for(u64 i = 0; i < page_count; ++i) {
        paddr page = g_phys_page_allocator.allocate_page();
        underlying_pages.append(page);
    }
    ASSERT(underlying_pages.length == page_count);
}

VObject::~VObject()
{
    dbg_str("~VOBJECT()\n");
    ASSERT(vspaces_mapped_in.length == 0); // object does not keep track of the vranges it is mapped to, so client code must keep track of this and unmap all vranges before destroying VObject
    for(u64 i = 0; i < underlying_pages.length; ++i) {
        paddr page = underlying_pages[i];
        g_phys_page_allocator.free_page(page);
    }
}

vaddr VObject::map(VSpace& map_into)
{
    vaddr addr = map_into.allocate_vobj(this);
    VSpaceMapping mapping = {&map_into, addr};
    vspaces_mapped_in.append(mapping);
    return addr;
}

void VObject::unmap(VSpace& map_outof)
{
    bool found = false;
    VSpaceMapping mapping;
    u32 i = 0;
    for(i = 0; i < vspaces_mapped_in.length; ++i) {
        mapping = vspaces_mapped_in[i];
        if(mapping.vspace == &map_outof) {
            found = true;
            break;
        }
    }
    ASSERT(found);

    map_outof.free_vobj(this, mapping.alloced_addr);
    vspaces_mapped_in.unstable_remove(i);
}
