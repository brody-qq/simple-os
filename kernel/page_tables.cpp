#pragma once
#include "kernel/page_tables.h"
#include "kernel/range.h"
#include "kernel/physical_allocator.h"
#include "kernel/kernel_defs.h"

// vaddr with 4-level paging and 4KB pages:
//      | PML4T index | PDPT index | PD index  | PT index  | page index   |
//      | 9 bits      | 9 bits     | 9 bits    | 9 bits    | 12 bits      |
//      | xxxxxxxxx   | xxxxxxxxx  | xxxxxxxxx | xxxxxxxxx | xxxxxxxxxxxx |
u64 pml4t_index(vaddr addr)
{
    return (addr >> (9+9+9+12)) & 0x1ffu;
}

u64 pdpt_index(vaddr addr)
{
    return (addr >> (9+9+12)) & 0x1ffu;
}

u64 pd_index(vaddr addr)
{
    return (addr >> (9+12)) & 0x1ffu;
}

u64 pt_index(vaddr addr)
{
    return (addr >> 12) & 0x1ffu;
}

u64 page_index(vaddr addr)
{
    return addr & 0xfff;
}

void PML4TE::set_phys_addr(paddr addr) { 
    ASSERT((addr & ~phys_addr_mask) == 0);
    raw &= ~phys_addr_mask;
    raw |= (addr & phys_addr_mask);
}
u64 PML4TE::get_phys_addr() { return raw & phys_addr_mask; }
void PML4TE::clear() { *this = {}; }

void PDPTE::set_phys_addr(paddr addr) { 
    ASSERT((addr & ~phys_addr_mask) == 0);
    raw &= ~phys_addr_mask;
    raw |= (addr & phys_addr_mask);
}
u64 PDPTE::get_phys_addr() { return raw & phys_addr_mask; }
void PDPTE::clear() { *this = {}; }

void PDE::set_phys_addr(paddr addr) { 
    ASSERT((addr & ~phys_addr_mask) == 0);
    raw &= ~phys_addr_mask;
    raw |= (addr & phys_addr_mask);
}
u64 PDE::get_phys_addr() { return raw & phys_addr_mask; }
void PDE::clear() { *this = {}; }

void PDEMaps2MBPage::set_phys_addr(paddr addr) { 
    ASSERT((addr & ~phys_addr_mask) == 0);
    raw &= ~phys_addr_mask;
    raw |= (addr & phys_addr_mask);
}
u64 PDEMaps2MBPage::get_phys_addr() { return raw & phys_addr_mask; }
void PDEMaps2MBPage::clear() { *this = {}; }

void PTE::set_phys_addr(paddr addr) { 
    ASSERT((addr & ~phys_addr_mask) == 0);
    raw &= ~phys_addr_mask;
    raw |= (addr & phys_addr_mask);
}
u64 PTE::get_phys_addr() { return raw & phys_addr_mask; }
void PTE::clear() { *this = {}; }


PML4T::~PML4T()
{
    UNREACHABLE(); // TODO unimplemented (this may be better to implement outside the class)
}
void PML4T::clear()
{
    // TODO is this faster as a memset()
    for(int i = 0; i < entry_count; ++i) {
        entries[i].clear();
    }
}
bool PML4T::is_empty()
{
    for(int i = 0; i < entry_count; ++i)
        if(entries[i].bitfield.present)
            return false;
    return true;
}

PDPT::~PDPT()
{
    //UNREACHABLE(); // TODO unimplemented (this may be better to implement outside the class)
}
void PDPT::clear()
{
    // TODO is this faster as a memset()
    for(int i = 0; i < entry_count; ++i) {
        entries[i].clear();
    }
}
bool PDPT::is_empty()
{
    for(int i = 0; i < entry_count; ++i)
        if(entries[i].bitfield.present)
            return false;
    return true;
}

PD::~PD()
{
    //UNREACHABLE(); // TODO unimplemented (this may be better to implement outside the class)
}
void PD::clear()
{
    // TODO is this faster as a memset()
    for(int i = 0; i < entry_count; ++i) {
        entries[i].clear();
    }
}
bool PD::is_empty()
{
    for(int i = 0; i < entry_count; ++i)
        if(entries[i].bitfield.present)
            return false;
    return true;
}

PDMaps2MBPages::~PDMaps2MBPages()
{
    //UNREACHABLE(); // TODO unimplemented (this may be better to implement outside the class)
}
void PDMaps2MBPages::clear()
{
    // TODO is this faster as a memset()
    for(int i = 0; i < entry_count; ++i) {
        entries[i].clear();
    }
}
bool PDMaps2MBPages::is_empty()
{
    for(int i = 0; i < entry_count; ++i)
        if(entries[i].bitfield.present)
            return false;
    return true;
}

bool PT::is_empty()
{
    for(int i = 0; i < entry_count; ++i)
        if(entries[i].bitfield.present) // TODO are there any problems with using present == 1 as the metric for if a table entry can be discarded?
            return false;
    return true;
}

PML4T *current_pml4t()
{
    return (PML4T *)read_cr3();
}
// -------------------------------------------------
// TODO this should go with the page table code
// TODO I should make a generic iterator for page tables...
// TODO in future the following features will likely need to be added:
//      - map a vrange to a specific (non-contiguous) set of physical pages (this will require ref counting pages)

// TODO map_vrange and unmap_vrange functions could easily be a performance bottleneck, can probably be sped up by
//      reducing branching and iterating each table (4kb page) in one go, removing some
//      of the redundant indexing that happens since the upper tables are indexed each time
//      through the loop, even though those tables change infrequently. Can also batch all phys_page_allocations
//      into one go
//      can also probably be optimized by using 128 bit, 256 bit, 512 bit registers if available, page table
//      entries are 64 bits large, so multiple can be put in one register and then masked to see if they
//      are present or not

void map_vrange(VRange vrange, PML4T *pml4t_to_map)
{
    ASSERT(vrange.addr >= KERNEL_VSPACE_START);
    dbg_str("map_vrange()\n");
    dbg_str("vrange.addr: "); dbg_uint(vrange.addr); dbg_str(" vrange.length: "); dbg_uint(vrange.length); dbg_str("\n");
    // TODO cleanup this code
    u64 vaddr = vrange.addr;
    u64 one_past_end = vrange.one_past_end();
    PML4T& pml4t = *pml4t_to_map;
    while(vaddr < one_past_end) {
        dbg_str("vaddr: "); dbg_uint(vaddr); dbg_str("\n");

        u64 pml4t_i = pml4t_index(vaddr);
        PML4TE& pml4te = pml4t[pml4t_i];
        if(!pml4te.bitfield.present) {
            u64 new_page = alloc_phys_page();
            *(PDPT *)new_page = PDPT{};

            pml4te.clear();
            pml4te.bitfield.present = 1;
            pml4te.bitfield.writable = 1;
            pml4te.set_phys_addr(new_page);
        }

        PDPT& pdpt = *(PDPT *)pml4te.get_phys_addr();
        u64 pdpt_i = pdpt_index(vaddr);
        PDPTE& pdpte = pdpt[pdpt_i];
        if(!pdpte.bitfield.present) {
            u64 new_page = alloc_phys_page();
            *(PD *)new_page = PD{};

            pdpte.clear();
            pdpte.bitfield.present = 1;
            pdpte.bitfield.writable = 1;
            pdpte.set_phys_addr(new_page);
        }

        PD& pd = *(PD *)pdpte.get_phys_addr();
        u64 pd_i = pd_index(vaddr);
        PDE& pde = pd[pd_i];
        if(!pde.bitfield.present) {
            u64 new_page = alloc_phys_page();
            // TODO is __builtin_memset faster than this?
            *(PT *)new_page = PT{};

            pde.clear();
            pde.bitfield.present = 1;
            pde.bitfield.writable = 1;
            pde.set_phys_addr(new_page);
            dbg_str("map_in 1\n");
        }

        PT& pt = *(PT *)pde.get_phys_addr();
        u64 pt_i = pt_index(vaddr);
        PTE& pte = pt[pt_i];

        ASSERT(!pte.bitfield.present); // this means the phys_page has already been mapped! something is wrong with vspace allocator
        u64 new_page = alloc_phys_page();

        pte.clear();
        pte.bitfield.present = 1;
        pte.bitfield.writable = 1;
        pte.set_phys_addr(new_page);

        vaddr += 4096;
    }
    ASSERT(vaddr == one_past_end);
}

// TODO this doesn't check if the next table is present or not before traversing it
void unmap_vrange(VRange vrange, PML4T *pml4t_to_unmap)
{
    ASSERT(vrange.addr >= KERNEL_VSPACE_START);
    dbg_str("unmap_vrange()\n");
    dbg_str("vrage.addr: "); dbg_uint(vrange.addr); dbg_str(" vrange.length: "); dbg_uint(vrange.length); dbg_str("\n");
    // TODO cleanup this code
    u64 vaddr = vrange.addr;
    u64 one_past_end = vrange.one_past_end();
    PML4T& pml4t = *pml4t_to_unmap;
    while(vaddr < one_past_end) {
        dbg_str("vaddr: "); dbg_uint(vaddr); dbg_str("\n");

        u64 pml4t_i = pml4t_index(vaddr);
        PML4TE& pml4te = pml4t[pml4t_i];
        ASSERT(pml4te.bitfield.present);

        PDPT& pdpt = *(PDPT *)pml4te.get_phys_addr();
        u64 pdpt_i = pdpt_index(vaddr);
        PDPTE& pdpte = pdpt[pdpt_i];
        ASSERT(pdpte.bitfield.present);

        PD& pd = *(PD *)pdpte.get_phys_addr();
        u64 pd_i = pd_index(vaddr);
        PDE& pde = pd[pd_i];
        ASSERT(pde.bitfield.present);

        PT& pt = *(PT *)pde.get_phys_addr();
        u64 pt_i = pt_index(vaddr);
        PTE& pte = pt[pt_i];
        ASSERT(pte.bitfield.present);

        free_phys_page((paddr)pte.get_phys_addr());
        pte.clear();

        vaddr += 4096;

        // TODO is_empty() iterates over all entries in the page table
        //      there are surely more efficient ways of doing this?
        //      also see the comments in struct PT for possible ways to
        //      speed up is_empty()
        if(pt.is_empty()) {
            dbg_str("is_empty() 1\n");
            free_phys_page(pde.get_phys_addr());
            pde.clear();
        }
        if(pd.is_empty()) {
            dbg_str("is_empty() 2\n");
            free_phys_page(pdpte.get_phys_addr());
            pdpte.clear();
        }
        if(pdpt.is_empty()) {
            dbg_str("is_empty() 3\n");
            free_phys_page(pml4te.get_phys_addr());
            pml4te.clear();
        }
    }
    ASSERT(vaddr == one_past_end);
}

// TODO this currently does not reference count pages, so there is room for error here if this is called
//      when there are pages shared between multiple vspaces
void free_page_tables(PML4T *pml4t_addr)
{
    dbg_str("free_page_tables()\n");

    PML4T& pml4t = *pml4t_addr;
    for(int pml4t_i = 0; pml4t_i < PML4T::entry_count; ++pml4t_i)
    {
        PML4TE& pml4te = pml4t.entries[pml4t_i];
        if(!pml4te.bitfield.present) continue;

        u64 pdpt_addr = pml4te.get_phys_addr();
        PDPT& pdpt = *(PDPT *)pdpt_addr;

        for(int pdpt_i = 0; pdpt_i < PDPT::entry_count; ++pdpt_i)
        {
            PDPTE& pdpte = pdpt.entries[pdpt_i];
            if(!pdpte.bitfield.present) continue;

            u64 pd_addr = pdpte.get_phys_addr();
            PD& pd = *(PD *)pd_addr;

            for(int pd_i = 0; pd_i < PD::entry_count; ++pd_i)
            {
                PDE& pde = pd.entries[pd_i];
                if(!pde.bitfield.present) continue;

                u64 pt_addr = pde.get_phys_addr();
                PT& pt = *(PT *)pt_addr;

                for(int pt_i = 0; pt_i < PT::entry_count; ++pt_i)
                {
                    PTE& pte = pt.entries[pt_i];
                    if(!pte.bitfield.present) continue;

                    u64 page_addr = pte.get_phys_addr();

                    free_phys_page(page_addr);

                }

                free_phys_page(pt_addr);

            }

            free_phys_page(pd_addr);

        }

        free_phys_page(pdpt_addr);

    }

    free_phys_page((u64)pml4t_addr);
}

// --------------------------------------------------------------------------------------------------------
// TODO this is almost identical to the map_vrange and unmap_vrange above except using a Vector of page addresses
//      instead of allocating the pages on demand, coalesce both into one pair of functions (or at least make a common
//      iterator over page tables)
void map_vrange(VRange vrange, PML4T *pml4t_to_map, const Vector<paddr>& pages_to_map)
{
    ASSERT(vrange.addr >= KERNEL_VSPACE_START);
    dbg_str("map_vrange()\n");
    dbg_str("vrange.addr: "); dbg_uint(vrange.addr); dbg_str(" vrange.length: "); dbg_uint(vrange.length); dbg_str("\n");
    // TODO cleanup this code
    u64 vaddr = vrange.addr;
    u64 one_past_end = vrange.one_past_end();
    PML4T& pml4t = *pml4t_to_map;
    u32 phys_page_index = 0;
    while(vaddr < one_past_end) {
        dbg_str("vaddr: "); dbg_uint(vaddr); dbg_str("\n");

        u64 pml4t_i = pml4t_index(vaddr);
        PML4TE& pml4te = pml4t[pml4t_i];
        if(!pml4te.bitfield.present) {
            u64 new_page = alloc_phys_page();
            *(PDPT *)new_page = PDPT();

            pml4te.clear();
            pml4te.bitfield.present = 1;
            pml4te.bitfield.writable = 1;
            pml4te.set_phys_addr(new_page);
        }

        PDPT& pdpt = *(PDPT *)pml4te.get_phys_addr();
        u64 pdpt_i = pdpt_index(vaddr);
        PDPTE& pdpte = pdpt[pdpt_i];
        if(!pdpte.bitfield.present) {
            u64 new_page = alloc_phys_page();
            *(PD *)new_page = PD();

            pdpte.clear();
            pdpte.bitfield.present = 1;
            pdpte.bitfield.writable = 1;
            pdpte.set_phys_addr(new_page);
        }

        PD& pd = *(PD *)pdpte.get_phys_addr();
        u64 pd_i = pd_index(vaddr);
        PDE& pde = pd[pd_i];
        if(!pde.bitfield.present) {
            u64 new_page = alloc_phys_page();
            // TODO is __builtin_memset faster than this?
            *(PT *)new_page = PT();

            pde.clear();
            pde.bitfield.present = 1;
            pde.bitfield.writable = 1;
            pde.set_phys_addr(new_page);
            dbg_str("map_in 1\n");
        }

        PT& pt = *(PT *)pde.get_phys_addr();
        u64 pt_i = pt_index(vaddr);
        PTE& pte = pt[pt_i];

        ASSERT(!pte.bitfield.present); // this means the phys_page has already been mapped! something is wrong with vspace allocator
        u64 new_page = (u64)pages_to_map[phys_page_index++];

        pte.clear();
        pte.bitfield.present = 1;
        pte.bitfield.writable = 1;
        pte.set_phys_addr(new_page);

        vaddr += 4096;
    }
    ASSERT(vaddr == one_past_end);
    ASSERT(phys_page_index == pages_to_map.length);
}

void unmap_vrange(VRange vrange, PML4T *pml4t_to_unmap, const Vector<paddr>& pages_to_unmap)
{
    ASSERT(vrange.addr >= KERNEL_VSPACE_START);
    dbg_str("unmap_vrange()\n");
    dbg_str("vrage.addr: "); dbg_uint(vrange.addr); dbg_str(" vrange.length: "); dbg_uint(vrange.length); dbg_str("\n");
    // TODO cleanup this code
    u64 vaddr = vrange.addr;
    u64 one_past_end = vrange.one_past_end();
    PML4T& pml4t = *pml4t_to_unmap;
    u32 phys_page_index = 0;
    while(vaddr < one_past_end) {
        dbg_str("vaddr: "); dbg_uint(vaddr); dbg_str("\n");

        u64 pml4t_i = pml4t_index(vaddr);
        PML4TE& pml4te = pml4t[pml4t_i];
        ASSERT(pml4te.bitfield.present);

        PDPT& pdpt = *(PDPT *)pml4te.get_phys_addr();
        u64 pdpt_i = pdpt_index(vaddr);
        PDPTE& pdpte = pdpt[pdpt_i];
        ASSERT(pdpte.bitfield.present);

        PD& pd = *(PD *)pdpte.get_phys_addr();
        u64 pd_i = pd_index(vaddr);
        PDE& pde = pd[pd_i];
        ASSERT(pde.bitfield.present);

        PT& pt = *(PT *)pde.get_phys_addr();
        u64 pt_i = pt_index(vaddr);
        PTE& pte = pt[pt_i];
        ASSERT(pte.bitfield.present);

        ASSERT(pte.get_phys_addr() == (u64)pages_to_unmap[phys_page_index++]);
        pte.clear();

        vaddr += 4096;

        // TODO is_empty() iterates over all entries in the page table
        //      there are surely more efficient ways of doing this?
        //      also see the comments in struct PT for possible ways to
        //      speed up is_empty()
        if(pt.is_empty()) {
            dbg_str("is_empty() 1\n");
            free_phys_page(pde.get_phys_addr());
            pde.clear();
        }
        if(pd.is_empty()) {
            dbg_str("is_empty() 2\n");
            free_phys_page(pdpte.get_phys_addr());
            pdpte.clear();
        }
        if(pdpt.is_empty()) {
            dbg_str("is_empty() 3\n");
            free_phys_page(pml4te.get_phys_addr());
            pml4te.clear();
        }
    }
    ASSERT(vaddr == one_past_end);
    ASSERT(phys_page_index == pages_to_unmap.length);
}
// --------------------------------------------------------------------------------------------------------

// this is for debugging purposes
bool is_page_mapped_pmap(paddr addr, PML4T *pml4t_to_map)
{
    // TODO cleanup this code
    u64 base_addr = 0;
    u64 nine_bits = 0x1ff;
    PML4T& pml4t = *pml4t_to_map;

    for(u64 pml4t_i = base_addr;
        pml4t_i <= nine_bits;
        ++pml4t_i)
    {
        auto& pml4te = pml4t[pml4t_i];
        if(!pml4te.bitfield.present)
            continue;
        PDPT& pdpt = *(PDPT *)pml4te.get_phys_addr();

        for(u64 pdpt_i = base_addr;
            pdpt_i <= nine_bits;
            ++pdpt_i)
        {
            auto& pdpte = pdpt[pdpt_i];
            if(!pdpte.bitfield.present)
                continue;
            PDMaps2MBPages& pd = *(PDMaps2MBPages *)pdpte.get_phys_addr();

            for(u64 pd_i = base_addr;
                pd_i <= nine_bits;
                ++pd_i)
            {
                auto& pde2mb = pd[pd_i];
                if(!pde2mb.bitfield.present)
                    continue;

                if(pde2mb.get_phys_addr() <= addr && pde2mb.get_phys_addr() + 2*MB > addr) {
                    return true;
                }

            }

        }

    }

    return false;
}
