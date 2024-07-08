#pragma once
#include "kernel/physical_allocator.h"
#include "include/math.h"

PhysicalPageAllocator g_phys_page_allocator;

paddr PhysicalPageAllocator::allocate_page()
{
    // under normal conditions a pointer of 0 means the freelist is empty
    ASSERT(m_freelist != 0);

    PhysicalPage *page = m_freelist;
    m_freelist = m_freelist->freelist_next;

    ASSERT(!page->is_allocated);
    page->is_allocated = true;
    page->freelist_next = 0;

    u64 index = ((u64)page - (u64)m_pages) / sizeof(PhysicalPage);
    paddr page_addr = m_allocation_region.addr + index*4096;

    __builtin_memset((void *)page_addr, 0, 4096);

    return page_addr;
}

void PhysicalPageAllocator::free_page(paddr addr)
{
    ASSERT(is_aligned(addr, 4096));

    u64 index = (addr - m_allocation_region.addr) / 4096;
    PhysicalPage& page = m_pages[index];

    if(!page.is_allocated) {
        [[maybe_unused]] int breakpoint = 0;
        [[maybe_unused]] int breakpoint2 = 0;
    }
    ASSERT(page.is_allocated);

    page.is_allocated = false;
    page.freelist_next = m_freelist;
    m_freelist = &page;
}

void PhysicalPageAllocator::init(const PRange& range)
{
    ASSERT(is_aligned(range.addr, 4096));
    ASSERT(is_aligned(range.one_past_end(), 4096));
    ASSERT((range.length % 4096) == 0);

    // range is the physical address space available to be allocated
    // we need to set aside some of this range to hold the PhysicalPage
    // array.
    // so range will be split like this:
    // | -- PhysicalPage array -- | -------- usable(allocatable) memory -------- |
    //
    // to find how big PhysicalPage array will be:
    //  - find the number of pages for a PhysicalPage array that covers all of range
    //  - subtract the number of pages that will be occupied by the PhysicalPage array
    //  - round the PhysicalPage range and the usable range to page boundaries
    //  TODO see if there is a smarter way to do this

    // TODO make a subtract() method for Ranges

    u64 num_structs = range.length / 4096;
    u64 page_arr_size = num_structs*sizeof(PhysicalPage);

    u64 useless_pages = round_up_divide(page_arr_size , 4096);
    num_structs -= useless_pages;
    page_arr_size = num_structs*sizeof(PhysicalPage);

    PRange allocator_range = {
        range.addr,
        round_up_align(page_arr_size, 4096)
    };

    paddr usable_base_addr = allocator_range.one_past_end();
    u64 usable_size = range.length - allocator_range.length;
    PRange allocatable_range = {
        round_up_align(usable_base_addr, 4096),
        round_down_align(usable_size, 4096)
    };

    ASSERT(allocator_range.one_past_end() <= allocatable_range.addr);
    ASSERT(allocatable_range.one_past_end() <= range.one_past_end());
    ASSERT(allocatable_range.length + allocator_range.length <= range.length);
    
    dbg_str("allocator_range:\n");
    allocator_range.print();
    dbg_str("allocatable_range:\n");
    allocatable_range.print();

    g_phys_page_allocator.m_allocation_region = allocatable_range;
    g_phys_page_allocator.m_pages = (PhysicalPage *)allocator_range.addr;
    g_phys_page_allocator.m_page_count = allocatable_range.length / 4096;
    ASSERT(g_phys_page_allocator.m_page_count * 4096 == g_phys_page_allocator.m_allocation_region.length);

    // init freelist
    g_phys_page_allocator.m_freelist = g_phys_page_allocator.m_pages;
    for(u32 i = 1; i < g_phys_page_allocator.m_page_count; ++i) {
        PhysicalPage& page = g_phys_page_allocator.m_pages[i];
        PhysicalPage& prev_page = g_phys_page_allocator.m_pages[i-1];
        prev_page.freelist_next = &page;
    }

    g_phys_page_allocator.m_is_initialized = true;
    dbg_str("init_phys_pages\n");
}

u64 PhysicalPageAllocator::count_freelist_entries()
{
    u64 sum = 0;
    for(auto ptr = m_freelist; ptr; ptr = ptr->freelist_next) {
        sum++;
    }
    return sum;
}

paddr alloc_phys_page()
{
    return g_phys_page_allocator.allocate_page();
}

void free_phys_page(paddr page)
{
    return g_phys_page_allocator.free_page(page);
}

/*
void test_phys_arr_allocations_and_deallocations()
{
    vga_print("phys arr test begin\n");
    paddr test = 0;
    for(u32 i = 0; i < phys_pages_arr_count; ++i) {
        test = alloc_phys_page();
        dbg_str("first page addr: ");
        dbg_uint((u64)test);
        dbg_str("\n");
    }
    test = phys_pages_allocation_region.addr;
    for(u32 i = 0; i < phys_pages_arr_count; ++i) {
        free_phys_page(test);
        dbg_str("freed page addr: ");
        dbg_uint((u64)test);
        dbg_str("\n");
        test += 4096;
    }

    test = 0;
    for(u32 i = 0; i < phys_pages_arr_count; ++i) {
        test = alloc_phys_page();
        dbg_str("first page addr: ");
        dbg_uint((u64)test);
        dbg_str("\n");
    }
    test = phys_pages_allocation_region.addr;
    for(u32 i = 0; i < phys_pages_arr_count; ++i) {
        free_phys_page(test);
        dbg_str("freed page addr: ");
        dbg_uint((u64)test);
        dbg_str("\n");
        test += 4096;
    }
    vga_print("phys arr test end\n");
}

*/
