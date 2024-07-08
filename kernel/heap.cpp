// TODO this was an attempt at making a heap allocator (which I stopped when I came up with a better solution)
//      this code can maybe be repurposed if I have need for a heap allocator (e.g. for a userspace malloc implementation)
/*
struct VSpace
{
    struct AllocHeader
    {
        // 0 indicates end or start of list
        // note that this list will only be traversed in following ways:
        //  - from start to end in one direction
        //  - check the prev and next of the current node
        AllocHeader *next;
        AllocHeader *prev;
        bool is_free;
        u64 alloc_size;
    };

    VRange allocation_space;
    AllocHeader *vranges = 0;

    VSpace(VRange alloc_space)
    {
        alloction_space = alloc_space;
        ASSERT(is_aligned(allocation_space.addr, 4096));
        paddr page = alloc_phys_page();
        VRange first_page = {
            allocation_space.addr,
            allocation_space.length
        };
        map_vrange(first_page, kernel_pml4t);
        vranges = (AllocHeader *)allocation_space.addr;
        vranges->next = 0;
        vranges->prev = 0;
        vranges->is_free = true;
        vranges->alloc_size = allocation_space.length;
    }

    // TODO update vranges * if ptr is at start
    vaddr allocate(u64 size, u64 alignment)
    {
        //u64 non_exact_fit_size = size + sizeof()
        // TODO things to take into account
        //  - is this an exact fit or not
        
        // this is called 'worst_case' based off the alignment-1 term, the worst case (biggest size) will be if 
        //  - the buffer needs to start alignment-1 bytes into the free vrange
        //  - the end of the buffer needs to be pushed 4096-1 bytes forward to align it to a page boundary
        // NOTE the + 4096 is to account for a page where we can put the FreeVAllocHeader
        u64 worst_case_size = sizeof(AllocHeader) + alignment-1 + size + 4096-1;

        auto range_hdr = vranges;
        while(range_hdr != 0) {
            if(range_hdr->is_free && range_hdr->alloc_size >= worst_case_size) {
                u64 alloc_start = (u64)range_hdr;
                ASSERT(is_aligned(alloc_start, 4096));
                u64 buffer_start = alloc_start + sizeof(AllocHeader);
                buffer_start = round_up_align(buffer_start, alignment);
                u64 buffer_end = buffer_start + size;
                u64 alloc_end = round_up_align(buffer_end, 4096);
                ASSERT(is_aligned(alloc_end, 4096));

                u64 alloc_size = alloc_end - alloc_start;
                ASSERT(alloc_size <= worst_case_size);

                VRange alloc_range = {
                    alloc_start,
                    alloc_size
                };
                VRange first_page = {
                    alloc_start,
                    4096
                };
                // TODO
                // copy next & prev pointers from header that will be deallocated
                auto old_hdr_next = range_hdr->next;
                auto old_hdr_prev = range_hdr->prev;
                //  de-allocator header we just used
                unmap_vrange(first_page, kernel_pml4t);

                // allocate header for our allocated space
                map_vrange(alloc_range, kernel_pml4t);
                auto new_alloc_hdr = (AllocHeader *)alloc_range.addr;
                new_alloc_hdr->next = old_hdr_next;
                new_alloc_hdr->prev = old_hdr_prev;
                new_alloc_hdr->is_free = false;
                new_alloc_hdr->alloc_size = alloc_size;

                // TODO is this necessary?
                new_alloc_hdr->next->prev = new_alloc_hdr;
                new_alloc_hdr->prev->next = new_alloc_hdr;

                // allocate space for next free-header
                if(alloc_size != range_hdr->alloc_size) { // if sizes match, there is no free space to next item
                    VRange next_alloc_header = {
                        alloc_range.one_past_end(),
                        4096
                    };
                    map_vrange(next_alloc_header, kernel_pml4t);
                    auto new_free_hdr = (AllocHeader *)next_alloc_header.addr;
                    new_free_hdr->next = old_hdr_next;
                    new_free_hdr->prev = new_alloc_hdr;
                    new_free_hdr->is_free = true;
                    new_free_hdr->alloc_size

                    new_free_hdr->next->prev = new_free_hdr;
                    new_free_hdr->prev->next = new_free_hdr;
                }

                // NOTE there will never be a prev header that is free, free regions are always combined on deallocation

                return buffer_start;
            }

            range_hdr = range_hdr->next;
        }
    }

    // TODO update vranges * if ptr is at start
    void free(vaddr ptr)
    {
        ASSERT(used_vranges != 0);
        u64 alloc_start = round_down_align(ptr, 4096);
        auto alloc_hdr = (AllocHeader *)alloc_start;
        VRange alloc_range = {
            (u64)alloc_hdr,
            alloc_hdr->alloc_size
        };

        alloc_hdr->is_free = true;

        // check if next is free, deallocate it and coalesc into this hdr, link up headers
        auto next_hdr = alloc_hdr->next;
        if(next_hdr && next_hdr->is_free) {
            alloc_hdr->next = next_hdr->next;
            alloc_hdr->next->prev = alloc_hdr;

            alloc_hdr->alloc_size += next_hdr->alloc_size;

            VRange next_hdr_range = {
                (u64)next_hdr,
                next_hdr->alloc_size
            };
            unmap_vrange(next_hdr_range, kernel_pml4t);
        }

        // if prev is free, deallocate this and coalesc into prev hdr, link up headers, return
        auto prev_hdr = alloc_hdr->prev;
        if(prev_hdr && prev_hdr->is_free) {
            prev_hdr->next = alloc_hdr->next;
            prev_hdr->next->prev = prev_hdr;

            prev_hdr->alloc_size += alloc_hdr->alloc_size;

            unmap_vrange(alloc_range, kernel_pml4t);
            return;
        }

        // unmap existing alloc_hdr range, alloc new freelist header, link up headers, return
        VRange alloc_first_page = {
            (u64)alloc_hdr,
            4096
        };
        auto alloc_hdr_cpy = *alloc_hdr;
        unmap_vrange(alloc_range, kernel_pml4t);
        map_vrange(alloc_first_page, kernel_pml4t);
        *alloc_hdr = alloc_hdr_cpy;
    }
};
*/
