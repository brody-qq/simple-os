#pragma once
#include "kernel/types.h"
#include "kernel/page_tables.h"
#include "kernel/range.h"
#include "kernel/vector.h"

// resource: Unix Interals (Uresh Vahalia), chapter 12
// TODO implement:
//
// doubly linked list heap allocator:
//  - each block has header:
//      struct header {
//          bool is_used;
//          header *next;
//          header *prev;
//          u64 size;
//      }
//  - when allocate:
//      - we search for a is_used == false region that has enough size
//      - we set header to is_used == true
//      - allocate phys pages for our new buffer
//      - allocate a phys page for the new header
//      - map vrange for new buffer & for new header
//      - we make a new header with is_used == false, an updated size, and link up the linked lists
//  - when de-allocate:
//      - look at next & prev to see if they are used, to see if we can combine our new free region with either (or both) of them
//      - de-allocate phys pages of buffer and unmap vrange
//      - if our new free region was combined with the prev or next (or both) regions, we de-allocate & unmape the header(s) that we don't need
//
//  ALTERNATIVELY (note that this is the 'resource map' from Unix Intenals book:
//      - implement almost exactly the above thing, but ONLY tracking the free regions with linked list headers (tracking the used regions is pointless)
//      - the header is allocated along with the buffer, and it is placed at the end of the buffer, so we always get rid of the buffer & the header in one go
//
//  NOTE:
//      since these are allocating from a vrange, we will need to allocate page tables, so we need an allocator for this allocator... (the page tables will be allocated via the physical page allocator)
//      this allocator will require a map() and unmap() function to map & unmap vranges to physical pages
//      each freelist entry takes a physical page to store
//      so the advantage of the above schemes is that they combine contiguous free regions into a single freelist entry
//      the disadvantage of power of 2 allocators is that they keep significantly more freelist entries, which takes up a lot of phys pages
//      also noteworthy is that each freelist entry takes up an entire page, just to store a freelist pointer
//      also because we are dealing with vranges, any gaps between the allocated buffers are irrelevant, since that space will not be maped to physical pages
//      buddy allocators don't use freelist entries like power of 2 freelist allocators do, but they are more complicated
//      I may use object pool / slab allocator for fixed size kernel objects in the future if I have time to implement this after rest of project is completed

// this is based off the 'resource map' as described in Unix Internals (Uresh Vahalia) chapter 12
// NOTE: this does not free physical pages that are no longer used
//       pages that are no longer used will stay around with their freelist entries
//       this is not ideal, but it will still lead to less memory usage than the alternative allocators
//       if this becomes an issue, a prune() method can be added to look for pages that contain nothing but freelist entries, and then those pages can be freed and their freelist entries discarded

bool is_page_mapped_pmap(paddr, PML4T *);
struct AllocList
{
    struct AllocHeader
    {
        AllocHeader *next;
        AllocHeader *prev;
        VRange alloc_range;
    };

    // TODO look over this code to see if the 0 first, last and 0 next and prev cases are accounted for properly
    AllocHeader *m_first = 0;
    AllocHeader *m_last = 0; // TODO is there any reason to track the last entry?

    AllocHeader *m_freelist = 0;

    VRange alloc_range = {};

    AllocList();
    AllocList(VRange span);

    VRange take_range(u64, u64);

    void return_range(VRange);

    void coalesce(AllocHeader *);

    void impl_coalesce(AllocHeader *, AllocHeader *);

    void add_to_hdr_freelist(AllocHeader *);

    void expand_hdr_freelist();

    AllocHeader *take_from_hdr_freelist();

    // for testing/debugging purposes
    u64 get_free_space();
};

struct VObject;

// TODO put unmapped guard pages before & after the returned buffer?
struct VSpace
{
    PML4T *m_pml4t = 0; // NOTE: this is 4KB aligned so it should always be first to avoid wasted space
    AllocList m_vrange_allocator = {};
    Vector<VObject *> mapped_vobjs = {};

    // this header is specifically to keep track of the allocation size, so that given a pointer, the alloc_size can be found
    // this allows for returning the entire allocated range to vrange_allocator
    struct AllocHeader
    {
        VRange buffer_alloc_range;
    };

    // return type for alloc_vbuffer to return multiple values
    struct VBuffer
    {
        VRange full_alloc_range;
        VRange header_alloc_range;
        VRange buffer_alloc_range;
        u64 alloc_hdr_start;
        u64 buffer_start;
    };
    // return type for get_alloc_vranges_from_ptr to return multiple values
    struct AllocedVRanges
    {
        VRange header_alloc_range;
        VRange buffer_alloc_range;
    };

    VSpace();
    VSpace(VRange);
    ~VSpace();

    static void init_kernel_vspace();

    // allocations should always be page aligned, otherwise there could be situations where 2 or more
    // buffers overlap on the same page, which would be messier to implement

    static u64 worst_case_size(u64, u64);
    AllocedVRanges get_alloc_vranges_from_ptr(vaddr);
    VBuffer alloc_vbuffer(u64, u64);

    // used by kmalloc and kfree
    vaddr allocate_size(u64, u64);
    void free_size(vaddr);
    
    // used by VObject map and unmap
    vaddr allocate_vobj(VObject *vobj);
    void free_vobj(VObject *vobj, vaddr addr);
    vaddr allocate_pages(const Vector<paddr>&, u64);
    void free_pages(const Vector<paddr>&, vaddr);

    // for testing/debugging purposes
    u64 get_free_space();

    void share_kernelspace();
    void unshare_kernelspace();

    void reload_cr3_if_needed();
};

bool in_kernel_vspace();
PML4T *kernel_pml4t();

struct VObject
{
    struct VSpaceMapping
    {
        VSpace *vspace = 0;
        vaddr alloced_addr = 0;
    };
    Vector<paddr> underlying_pages{}; // TODO this may need to be switched to an entirely physical page based linked list
    Vector<VSpaceMapping> vspaces_mapped_in = {};
    u64 alignment = 0;
    VObject();
    VObject(u64, u64);
    ~VObject();
    vaddr map(VSpace&);
    void unmap(VSpace&);
    static void initialize_interrupt_stack(u64, u64);
};
