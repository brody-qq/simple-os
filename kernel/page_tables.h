#pragma once
#include "kernel/utils.h"
#include "kernel/debug.cpp"

u64 pml4t_index(vaddr addr);

u64 pdpt_index(vaddr addr);

u64 pd_index(vaddr addr);

u64 pt_index(vaddr addr);

u64 page_index(vaddr addr);


const u64 bytes_mapped_by_page = 4*KB;
const u64 bytes_mapped_by_pt = 512*bytes_mapped_by_page;
const u64 bytes_mapped_by_pd = 512*bytes_mapped_by_pt;
const u64 bytes_mapped_by_pdpt = 512*bytes_mapped_by_pd;
const u64 bytes_mapped_by_pml4t = 512*bytes_mapped_by_pdpt;

// Definitions of 64-bit page structures are in Intel Software Developer's manual Volume 3. Chapter 4

// TODO: make print() or to_str() function for debug purposes
// NOTE: the difference between _reserved and _ignored:
//  - _reserved MUST be set to 0, can cause hardware exceptions otherwise
//  - _ignored can be set to anything by software, useful for copy-on-write pages
union PML4TE
{
    struct {
        u64 present : 1;
        u64 writable : 1;
        u64 usermode_access : 1;
        u64 page_write_through : 1;
        u64 page_cache_disable : 1;
        u64 accessed : 1;
        u64 _ignored1 : 1;
        u64 _reserved1 : 1;
        u64 _ignored2 : 3;

    // keep this as 0, this is only used for CPU VM features
        u64 hlat_restart : 1; 

    // upper [12, (48-1)] = 36 bits of phys_addr (next entry must be 4kb aligned)
        u64 phys_addr : 36; 

    // bits [48, 51] = 4 bits are reserved
        u64 _reserved2 : 4;
        u64 _ignored3 : 11;
        u64 execute_disable : 1;
    } bitfield;

    u64 raw;

    static const u64 phys_addr_mask = 0x0000fffffffff000;
    PML4TE() : raw(0) {}
    PML4TE(u64 raw_val) : raw(raw_val) {}
    void set_phys_addr(paddr addr);
    u64 get_phys_addr();
    void clear();
};
static_assert(sizeof(PML4TE) == sizeof(u64), "PML4TE is incorrect size, must be 8 bytes");
static_assert(sizeof(decltype(PML4TE{}.bitfield)) == sizeof(u64), "PML4TE.bitfield is incorrect size, must be 8 bytes");
static_assert(sizeof(decltype(PML4TE{}.raw)) == sizeof(u64), "PML4TE.raw is incorrect size, must be 8 bytes");

union PDPTE
{
    struct {
        u64 present : 1;
        u64 writable : 1;
        u64 usermode_access : 1;
        u64 page_write_through : 1;
        u64 page_cache_disable : 1;
        u64 accessed : 1;
        u64 _ignored1 : 1;

    // keep this as 0, if set, this means this PDPTE maps a 1GB page
        u64 page_size : 1;

        u64 _ignored2 : 3;

    // keep this as 0, this is only used for CPU VM features
        u64 hlat_restart : 1; 

    // upper [12, (48-1)] = 36 bits of phys_addr (next entry must be 4kb aligned)
        u64 phys_addr : 36; 

    // bits [48, 51] = 4 bits are reserved
        u64 _reserved2 : 4;
        u64 _ignored3 : 11;
        u64 execute_disable : 1;
    } bitfield;

    u64 raw;

    static const u64 phys_addr_mask = 0x0000fffffffff000;
    PDPTE() : raw(0) {}
    PDPTE(u64 raw_val) : raw(raw_val) {}
    void set_phys_addr(paddr addr);
    u64 get_phys_addr();
    void clear();
};
static_assert(sizeof(PDPTE) == sizeof(u64), "PDPTE is incorrect size, must be 8 bytes");
static_assert(sizeof(decltype(PDPTE{}.bitfield)) == sizeof(u64), "PDPTE.bitfield is incorrect size, must be 8 bytes");
static_assert(sizeof(decltype(PDPTE{}.raw)) == sizeof(u64), "PDPTE.raw is incorrect size, must be 8 bytes");

// a PDE that maps a page table
union PDE {
    struct {
        u64 present : 1;
        u64 writable : 1;
        u64 usermode_access : 1;
        u64 page_write_through : 1;
        u64 page_cache_disable : 1;
        u64 accessed : 1;
        u64 _ignored1 : 1;

    // keep this as 0, if set, this means this PDE maps a 2MB page
        u64 page_size : 1;

        u64 _ignored2 : 3;

    // keep this as 0, this is only used for CPU VM features
        u64 hlat_restart : 1; 

    // upper [12, (48-1)] = 36 bits of phys_addr (next entry must be 4kb aligned)
        u64 phys_addr : 36; 

    // bits [48, 51] = 4 bits are reserved
        u64 _reserved2 : 4;
        u64 _ignored3 : 11;
        u64 execute_disable : 1;
    } bitfield;

    u64 raw;

    static const u64 phys_addr_mask = 0x0000fffffffff000;
    PDE() : raw(0) {}
    PDE(u64 raw_val) : raw(raw_val) {}
    void set_phys_addr(paddr addr);
    u64 get_phys_addr();
    void clear();
};
static_assert(sizeof(PDE) == sizeof(u64), "PDE is incorrect size, must be 8 bytes");
static_assert(sizeof(decltype(PDE{}.bitfield)) == sizeof(u64), "PDE.bitfield is incorrect size, must be 8 bytes");
static_assert(sizeof(decltype(PDE{}.raw)) == sizeof(u64), "PDE.raw is incorrect size, must be 8 bytes");

// a PDE that maps a 2MB page
// TODO rename
// NOTE these will only be used for the pmap, so we make them a separate type instead of merging them with the PDE type
union PDEMaps2MBPage {
    struct {
        u64 present : 1;
        u64 writable : 1;
        u64 usermode_access : 1;
        u64 page_write_through : 1;
        u64 page_cache_disable : 1;
        u64 accessed : 1; // set by cpu if software accesses page
        u64 dirty : 1; // set by cpu if software writes to page

    // must be set to 1, which means this entry maps a page
        u64 page_size : 1;

    // ignored if page_size is not set
        u64 global : 1;

        u64 _ignored2 : 2;

    // keep this as 0, this is only used for CPU VM features
        u64 hlat_restart : 1; 

        u64 page_attribute_table : 1;
        u64 _reserved1 : 8;

    // upper [21, (48-1)] = 27 bits of phys_addr (page is 2MB (21 bit) aligned)
        u64 phys_addr : 27; 

    // bits [48, 51] = 4 bits are reserved
        u64 _reserved2 : 4;
        u64 _ignored3 : 7;

    // ignored if CR4 protection key bits are not enabled
        u64 protection_key : 4;

        u64 execute_disable : 1;
    } bitfield;

    u64 raw;

    static const u64 phys_addr_mask = 0x0000ffffffe00000;
    static const u64 page_size_bit = 0x0000000000000080; // PS bit set
    //PDEMaps2MBPage() : raw(page_size_bit) {}
    PDEMaps2MBPage() : raw(0) {}
    PDEMaps2MBPage(u64 raw_val) : raw(raw_val) { ASSERT(raw_val & page_size_bit); }
    void set_phys_addr(paddr addr);
    u64 get_phys_addr();
    void clear();
};
static_assert(sizeof(PDEMaps2MBPage) == sizeof(u64), "PDEMaps2MBPage is incorrect size, must be 8 bytes");
static_assert(sizeof(decltype(PDEMaps2MBPage{}.bitfield)) == sizeof(u64), "PDEMaps2MBPage.bitfield is incorrect size, must be 8 bytes");
static_assert(sizeof(decltype(PDEMaps2MBPage{}.raw)) == sizeof(u64), "PDEMaps2MBPage.raw is incorrect size, must be 8 bytes");

union PTE {
    struct {
        u64 present : 1;
        u64 writable : 1;
        u64 usermode_access : 1;
        u64 page_write_through : 1;
        u64 page_cache_disable : 1;
        u64 accessed : 1; // set by cpu if software accesses page
        u64 dirty : 1; // set by cpu if software writes to page
        u64 page_attribute_table : 1;

    // ignored if page_size is not set
        u64 global : 1;

        u64 _ignored1 : 2;

    // keep this as 0, this is only used for CPU VM features
        u64 hlat_restart : 1; 

    // upper [21, (48-1)] = 36 bits of phys_addr (page is 4KB (12 bit) aligned)
        u64 phys_addr : 36; 

    // bits [48, 51] = 4 bits are reserved
        u64 _reserved2 : 4;
        u64 _ignored2 : 7;

    // ignored if CR4 protection key bits are not enabled
        u64 protection_key : 4;

        u64 execute_disable : 1;
    } bitfield;

    u64 raw;

    // TODO these methods are duplicated in all page table structs
    static const u64 phys_addr_mask = 0x0000fffffffff000;
    PTE() : raw(0) {}
    PTE(u64 raw_val) : raw(raw_val) {}
    void set_phys_addr(paddr addr);
    u64 get_phys_addr();
    void clear();
};
static_assert(sizeof(PTE) == sizeof(u64), "PTE is incorrect size, must be 8 bytes");
static_assert(sizeof(decltype(PTE{}.bitfield)) == sizeof(u64), "PTE.bitfield is incorrect size, must be 8 bytes");
static_assert(sizeof(decltype(PTE{}.raw)) == sizeof(u64), "PTE.raw is incorrect size, must be 8 bytes");

// TODO these page table types are almost identical
struct alignas(4096) PML4T {
    static const int entry_count = 512;
    PML4TE entries[entry_count] = {};
    PML4TE& operator[](size_t index) { return entries[index]; }
    bool is_empty();
    void clear();
    ~PML4T();
};
static_assert(sizeof(PML4T) == 4096, "PML4T is incorrect size, must be 4096 bytes");
static_assert(alignof(PML4T) == 4096, "PML4T has incorrect alignment, must be 4096 bytes");

struct alignas(4096) PDPT {
    static const int entry_count = 512;
    PDPTE entries[entry_count] = {};
    PDPTE& operator[](size_t index) { return entries[index]; }
    bool is_empty();
    void clear();
    ~PDPT();
};
static_assert(sizeof(PDPT) == 4096, "PDPT is incorrect size, must be 4096 bytes");
static_assert(alignof(PDPT) == 4096, "PDPT has incorrect alignment, must be 4096 bytes");

struct alignas(4096) PD {
    static const int entry_count = 512;
    PDE entries[entry_count] = {};
    PDE& operator[](size_t index) { return entries[index]; }
    bool is_empty();
    void clear();
    ~PD();
};
static_assert(sizeof(PD) == 4096, "PD is incorrect size, must be 4096 bytes");
static_assert(alignof(PD) == 4096, "PD has incorrect alignment, must be 4096 bytes");

struct alignas(4096) PDMaps2MBPages {
    static const int entry_count = 512;
    PDEMaps2MBPage entries[entry_count] = {};
    PDEMaps2MBPage& operator[](size_t index) { return entries[index]; }
    bool is_empty();
    void clear();
    ~PDMaps2MBPages();
};
static_assert(sizeof(PDMaps2MBPages) == 4096, "PDMaps2MBPages is incorrect size, must be 4096 bytes");
static_assert(alignof(PDMaps2MBPages) == 4096, "PDMaps2MBPages has incorrect alignment, must be 4096 bytes");

// TODO assert are aligned, this has already bitten me!
struct alignas(4096) PT {
    static const int entry_count = 512;
    PTE entries[entry_count] = {};
    PTE& operator[](size_t index) { return entries[index]; }
    // TODO is_empty can be sped up by keeping a int that counts how many entries are used
    //      then is_empty(); is just 'return count == 0'
    //      however this will require using methods that increment and decrement the count
    //      everytime a entry is written with a non-zero value
    // TODO or... this could treat the array as u64, iterate over the entire array looking
    //      for a non-zero value, and if it doesn't find one then is_empty(); == true
    // TODO or... use a bitmap to track which entries are present and which are not, when the 
    //      bitmap == 0 then the table is empty
    bool is_empty();
};
static_assert(sizeof(PT) == 4096, "PT is incorrect size, must be 4096 bytes");
static_assert(alignof(PT) == 4096, "PT has incorrect alignment, must be 4096 bytes");

PML4T *current_pml4t();
