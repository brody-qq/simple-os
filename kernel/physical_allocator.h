#pragma once
#include "kernel/types.h"
#include "kernel/range.h"

// TODO use the 0 to 2MB memory range or the space under the kernel image for this?
// TODO zero memory (this has bitten me, qemu seems to zero memory unless you restart...)

// TODO this struct could be made smaller by removing is_allocated
//      and using invalid pointer values in freelist_next to indicate 
//      if page is allocated or not
// TODO future features may require that this is reference counted 
//      (e.g. mapping the same buffer to kernel & user address space, mapping the same pages to multiple process addr spaces)

struct PhysicalPage
{
    PhysicalPage *freelist_next = 0;
    bool is_allocated = false;
};

// this object keeps track of which physical pages are used/unused in the region described by allocation_region
// the page tracked by each entry can be found based off the index in the array and the start address of the allocation_region
struct PhysicalPageAllocator
{
// NOTE m_pages is allocated dynamically to keep the kernel image smaller
//      (I tried allocting this statically but it made kernel image too
//       big for GRUB)
    PhysicalPage *m_pages;
    u32 m_page_count;

    PhysicalPage *m_freelist;

    // used to track which phys_pages_arr index is associated with which physical page
    PRange m_allocation_region;

    bool m_is_initialized = false;

    paddr allocate_page();

    void free_page(paddr addr);

    static void init(const PRange& range);

    u64 count_freelist_entries();
};

paddr alloc_phys_page();

void free_phys_page(paddr page);