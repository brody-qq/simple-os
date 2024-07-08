#pragma once
#include "kernel/types.h"
#include "kernel/debug.cpp"
#include "kernel/ata.cpp"
#include "include/math.h"
#include "kernel/kmalloc.h"
#include "include/stdlib_workaround.h"
#include "include/math.h"
#include "include/filesystem_defs.h"

// https://www.nongnu.org/ext2-doc/ext2.html#s-block-group-nr

// https://wiki.osdev.org/MBR_(x86)
const u16 MBR_MAGIC_NUM = 0xaa55; // this is backwards because little endian
struct __attribute__((__packed__)) MBREntry
{
    u8 drive_attributes;
    u8 chs_partition_start[3];
    u8 partition_type;
    u8 chs_partition_last[3];
    u32 start_lba;
    u32 sector_count;
};
static_assert(sizeof(MBREntry) == 16, "MBREntry must be 16 bytes");

struct __attribute__((__packed__)) MBRSector
{
    u8 _bootstrap_code[440];
    u32 disk_id;
    u16 _reserved1;
    MBREntry partitions[4];
    u16 magic_num;
};
static_assert(sizeof(MBRSector) == 512, "MBRSector must be same size as drive sector");

const u16 EXT2_MAGIC_NUM = 0xef53;

const u16 EXT2_STATE_VALID_FS = 1;
const u16 EXT2_STATE_ERROR_FS = 2;

const u16 EXT2_ERRORS_CONTINUE = 1;
const u16 EXT2_ERRORS_READONLY = 2;
const u16 EXT2_ERRORS_PANIC = 3;

const u32 EXT2_FEATURE_COMPAT_DIR_PREALLOC = 0x01;
const u32 EXT2_FEATURE_COMPAT_IMAGIC_INODES = 0x02;
const u32 EXT2_FEATURE_COMPAT_HAS_JOURNAL = 0x04;
const u32 EXT2_FEATURE_COMPAT_EXT_ATTR = 0x08;
const u32 EXT2_FEATURE_COMPAT_RESIZE_INO = 0x10; // non standard inode size used
const u32 EXT2_FEATURE_COMPAT_DIR_INDEX = 0x20; // directory hash tree indexing enabled

const u32 EXT2_FEATURE_INCOMPAT_COMPRESSION = 0x01;
const u32 EXT2_FEATURE_INCOMPAT_DIR_ENTRY_TYPE = 0x02; // directory entries use the type field
const u32 EXT2_FEATURE_INCOMPAT_REPLAY_JOURNAL = 0x04; // file system must replay journal
const u32 EXT2_FEATURE_INCOMPAT_JOURNAL_DEV = 0x08; // file system uses a journal device

const u32 EXT2_FEATURE_RO_COMPAT_SPARSE_SUPER = 0x01; // superblock copies only stored in block group 0, 1, and powers of 3, 5, 7
const u32 EXT2_FEATURE_RO_COMPAT_LARGE_FILE = 0x02; // 64 bit file size supported
const u32 EXT2_FEATURE_RO_COMPAT_BTREE_DIR = 0x04; // binary tree sorted directory files

const u32 EXT2_ROOT_DIR_INODE = 2; // inode index of the root directory

struct __attribute__((__packed__)) SuperBlock
{
    u32 total_inode_count;
    u32 total_block_count;
    u32 reserved_block_count; // blocks reserved for superuser
    u32 free_block_count;
    u32 free_inode_count;
    u32 first_data_block; // the first block of the first block group, this is 1 for 1KB block size or 0 for >1KB block size
    u32 log_block_size; // compute block size by 1024 << log_block_size
    s32 log_fragment_size; // comput frag size by 1024 << log_fragmanet_size or 1024 >> -log_fragment_size if negative
    u32 blocks_per_group;
    u32 frags_per_group;
    u32 inodes_per_group;
    u32 mtime;
    u32 wtime;
    u16 mount_count;
    u16 max_mount_count;
    u16 magic_num;
    u16 state; // set to ERROR_FS on mount, set to VALID_FS on unmount
    u16 errors;
    u16 minor_revision;
    u32 lastcheck;
    u32 check_interval;
    u32 creator_os;
    u32 major_revision;
    u16 reserved_uid; // default user id for reserved blocks
    u16 reserved_gid; // default group id for reserved blocks

    // revision 1 only:
    u32 first_non_reserved_inode;
    u16 inode_size;
    u16 block_group_hosting_this_superblock;
    u32 compatible_features;
    u32 incompatible_features;
    u32 read_only_compatible_features;
    u8 uuid[16];
    u8 volume_name[16];
    u8 path_last_mounted[64];
    u32 compression_algo_bitmap;

    // performance hints
    u8 prealloc_data_blocks;
    u8 prealloc_dir_blocks;
    u16 _padding1;

    // journaling support
    u8 journal_uuid[16];
    u32 journal_inum;
    u32 journal_dev;
    u32 last_orphan; // only used by ext3 journaling

    // directory indexing support
    u8 hash_seed[16];
    u8 def_hash_version;
    u8 _padding2[3];

    // other options
    u32 default_mount_options;
    u32 first_meta_block_group;
    u8 _reserved[760];
};
static_assert(sizeof(SuperBlock) == 1024, "Superblock must be 1024 (size of smallest block)");

struct __attribute__((__packed__)) BlockGroupDescriptor
{
    u32 block_bitmap;
    u32 inode_bitmap;
    u32 inode_table;
    u16 free_block_count;
    u16 free_inode_count;
    u16 used_dirs_count;
    u16 _padding1;
    u8 _reserved1[12];
};
static_assert(sizeof(BlockGroupDescriptor) == 32, "BlockGroupDescriptor must be 32 bytes");

const u16 EXT2_INODE_TYPE_REG_FILE = 0x8000;
const u16 EXT2_INODE_TYPE_DIR      = 0x4000;

const u16 EXT2_INODE_USER_READ     = 0x0100;
const u16 EXT2_INODE_USER_WRITE    = 0x0080;
const u16 EXT2_INODE_USER_EXEC     = 0x0040;
const u16 EXT2_INODE_GROUP_READ    = 0x0020;
const u16 EXT2_INODE_GROUP_WRITE   = 0x0010;
const u16 EXT2_INODE_GROUP_EXEC    = 0x0008;
const u16 EXT2_INODE_OTHER_READ    = 0x0004;
const u16 EXT2_INODE_OTHER_WRITE   = 0x0002;
const u16 EXT2_INODE_OTHER_EXEC    = 0x0001;
const u16 EXT2_INODE_ALL_PERMISSIONS = 0x1ff;

const u32 EXT2_INODE_DIRECT_BLOCK_COUNT = 12;
struct __attribute__((__packed__)) INode
{
    u16 mode; // file type and permissions
    u16 uid;
    u32 size_lo;
    u32 atime;
    u32 ctime;
    u32 mtime;
    u32 dtime;
    u16 gid;
    u16 hardlink_count; // reference count of directories pointing to this inode, if this is 0 then inode is unused/deleted

// number of data blocks reserved by this inode, can be larger than size_lo and size_hi would imply
// NOTE this is counted in 512 byte blocks for some reason, so must be converted in calculations
    u32 reserved_512_blocks;
    u32 flags; // flags indicating how operations on this file should work, e.g. compression, un-deletable, append-only, etc.
    u32 _osd1; // OS specific value

// pointers to data blocks
    u32 blocks[EXT2_INODE_DIRECT_BLOCK_COUNT];
    u32 indirect_block;
    u32 double_indirect_block;
    u32 triple_indirect_block;

    u32 generation; // used by NFS
    u32 file_acl;
    u32 size_hi; // in rev 0 this will always be 0, in rev 1 this is the high 4 bytes of the file size, but only for regular files
    u32 faddr; // fragmane address, unused by linux
    u8 _osd2[12]; // OS specific value

    u8 _reserved[128];
};
static_assert(sizeof(INode) == 256, "INode must be 256 bytes");

const u8 EXT2_DIR_FTYPE_UNKNOWN = 0;
const u8 EXT2_DIR_FTYPE_REG_FILE = 1;
const u8 EXT2_DIR_FTYPE_DIR = 2;
const u64 EXT2_DIR_MAX_NAME_LENGTH = 255;
struct __attribute__((__packed__)) DirectoryEntry
{
    u32 inode_num;

// NOTE: length of entire entry, must be aligned to 4, can be larger than the true entry size
//       if length is larger than the true entry size, the end of the entry must be padded with zeroes
    u16 length;     
    u8 name_length; // does not need to be aligned to 4 or null terminated
    u8 file_type;
    char name[0]; // NOTE: GCC does not count this towards sizeof(DirectoryEntry)
};
static_assert(sizeof(DirectoryEntry) == 8, "INode must be 8 bytes");

// TODO make WriteBack types like
// template<typename T>
// struct WriteBackBlocks
// {
//     u32 blocknum;
//     u32 blockcount;
//     T *blocks;
// };
//
// template<typename T>
// struct WriteBackSectors
// {
//     u64 lba;
//     u64 sectorcount;
//     T *sectors;
// };

// forward declares
u32 alloc_block();
void free_block(u32);
u32 alloc_inode();
u64 dir_entry_true_size(u32);
void writeback_inode_data_blocks(INode *, u8 *, u64, u64);

// TODO rename bg to 'group'
// TODO rename BlockGroupDescriptor to GroupDescriptor
// TODO split these vars into ones that exist just for writeback_ functions and ones that have other uses
BlockGroupDescriptor *g_group_desc_table_block = 0; // table of block groups descriptors
u32 g_group_count = 0; // number of block groups
u32 g_group_desc_block_count = 0;
u32 g_group_desc_table_blocknum = 0;

u32 g_block_size_bytes = 0;
u32 g_block_size_sectors = 0;
u32 g_superblock_start_block = 0;

// for the write_back_superblock(), since a superblock can be much smaller than the block that contains it
// TODO is it better to load the entire block into a kmalloc() buffer, and then access the superblock via a pointer into this block
SuperBlock g_superblock;
u32 g_superblock_start_lba = 0;
u32 g_superblock_size_sectors = 0;

u8 *g_block_bitmaps = 0;
u8 *g_inode_bitmaps = 0;

// TODO switch this to an array of WriteBack types instead of a flat kmalloc array?
INode *g_inode_table = 0;
u32 g_inode_table_blocks_per_group = 0;

MBRSector g_mbr;
MBREntry g_ext2_partition;

u64 g_block_ptrs_per_indirect_block = 0;
u64 g_block_ptrs_per_double_indirect_block = 0;
u64 g_block_ptrs_per_triple_indirect_block = 0;

u64 g_direct_index_highest;
u64 g_indirect_index_highest;
u64 g_double_indirect_index_highest;
u64 g_triple_indirect_index_highest;

u64 blocknum_to_lba(u32 blocknum)
{
    u64 lba = g_ext2_partition.start_lba + blocknum * g_block_size_sectors;
    return lba;
}
// NOTE: block_index is based off start of the partition, not start of the drive
// TODO these args are backwards compared to read_sectors_pio()
void read_blocks(u8 *buffer, u32 blocknum, u16 block_count)
{
    u64 lba = blocknum_to_lba(blocknum);
    u16 sector_count = block_count * g_block_size_sectors;
    ASSERT(sector_count > block_count); // TODO check overflow properly with GCC built ins
    read_sectors_pio(buffer, sector_count, lba);
}

// TODO these args are backwards compared to write_sectors_pio()
void write_blocks(u8 *buffer, u32 blocknum, u16 block_count)
{
    // TODO duplicated code with read_blocks
    u64 lba = blocknum_to_lba(blocknum);
    u16 sector_count = block_count * g_block_size_sectors;
    ASSERT(sector_count > block_count); // TODO check overflow properly with GCC built ins
    write_sectors_pio(buffer, sector_count, lba);
}

void init_ext2()
{
    read_sectors_pio((u8 *)&g_mbr, 1, 0);
    ASSERT(g_mbr.magic_num == MBR_MAGIC_NUM);
    g_ext2_partition = g_mbr.partitions[0];

    u32 superblock_size_bytes = sizeof(SuperBlock);
    ASSERT(superblock_size_bytes % 512 == 0);
    g_superblock_size_sectors = superblock_size_bytes / 512;
    g_superblock_start_lba = g_ext2_partition.start_lba + 2; // Superblock starts 1024 bytes (2 sectors) past the start of the partition
    read_sectors_pio((u8 *)&g_superblock, g_superblock_size_sectors, g_superblock_start_lba);

    ASSERT(g_superblock.magic_num == EXT2_MAGIC_NUM);
    ASSERT(g_superblock.major_revision == 1); // rest of code is written on the assumption this is ext2 revision 1
    u32 unsupported_features = EXT2_FEATURE_INCOMPAT_COMPRESSION | EXT2_FEATURE_INCOMPAT_REPLAY_JOURNAL | EXT2_FEATURE_INCOMPAT_JOURNAL_DEV;
    ASSERT((g_superblock.incompatible_features & unsupported_features) == 0);
    ASSERT(g_superblock.inode_size == sizeof(INode)); // INodes default to 256 bytes on mkfs for some reason...

    g_block_size_bytes = 1024 << g_superblock.log_block_size;
    g_block_size_sectors = g_block_size_bytes / 512;
    g_superblock_start_block = g_superblock.first_data_block;

    g_group_desc_table_blocknum = g_superblock_start_block + 1; // block group descriptor starts one block after the super block
    g_group_count = round_up_divide(g_superblock.total_block_count, g_superblock.blocks_per_group);
    u32 bg_desc_per_block = g_block_size_bytes / sizeof(BlockGroupDescriptor);
    g_group_desc_block_count = round_up_divide(g_group_count, bg_desc_per_block);
    g_group_desc_table_block = (BlockGroupDescriptor *)kmalloc(g_block_size_bytes * g_group_desc_block_count, 4096);
    read_blocks((u8 *)g_group_desc_table_block, g_group_desc_table_blocknum, g_group_desc_block_count);

    g_block_bitmaps = (u8 *)kmalloc(g_block_size_bytes * g_group_count, 4096);
    for(u32 i = 0; i < g_group_count; ++i) {
        u8 *curr_block_bitmap = g_block_bitmaps + i * g_block_size_bytes;
        u32 block_bitmap_blocknum = g_group_desc_table_block[i].block_bitmap;
        read_blocks(curr_block_bitmap, block_bitmap_blocknum, 1);
    }

    g_inode_bitmaps = (u8 *)kmalloc(g_block_size_bytes * g_group_count, 4096);
    for(u32 i = 0; i < g_group_count; ++i) {
        u8 *curr_inode_bitmap = g_inode_bitmaps + i * g_block_size_bytes;
        u32 inode_bitmap_blocknum = g_group_desc_table_block[i].inode_bitmap;
        read_blocks(curr_inode_bitmap, inode_bitmap_blocknum, 1);
    }

    // TODO inode table uses huge amount of memory, that scales linearly with the number of inodes on the drive
    //      (which I assume scales linearly with the size of the drive), this large inode table already takes up
    //      8MB for a 128MB drive, so larger drives cannot be used with this inode caching scheme
    g_inode_table_blocks_per_group = round_up_divide(g_superblock.inodes_per_group * g_superblock.inode_size, g_block_size_bytes);
    u32 inode_table_size_bytes = g_inode_table_blocks_per_group * g_group_count * g_block_size_bytes;
    g_inode_table = (INode *)kmalloc(inode_table_size_bytes, 4096);
    for(u32 group = 0; group < g_group_count; ++group) {
        u32 blocknum = g_group_desc_table_block[group].inode_table;

        u8 *ptr = (u8 *)g_inode_table + (group * g_inode_table_blocks_per_group * g_block_size_bytes);
        read_blocks(ptr, blocknum, g_inode_table_blocks_per_group);
    }

    g_block_ptrs_per_indirect_block = g_block_size_bytes / sizeof(u32); // TODO make a typedef for blocknum type
    g_block_ptrs_per_double_indirect_block = g_block_ptrs_per_indirect_block * g_block_ptrs_per_indirect_block;
    g_block_ptrs_per_triple_indirect_block = g_block_ptrs_per_indirect_block * g_block_ptrs_per_double_indirect_block;
    ASSERT(g_block_ptrs_per_triple_indirect_block > g_block_ptrs_per_double_indirect_block); // TODO do a proper overflow check with GCC builtins

    g_direct_index_highest = EXT2_INODE_DIRECT_BLOCK_COUNT;
    g_indirect_index_highest = g_direct_index_highest + g_block_ptrs_per_indirect_block;
    g_double_indirect_index_highest = g_indirect_index_highest + g_block_ptrs_per_double_indirect_block;
    g_triple_indirect_index_highest = g_double_indirect_index_highest + g_block_ptrs_per_triple_indirect_block;
}

// ---------------------------------------------------------------------------------------------------------
// NOTE when indexing the inode table, there is no guarantee that inode N+1 comes directly after
//      inode N in the table, there could be a gap if inode N is at the end of its' group's
//      inode table, so do NOT index the table using array indices without taking this into
//      account
struct GroupAndIndex
{
    u32 group = 0;
    u32 in_group_index = 0;
};
GroupAndIndex inode_num_to_group_and_index(u32 inode_num)
{
    u32 index = inode_num - 1;
    u32 group = index / g_superblock.inodes_per_group;
    u32 in_group_index = index % g_superblock.inodes_per_group;
    return { group, in_group_index };
}

INode *get_inode(u32 inode_num)
{
    auto res = inode_num_to_group_and_index(inode_num);
    u32 group = res.group;
    u32 in_group_index = res.in_group_index;

    INode *group_table = (INode *)((u8 *)g_inode_table + (group * g_inode_table_blocks_per_group * g_block_size_bytes));
    INode *found_inode = group_table + in_group_index;
    return found_inode;
}

u64 inode_size(INode *inode)
{
    u64 val = ((u64)inode->size_hi << 32) | (u64)inode->size_lo;
    return val;
}

void __inode_set_size(INode *inode, u64 size)
{
    inode->size_lo = (u32)(size & 0xffffffff);
    inode->size_hi = (u32)(size >> 32);
}

u32 inode_used_blocks(INode *inode)
{
    u64 size = inode_size(inode);
    u32 blockcount = round_up_divide(size, g_block_size_bytes);
    return blockcount;
}

u32 inode_reserved_blocks(INode *inode)
{
    u32 reserved_blocks = inode->reserved_512_blocks / g_block_size_sectors;
    return reserved_blocks;
}

/*
u32 inode_get_blocknum_from_index(INode *inode, u64 index)
{
    u32 blocknum = 0;
    if(index < g_direct_index_highest) {
        blocknum = inode->blocks[index];
    } else {
        u32 *l3_buffer, *l2_buffer, *l1_buffer;
        u64 l3_i, l2_i, l1_i;
        u32 *l3_blocknum_slot = 0, *l2_blocknum_slot = 0, *l1_blocknum_slot = 0;
        bool has_l3  = false, has_l2 = false;

        if(index < g_indirect_index_highest) { // TODO untested path
            index -= g_direct_index_highest;
            l1_i = index;

            l1_blocknum_slot = &inode->indirect_block;
        } else if(index < g_double_indirect_index_highest) { // TODO untested path
            index -= g_indirect_index_highest;
            l2_i = index / g_block_ptrs_per_indirect_block;
            l1_i = index % g_block_ptrs_per_indirect_block;

            l2_blocknum_slot = &inode->double_indirect_block;
            has_l2 = true;
        } else if(index < g_triple_indirect_index_highest) { // TODO untested path
            index -= g_double_indirect_index_highest;
            l3_i = index / g_block_ptrs_per_double_indirect_block;
            u64 l3_i_remainder = index % g_block_ptrs_per_double_indirect_block;
            l2_i = l3_i_remainder / g_block_ptrs_per_indirect_block;
            l1_i = l3_i_remainder % g_block_ptrs_per_indirect_block;

            l3_blocknum_slot = &inode->triple_indirect_block;
            has_l3 = has_l2 = true;
        } else {
            UNREACHABLE();
        }

        if(has_l3) {
            l3_buffer = (u32 *)kmalloc(g_block_size_bytes, 4096);
            ASSERT(*l3_blocknum_slot);
            read_blocks((u8 *)l3_buffer, *l3_blocknum_slot, 1);
            l2_blocknum_slot = l3_buffer + l3_i;
        }

        if(has_l2) {
            l2_buffer = (u32 *)kmalloc(g_block_size_bytes, 4096);
            ASSERT(*l2_blocknum_slot);
            read_blocks((u8 *)l2_buffer, *l2_blocknum_slot, 1);
            l1_blocknum_slot = l2_buffer + l2_i;
        }

        l1_buffer = (u32 *)kmalloc(g_block_size_bytes, 4096);
        ASSERT(*l1_blocknum_slot);
        read_blocks((u8 *)l1_buffer, *l1_blocknum_slot, 1);

        blocknum = l1_buffer[l1_i];

        if(has_l3) {
            write_blocks((u8 *)l3_buffer, *l3_blocknum_slot, 1);
            kfree((vaddr)l3_buffer);
        }

        if(has_l2) {
            write_blocks((u8 *)l2_buffer, *l2_blocknum_slot, 1);
            kfree((vaddr)l2_buffer);
        }

        write_blocks((u8 *)l1_buffer, *l1_blocknum_slot, 1);
        kfree((vaddr)l1_buffer);
    }

    return blocknum;
}
*/

bool all_zero(u8 *buffer, u64 size)
{
    for(u64 i = 0; i < size; ++i) {
        if(buffer[i] != 0)
            return false;
    }
    return true;
}

/*
// TODO code duplication with inode_get_blocknum_from_index
void __inode_set_blocknum_at_index(INode *inode, u64 index, u32 blocknum)
{
    if(index < g_direct_index_highest) {
        inode->blocks[index] = blocknum;
    } else {
        u32 *l3_buffer, *l2_buffer, *l1_buffer;
        u64 l3_i, l2_i, l1_i;
        u32 *l3_blocknum_slot = 0, *l2_blocknum_slot = 0, *l1_blocknum_slot = 0;
        bool has_l3  = false, has_l2 = false;

        if(index < g_indirect_index_highest) { // TODO untested path
            index -= g_direct_index_highest;
            l1_i = index;

            l1_blocknum_slot = &inode->indirect_block;
        } else if(index < g_double_indirect_index_highest) { // TODO untested path
            index -= g_indirect_index_highest;
            l2_i = index / g_block_ptrs_per_indirect_block;
            l1_i = index % g_block_ptrs_per_indirect_block;

            l2_blocknum_slot = &inode->double_indirect_block;
            has_l2 = true;
        } else if(index < g_triple_indirect_index_highest) { // TODO untested path
            index -= g_double_indirect_index_highest;
            l3_i = index / g_block_ptrs_per_double_indirect_block;
            u64 l3_i_remainder = index % g_block_ptrs_per_double_indirect_block;
            l2_i = l3_i_remainder / g_block_ptrs_per_indirect_block;
            l1_i = l3_i_remainder % g_block_ptrs_per_indirect_block;

            l3_blocknum_slot = &inode->triple_indirect_block;
            has_l3 = has_l2 = true;
        } else {
            UNREACHABLE();
        }

        if(has_l3) {
            l3_buffer = (u32 *)kmalloc(g_block_size_bytes, 4096);
            if(*l3_blocknum_slot == 0) {
                *l3_blocknum_slot = alloc_block();
                memset_workaround(l3_buffer, 0, g_block_size_bytes);
                write_blocks((u8 *)l3_buffer, *l3_blocknum_slot, 1);
            } else {
                read_blocks((u8 *)l3_buffer, *l3_blocknum_slot, 1);
            }
            l2_blocknum_slot = l3_buffer + l3_i;
        }

        if(has_l2) {
            l2_buffer = (u32 *)kmalloc(g_block_size_bytes, 4096);
            if(*l2_blocknum_slot == 0) {
                *l2_blocknum_slot = alloc_block();
                memset_workaround(l2_buffer, 0, g_block_size_bytes);
                write_blocks((u8 *)l2_buffer, *l2_blocknum_slot, 1);
            } else {
                read_blocks((u8 *)l2_buffer, *l2_blocknum_slot, 1);
            }
            l1_blocknum_slot = l2_buffer + l2_i;
        }

        l1_buffer = (u32 *)kmalloc(g_block_size_bytes, 4096);
        if(*l1_blocknum_slot == 0) {
            *l1_blocknum_slot = alloc_block();
            memset_workaround(l1_buffer, 0, g_block_size_bytes);
        } else {
            read_blocks((u8 *)l1_buffer, *l1_blocknum_slot, 1);
        }

        l1_buffer[l1_i] = blocknum;
        write_blocks((u8 *)l1_buffer, *l1_blocknum_slot, 1);

        if(all_zero((u8 *)l1_buffer, g_block_size_bytes)) {
            free_block(*l1_blocknum_slot);
            *l1_blocknum_slot = 0;

            if(has_l2 && all_zero((u8 *)l2_buffer, g_block_size_bytes)) {
                free_block(*l2_blocknum_slot);
                *l2_blocknum_slot = 0;

                if(has_l3 && all_zero((u8 *)l3_buffer, g_block_size_bytes)) {
                    free_block(*l3_blocknum_slot);
                    *l3_blocknum_slot = 0;
                }
            }
        }

        kfree((vaddr)l1_buffer);
        if(has_l2) {
            kfree((vaddr)l2_buffer);
        }
        if(has_l3) {
            kfree((vaddr)l3_buffer);
        }
    }
}
*/
void indirect_blocks_indexes(u64 i, u64& l1_i)
{
    l1_i = i - g_direct_index_highest;
}
void double_indirect_blocks_indexes(u64 i, u64& l2_i, u64& l1_i)
{
    l2_i = (i - g_indirect_index_highest) / g_block_ptrs_per_indirect_block;
    l1_i = (i - g_indirect_index_highest) % g_block_ptrs_per_indirect_block;
}
void triple_indirect_blocks_indexes(u64 i, u64& l3_i, u64& l2_i, u64& l1_i)
{
    l3_i = (i - g_double_indirect_index_highest) / g_block_ptrs_per_double_indirect_block;
    u64 remainder = (i - g_double_indirect_index_highest) % g_block_ptrs_per_double_indirect_block;
    l2_i = remainder / g_block_ptrs_per_indirect_block;
    l1_i = remainder % g_block_ptrs_per_indirect_block;
}

inline void ensure_slot(u32& blocknum_slot, u32 *buffer)
{
    if(blocknum_slot == 0) {
        memset_workaround(buffer, 0, g_block_size_bytes);
        blocknum_slot = alloc_block();
    } else {
        read_blocks((u8 *)buffer, blocknum_slot, 1);
    }
}
void __inode_ensure_blocks_impl(INode *inode, u64 block_index, u64 blockcount)
{
    u64 i = block_index;
    u64 end = i + blockcount;

// ----------------------------------------------------------------------------

    for(; i < end && i < EXT2_INODE_DIRECT_BLOCK_COUNT; ++i) {
        inode->blocks[i] = alloc_block();
    }
    if(i >= end)
        return;

// ----------------------------------------------------------------------------

    u32 *l3_buffer, *l2_buffer, *l1_buffer;
    u64 l1_i, l2_i, l3_i;
    u64 ptrcount = g_block_ptrs_per_indirect_block;
    u64 blocksize = g_block_size_bytes;
    u32 l3_blocknum, l2_blocknum, l1_blocknum;
// ----------------------------------------------------------------------------

    l1_buffer = (u32 *)kmalloc(blocksize, 4096);
    l1_blocknum = inode->indirect_block;
    ensure_slot(l1_blocknum, l1_buffer);
    inode->indirect_block = l1_blocknum;

    indirect_blocks_indexes(i, l1_i);
    for(;
        i < end && i < g_indirect_index_highest && l1_i < ptrcount;
        ++l1_i)
    {
        l1_buffer[l1_i] = alloc_block();
        ++i;
    }

    write_blocks((u8 *)l1_buffer, inode->indirect_block, 1);
    if(i >= end)
        goto free_l1;

// ----------------------------------------------------------------------------

    l2_buffer = (u32 *)kmalloc(blocksize, 4096);
    l2_blocknum = inode->double_indirect_block;
    ensure_slot(l2_blocknum, l2_buffer);
    inode->double_indirect_block = l2_blocknum;

    double_indirect_blocks_indexes(i, l2_i, l1_i);
    for(;
        i < end && i < g_double_indirect_index_highest && l2_i < ptrcount;
        ++l2_i)
    {
        ensure_slot(l2_buffer[l2_i], l1_buffer);

        for(;
            i < end && i < g_double_indirect_index_highest && l1_i < ptrcount;
            ++l1_i)
        {
            l1_buffer[l1_i] = alloc_block();
            ++i;
        }

        write_blocks((u8 *)l1_buffer, l2_buffer[l2_i], 1);
    }

    write_blocks((u8 *)l2_buffer, inode->double_indirect_block, 1);
    if(i >= end)
        goto free_l2;

// ----------------------------------------------------------------------------

    l3_buffer = (u32 *)kmalloc(blocksize, 4096);
    l3_blocknum = inode->triple_indirect_block;
    ensure_slot(l3_blocknum, l3_buffer);
    inode->triple_indirect_block = l3_blocknum;

    triple_indirect_blocks_indexes(i, l3_i, l2_i, l1_i);
    for(;
        i < end && i < g_triple_indirect_index_highest && l3_i < ptrcount;
        ++l3_i)
    {
        ensure_slot(l3_buffer[l3_i], l2_buffer);

        for(;
            i < end && i < g_triple_indirect_index_highest && l2_i < ptrcount;
            ++l2_i)
        {
            ensure_slot(l2_buffer[l2_i], l1_buffer);

            for(;
                i < end && i < g_triple_indirect_index_highest && l1_i < ptrcount;
                ++l1_i)
            {
                l1_buffer[l1_i] = alloc_block();
                ++i;
            }

            write_blocks((u8 *)l1_buffer, l2_buffer[l2_i], 1);
        }

        write_blocks((u8 *)l2_buffer, l3_buffer[l3_i], 1);
    }

    write_blocks((u8 *)l3_buffer, inode->triple_indirect_block, 1);

// ----------------------------------------------------------------------------

    kfree((vaddr)l3_buffer);
    free_l2:
    kfree((vaddr)l2_buffer);
    free_l1:
    kfree((vaddr)l1_buffer);
}

inline void free_if_zero(u32 *buffer, u32& blocknum_slot)
{
    if(all_zero((u8 *)buffer, g_block_size_bytes)) {
        free_block(blocknum_slot);
        blocknum_slot = 0;
    } else {
        write_blocks((u8 *)buffer, blocknum_slot, 1);
    }
}
void __inode_discard_blocks_from_end_impl(INode *inode, u64 block_index, u64 blockcount)
{
    u64 i = block_index;
    u64 end = i + blockcount;

// ----------------------------------------------------------------------------

    for(; i < end && i < EXT2_INODE_DIRECT_BLOCK_COUNT; ++i) {
        free_block(inode->blocks[i]);
        inode->blocks[i] = 0;
    }
    if(i >= end)
        return;

// ----------------------------------------------------------------------------
    u32 *l3_buffer, *l2_buffer, *l1_buffer;
    u32 l3_blocknum, l2_blocknum, l1_blocknum;
    u64 l1_i, l2_i, l3_i;
    u64 ptrcount = g_block_ptrs_per_indirect_block;
    u64 blocksize = g_block_size_bytes;
// ----------------------------------------------------------------------------

    l1_buffer = (u32 *)kmalloc(blocksize, 4096);
    l1_blocknum = inode->indirect_block;
    ASSERT(l1_blocknum);
    read_blocks((u8 *)l1_buffer, l1_blocknum, 1);

    indirect_blocks_indexes(i, l1_i);
    for(;
        i < end && i < g_indirect_index_highest && l1_i < ptrcount;
        ++l1_i)
    {
        free_block(l1_buffer[l1_i]);
        l1_buffer[l1_i] = 0;
        ++i;
    }

    free_if_zero(l1_buffer, l1_blocknum);
    inode->indirect_block = l1_blocknum;
    if(i >= end)
        goto free_l1;

// ----------------------------------------------------------------------------

    l2_buffer = (u32 *)kmalloc(blocksize, 4096);
    l2_blocknum = inode->double_indirect_block;
    ASSERT(l2_blocknum);
    read_blocks((u8 *)l2_buffer, l2_blocknum, 1);

    double_indirect_blocks_indexes(i, l2_i, l1_i);
    for(;
        i < end && i < g_double_indirect_index_highest && l2_i < ptrcount;
        ++l2_i)
    {
        ASSERT(l2_buffer[l2_i]);
        read_blocks((u8 *)l1_buffer, l2_buffer[l2_i], 1);

        for(;
            i < end && i < g_double_indirect_index_highest && l1_i < ptrcount;
            ++l1_i)
        {
            free_block(l1_buffer[l1_i]);
            l1_buffer[l1_i] = 0;
            ++i;
        }

        free_if_zero(l1_buffer, l2_buffer[l2_i]);
    }

    free_if_zero(l2_buffer, l2_blocknum);
    inode->double_indirect_block = l2_blocknum;
    if(i >= end)
        goto free_l2;

// ----------------------------------------------------------------------------

    l3_buffer = (u32 *)kmalloc(blocksize, 4096);
    l3_blocknum = inode->triple_indirect_block;
    ASSERT(l3_blocknum);
    read_blocks((u8 *)l3_buffer, l3_blocknum, 1);

    triple_indirect_blocks_indexes(i, l3_i, l2_i, l1_i);
    for(;
        i < end && i < g_triple_indirect_index_highest && l3_i < ptrcount;
        ++l3_i)
    {
        ASSERT(l3_buffer[l3_i]);
        read_blocks((u8 *)l2_buffer, l3_buffer[l3_i], 1);

        for(;
            i < end && i < g_triple_indirect_index_highest && l2_i < ptrcount;
            ++l2_i)
        {
            ASSERT(l2_buffer[l2_i]);
            read_blocks((u8 *)l1_buffer, l2_buffer[l2_i], 1);

            for(;
                i < end && i < g_triple_indirect_index_highest && l1_i < ptrcount;
                ++l1_i)
            {
                free_block(l1_buffer[l1_i]);
                l1_buffer[l1_i] = 0;
                ++i;
            }

            free_if_zero(l1_buffer, l2_buffer[l2_i]);
        }

        free_if_zero(l2_buffer, l3_buffer[l3_i]);
    }

    free_if_zero(l3_buffer, l3_blocknum);
    inode->triple_indirect_block = l3_blocknum;

// ----------------------------------------------------------------------------

    kfree((vaddr)l3_buffer);
    free_l2:
    kfree((vaddr)l2_buffer);
    free_l1:
    kfree((vaddr)l1_buffer);
}

void __inode_ensure_blocks(INode *inode, u64 count)
{
    u32 used = inode_used_blocks(inode);
    u32 reserved = inode_reserved_blocks(inode);
    ASSERT(reserved >= used);
    u32 unused = reserved - used;
    u32 needed = (count > unused) ? count - unused : 0;

    __inode_ensure_blocks_impl(inode, reserved, needed);

    inode->reserved_512_blocks += needed * g_block_size_sectors;
}

/*
u32 inode_next_block(INode *inode)
{
    u32 used = inode_used_blocks(inode);
    u32 reserved = inode_reserved_blocks(inode);
    int unused = reserved - used;
    ASSERT(unused > 0);
    return inode_get_blocknum_from_index(inode, used);
}
*/

void __inode_discard_blocks_from_end(INode *inode, u64 discard_count)
{
    u32 reserved = inode_reserved_blocks(inode);
    ASSERT(discard_count <= reserved);
    u32 start_index = reserved - discard_count;
    
    __inode_discard_blocks_from_end_impl(inode, start_index, discard_count);

    inode->reserved_512_blocks -= g_block_size_sectors * discard_count;
}

void inode_read_data_blocks(INode *inode, u8 *block_buffer, u64 blockcount, u64 block_index)
{
    u32 lastblock = block_index + blockcount;

    u32 used_blocks = inode_used_blocks(inode);
    ASSERT(lastblock <= used_blocks);

    u8 *ptr = block_buffer;
    u8 *end = ptr + g_block_size_bytes * blockcount;
    u64 i = block_index;
    for(;
        ptr < end && i < EXT2_INODE_DIRECT_BLOCK_COUNT;
        ++i)
    {
        u32 blocknum = inode->blocks[i];
        read_blocks(ptr, blocknum, 1);
        ptr += g_block_size_bytes;
    }
    if(ptr >= end)
        return;

// ----------------------------------------------------------------------------
    u32 *l3_buffer, *l2_buffer, *l1_buffer;
    u64 l1_i, l2_i, l3_i;
    u64 ptrcount = g_block_ptrs_per_indirect_block;
    u64 blocksize = g_block_size_bytes;
// ----------------------------------------------------------------------------

    l1_buffer = (u32 *)kmalloc(blocksize, 4096);
    read_blocks((u8 *)l1_buffer, inode->indirect_block, 1);

    indirect_blocks_indexes(i, l1_i);
    for(;
        ptr < end && i < g_indirect_index_highest && l1_i < ptrcount;
        ++l1_i)
    {
        u32 blocknum = l1_buffer[l1_i];
        read_blocks(ptr, blocknum, 1);
        ptr += blocksize;
        ++i;
    }
    if(ptr >= end)
        goto free_l1;
// ----------------------------------------------------------------------------

    l2_buffer = (u32 *)kmalloc(blocksize, 4096);
    read_blocks((u8 *)l2_buffer, inode->double_indirect_block, 1);

    double_indirect_blocks_indexes(i, l2_i, l1_i);
    for(;
        ptr < end && i < g_double_indirect_index_highest && l2_i < ptrcount;
        ++l2_i)
    {
        u32 l1_blocknum = l2_buffer[l2_i];
        read_blocks((u8 *)l1_buffer, l1_blocknum, 1);

        for(;
            ptr < end && i < g_double_indirect_index_highest && l1_i < ptrcount;
            ++l1_i)
        {
            u32 blocknum = l1_buffer[l1_i];
            read_blocks(ptr, blocknum, 1);
            ptr += blocksize;
            ++i;
        }
    }
    if(ptr >= end)
        goto free_l2;
// ----------------------------------------------------------------------------

    l3_buffer = (u32 *)kmalloc(blocksize, 4096);
    read_blocks((u8 *)l3_buffer, inode->triple_indirect_block, 1);

    triple_indirect_blocks_indexes(i, l3_i, l2_i, l1_i);
    for(;
        ptr < end && i < g_triple_indirect_index_highest && l3_i < ptrcount;
        ++l3_i)
    {
        u32 l2_blocknum = l3_buffer[l3_i];
        read_blocks((u8 *)l2_buffer, l2_blocknum, 1);

        for(;
            ptr < end && i < g_triple_indirect_index_highest && l2_i < ptrcount;
            ++l2_i)
        {
            u32 l1_blocknum = l2_buffer[l2_i];
            read_blocks((u8 *)l1_buffer, l1_blocknum, 1);

            for(;
                ptr < end && i < g_triple_indirect_index_highest && l1_i < ptrcount;
                ++l1_i)
            {
                u32 blocknum = l1_buffer[l1_i];
                read_blocks(ptr, blocknum, 1);
                ptr += blocksize;
                ++i;
            }
        }
    }
// ----------------------------------------------------------------------------

    kfree((vaddr)l3_buffer);
    free_l2:
    kfree((vaddr)l2_buffer);
    free_l1:
    kfree((vaddr)l1_buffer);
// ----------------------------------------------------------------------------
}

void __inode_init_dir(INode *inode, u32 inode_num, u32 parent_inode_num)
{
    ASSERT(inode_size(inode) == 0); // this fundtion should only be called on new uninitialized inodes

    __inode_ensure_blocks(inode, 1);
    u8 *buffer = (u8 *)kmalloc(g_block_size_bytes, 4096);
    memset_workaround(buffer, 0, g_block_size_bytes);

    u64 true_len = dir_entry_true_size(1);
    u64 remaining_len = g_block_size_bytes - true_len;

    u8 *ptr = buffer;
    auto entry = (DirectoryEntry *)ptr;
    entry->file_type = EXT2_DIR_FTYPE_DIR;
    entry->inode_num = inode_num;
    entry->length = true_len;
    entry->name_length = 1;
    memmove_workaround(entry->name, (void *)".", 1);

    ptr += entry->length;
    entry = (DirectoryEntry *)ptr;
    entry->file_type = EXT2_DIR_FTYPE_DIR;
    entry->inode_num = parent_inode_num;
    entry->length = remaining_len;
    entry->name_length = 2;
    memmove_workaround(entry->name, (void *)"..", 2);

    __inode_set_size(inode, g_block_size_bytes);
    writeback_inode_data_blocks(inode, buffer, 0, 1);
    kfree((vaddr)buffer);
}

// ---------------------------------------------------------------------------------------------------------
u64 dir_entry_true_size(u32 name_length)
{
// directory entry length must be aligned to 4 and padded with zeroes if there is extra space
    u64 true_size = sizeof(DirectoryEntry) + name_length;
    true_size = round_up_align(true_size, 4);
    return true_size;
}

bool dir_entry_name_match(DirectoryEntry *entry, const char *name, u64 name_len, bool is_dir)
{
    bool type_matches = !is_dir || entry->file_type == EXT2_DIR_FTYPE_DIR;
    bool is_match = type_matches && name_len == entry->name_length && (strncmp_workaround(name, entry->name, name_len) == 0);
    return is_match;
}
bool dir_entry_name_match(DirectoryEntry *entry, const char *name, u64 name_len)
{
    bool is_match = name_len == entry->name_length && (strncmp_workaround(name, entry->name, name_len) == 0);
    return is_match;
}
// ---------------------------------------------------------------------------------------------------------

// NOTE this is based on the assumption that an empty directory contains '.' and '..' entries, and nothing else.
//      this assumption holds up in practice, but I haven't confirmed if it is guaranteed in all cases
bool is_dir_empty(INode *dir)
{
    ASSERT(dir->mode & EXT2_INODE_TYPE_DIR);
    if(inode_used_blocks(dir) != 1)
        return false;

    u32 blockcount = inode_used_blocks(dir);
    u64 block_buffer_size = blockcount * g_block_size_bytes;
    u8 *block_buffer = (u8 *)kmalloc(blockcount * g_block_size_bytes, 4096);
    inode_read_data_blocks(dir, block_buffer, blockcount, 0);

    u8 *ptr = block_buffer;
    u8 *block_buffer_end = ptr + block_buffer_size;

    auto entry = (DirectoryEntry *)ptr;
    if(!dir_entry_name_match(entry, ".", 1, true))
        return false;

    ptr += entry->length;
    entry = (DirectoryEntry *)ptr;
    if(!dir_entry_name_match(entry, "..", 2, true))
        return false;

    ptr += entry->length;
    return ptr == block_buffer_end;
}

// ---------------------------------------------------------------------------------------------------------
void writeback_inode_bitmap(u32 group_i)
{
    u32 blocknum = g_group_desc_table_block[group_i].inode_bitmap;
    u8 *block_buffer = g_inode_bitmaps + group_i * g_block_size_bytes;
    write_blocks(block_buffer, blocknum, 1);
}

void writeback_block_bitmap(u32 group_i)
{
    u32 blocknum = g_group_desc_table_block[group_i].block_bitmap;
    u8 *block_buffer = g_block_bitmaps + group_i * g_block_size_bytes;
    write_blocks(block_buffer, blocknum, 1);
}

// TODO the group descriptor table in memory spans several blocks
//      this writes back all blocks instead of just the modified block (or sector)
void writeback_group_desc([[maybe_unused]] u32 group_i)
{
    u8 *block_buffer = (u8 *)g_group_desc_table_block;
    write_blocks(block_buffer, g_group_desc_table_blocknum, g_group_desc_block_count);
}

void writeback_superblock()
{
    u8 *ptr = (u8 *)&g_superblock;
    write_sectors_pio(ptr, g_superblock_size_sectors, g_superblock_start_lba);
}

// TODO performance can be improved by sorting the written blocknums before writing to see if 
//      any of them are contiguous
void writeback_inode_data_blocks(INode *inode, u8 *buffer, u64 block_index, u64 blockcount)
{
    u32 lastblock = block_index + blockcount;

    u32 reserved_blocks = inode_reserved_blocks(inode);
    ASSERT(lastblock <= reserved_blocks);

    u8 *ptr = buffer;
    u8 *end = ptr + g_block_size_bytes * blockcount;
    u64 i = block_index;
    for(; ptr < end && i < EXT2_INODE_DIRECT_BLOCK_COUNT; ++i) {
        u32 blocknum = inode->blocks[i];
        write_blocks(ptr, blocknum, 1);
        ptr += g_block_size_bytes;
    }
    if(ptr >= end)
        return;

// ----------------------------------------------------------------------------
    u32 *l3_buffer, *l2_buffer, *l1_buffer;
    u64 l1_i, l2_i, l3_i;
    u64 ptrcount = g_block_ptrs_per_indirect_block;
    u64 blocksize = g_block_size_bytes;
// ----------------------------------------------------------------------------

    l1_buffer = (u32 *)kmalloc(blocksize, 4096);
    read_blocks((u8 *)l1_buffer, inode->indirect_block, 1);

    indirect_blocks_indexes(i, l1_i);
    for(;
        ptr < end && i < g_indirect_index_highest && l1_i < ptrcount;
        ++l1_i)
    {
        write_blocks(ptr, l1_buffer[l1_i], 1);
        ptr += blocksize;
        ++i;
    }
    if(ptr >= end)
        goto free_l1;
// ----------------------------------------------------------------------------

    l2_buffer = (u32 *)kmalloc(blocksize, 4096);
    read_blocks((u8 *)l2_buffer, inode->double_indirect_block, 1);

    double_indirect_blocks_indexes(i, l2_i, l1_i);
    for(;
        ptr < end && i < g_double_indirect_index_highest && l2_i < ptrcount;
        ++l2_i)
    {
        read_blocks((u8 *)l1_buffer, l2_buffer[l2_i], 1);

        for(;
            ptr < end && i < g_double_indirect_index_highest && l1_i < ptrcount;
            ++l1_i)
        {
            u32 blocknum = l1_buffer[l1_i];
            write_blocks(ptr, blocknum, 1);
            ptr += blocksize;
            ++i;
        }
    }
    if(ptr >= end)
        goto free_l2;
// ----------------------------------------------------------------------------

    l3_buffer = (u32 *)kmalloc(blocksize, 4096);
    read_blocks((u8 *)l3_buffer, inode->triple_indirect_block, 1);

    triple_indirect_blocks_indexes(i, l3_i, l2_i, l1_i);
    for(;
        ptr < end && i < g_triple_indirect_index_highest && l3_i < ptrcount;
        ++l3_i)
    {
        read_blocks((u8 *)l2_buffer, l3_buffer[l3_i], 1);

        for(;
            ptr < end && i < g_triple_indirect_index_highest && l2_i < ptrcount;
            ++l2_i)
        {
            read_blocks((u8 *)l1_buffer, l2_buffer[l2_i], 1);

            for(;
                ptr < end && i < g_triple_indirect_index_highest && l1_i < ptrcount;
                ++l1_i)
            {
                write_blocks(ptr, l1_buffer[l1_i], 1);
                ptr += blocksize;
                ++i;
            }
        }
    }
// ----------------------------------------------------------------------------

    kfree((vaddr)l3_buffer);
    free_l2:
    kfree((vaddr)l2_buffer);
    free_l1:
    kfree((vaddr)l1_buffer);
}

void writeback_inode(u32 inode_num)
{
    auto res = inode_num_to_group_and_index(inode_num);
    u32 group = res.group;
    u32 in_group_index = res.in_group_index;

    u32 group_block = g_group_desc_table_block[group].inode_table;
    u64 group_lba = blocknum_to_lba(group_block);
    u64 inodes_per_sector = 512 / sizeof(INode);
    u64 lba = group_lba + in_group_index / inodes_per_sector;

    u8 *sector = ((u8 *)g_inode_table) + group * g_inode_table_blocks_per_group * g_block_size_bytes + (in_group_index / inodes_per_sector) * 512;
    write_sectors_pio(sector, 1, lba);
}
// ---------------------------------------------------------------------------------------------------------
// TODO make alloc_block and free_block count based?

struct BitmapFindZeroResult
{
    bool found_zero = false;
    u64 byte_i;
    u64 bit_i;
};
// TODO scan as u64 or higher for performance
BitmapFindZeroResult bitmap_find_zero_and_set(u8 *bitmap, u64 bitcount)
{
    bool found_zero = false;
    u8 *ptr = bitmap;
    u64 bytecount = bitcount / 8; // NOTE this ignores the last byte if bitcount is not a multiple of 8
    u64 bits_overflow = bitcount % 8;
    u64 byte_i = 0;
    u64 bit_i = 0;
    for(; byte_i < bytecount; ++byte_i) {
        u8 byte = ~(ptr[byte_i]);
        if(byte) {
            found_zero = true;
            bit_i = __builtin_ffs(byte) -1;
            break;
        }
    }

    if(!found_zero && bits_overflow) {
        u8 byte = ~(ptr[byte_i]);
        bit_i = __builtin_ffs(byte);
        if(bit_i == 0 || bit_i > bits_overflow)
            return {false, 0, 0};
        else
            bit_i--;
    }

    u8 *byte = ptr + byte_i;
    ASSERT((*byte & (1 << bit_i)) == 0);
    *byte |= (1 << bit_i);

    return {true, byte_i, bit_i};
}

u32 alloc_inode()
{
    u8 *base_ptr = g_inode_bitmaps;

    u64 group_i = 0;
    BitmapFindZeroResult res = {};
    for(group_i = 0; group_i < g_group_count; ++group_i) {
        if(g_group_desc_table_block[group_i].free_inode_count == 0)
            continue;

        u8 *ptr = base_ptr + group_i * g_block_size_bytes;
        res = bitmap_find_zero_and_set(ptr, g_superblock.inodes_per_group);

        if(res.found_zero)
            break;
    }
    ASSERT(res.found_zero);

    u64 byte_i = res.byte_i;
    u64 bit_i = res.bit_i;

    u32 inode_index = group_i * g_superblock.inodes_per_group + byte_i * 8 + bit_i;
    u32 inode_num = inode_index + 1;

    g_group_desc_table_block[group_i].free_inode_count--;
    g_superblock.free_inode_count--;

    // modified:
    //      inode bitmap
    //      group descriptor
    //      superblock
    writeback_inode_bitmap(group_i);
    writeback_group_desc(group_i);
    writeback_superblock();

    return inode_num;
}

u32 alloc_block()
{
    u8 *base_ptr = g_block_bitmaps;

    u64 group_i = 0;
    BitmapFindZeroResult res = {};
    for(group_i = 0; group_i < g_group_count; ++group_i) {
        if(g_group_desc_table_block[group_i].free_block_count == 0)
            continue;

        u8 *ptr = base_ptr + group_i * g_block_size_bytes;
        res = bitmap_find_zero_and_set(ptr, g_superblock.blocks_per_group);

        if(res.found_zero)
            break;
    }
    ASSERT(res.found_zero);

    u64 byte_i = res.byte_i;
    u64 bit_i = res.bit_i;

    u32 blocknum = group_i * g_superblock.blocks_per_group + byte_i * 8 + bit_i;

    g_group_desc_table_block[group_i].free_block_count--;
    g_superblock.free_block_count--;

    // modified:
    //      block bitmap
    //      group descriptor
    //      superblock
    writeback_block_bitmap(group_i);
    writeback_group_desc(group_i);
    writeback_superblock();

    return blocknum;
}

void bitmap_unset(u8 *bitmap, u64 index)
{
    u64 byte_i = index / 8;
    u64 bit_i = index % 8;
    
    u8 *byte = bitmap + byte_i;
    ASSERT(*byte & (1 << bit_i));
    *byte &= ~(1 << bit_i);
}

void free_block(u32 blocknum)
{
    u32 group_i = blocknum / g_superblock.blocks_per_group;
    u8 *bitmap = g_block_bitmaps + group_i * g_block_size_bytes;
    bitmap_unset(bitmap, blocknum);

    g_group_desc_table_block[group_i].free_block_count++;
    g_superblock.free_block_count++;

    // modified:
    //      block bitmap
    //      group descriptor
    //      superblock
    writeback_block_bitmap(group_i);
    writeback_group_desc(group_i);
    writeback_superblock();
}

void __free_inode(u32 inode_num)
{
    u32 inode_index = inode_num-1;
    
    u32 group_i = inode_index / g_superblock.inodes_per_group;
    u8 *bitmap = g_inode_bitmaps + group_i * g_block_size_bytes;
    bitmap_unset(bitmap, inode_index);

    g_group_desc_table_block[group_i].free_inode_count++;
    g_superblock.free_inode_count++;

    // modified:
    //      inode bitmap
    //      group descriptor
    //      superblock
    writeback_inode_bitmap(group_i);
    writeback_group_desc(group_i);
    writeback_superblock();
}
// ---------------------------------------------------------------------------------------------------------

struct PathLookupResult
{
    bool found_inode = false;
    u32 inode_num = 0;
};

// TODO implement a hash table lookup that caches path -> inode mappings
// TODO this currently goes through the whole loop even for redundant parts of the
//      path (e.g. a/b/././././c or a/b/,,/c), these redundant iterations can be
//      skipped by normalizing the path string first
PathLookupResult lookup_path(const char *path, u64 path_len)
{
    ASSERT(path);
    ASSERT(*path);
    bool is_abs_path = *path == '/';

    const char *part_start = path;
    for(; *part_start && *part_start == '/'; ++part_start) {} // skip redundant '/' in paths, e.g. /a///b
    const char *part_end = part_start +1;

    u32 curr_inode_num = 0;
    if(is_abs_path)
        curr_inode_num = EXT2_ROOT_DIR_INODE;
    else
        UNREACHABLE(); // relative paths should be handled before fs functions are called

    const char *str_end = path + path_len;
    while(*part_start && part_start != str_end) {
        INode *curr_dir = get_inode(curr_inode_num);
        u32 blockcount = inode_used_blocks(curr_dir);
        u8 *block_buffer = (u8 *)kmalloc(blockcount * g_block_size_bytes, 4096);
        inode_read_data_blocks(curr_dir, block_buffer, blockcount, 0);

        ASSERT(*part_start != '/'); // on loop entry, part_start is assumed to point to first non '/' character in the part
        part_end = part_start+1;
        for(; *part_end && *part_end != '/'; ++part_end) {}
        u64 part_len = ((u64)part_end - (u64)part_start);
        ASSERT(part_len < EXT2_DIR_MAX_NAME_LENGTH);
        bool is_dir = (*part_end == '/');

        u8 *base_ptr = block_buffer;
        u8 *ptr = base_ptr;
        bool found_entry = false;
        DirectoryEntry *entry = 0;
        while(ptr < base_ptr + g_block_size_bytes) {
            entry = (DirectoryEntry *)ptr;
            ASSERT(entry->name_length < EXT2_DIR_MAX_NAME_LENGTH);

            if(dir_entry_name_match(entry, part_start, part_len, is_dir)) {
                found_entry = true;
                break;
            }

            ptr += entry->length;
        }

        // TODO curr_inode_num is unecessary if entry ptr is pushed outside the outer while loop scope
        curr_inode_num = entry->inode_num; // this must come before the kfree()
        kfree((vaddr)block_buffer);

        if(!found_entry)
            return {};

        part_start = part_end;
        for(; *part_start && *part_start == '/'; ++part_start) {} // skip redundant '/' in paths, e.g. /a///b
    }

    return {true, curr_inode_num};
}

PathLookupResult lookup_path(const char *path)
{
    return lookup_path(path, strlen_workaround(path));
}
// ---------------------------------------------------------------------------------------------------------

// TODO make this print to a string buffer passed by the caller
int fs_list_dir_buffer_size(const char *path)
{
    auto lookupres = lookup_path(path);
    if(!lookupres.found_inode) {
        return FS_STATUS_FILE_NOT_FOUND;
    }
    INode *dir = get_inode(lookupres.inode_num);

    u32 blockcount = inode_used_blocks(dir);
    return blockcount * g_block_size_bytes;
}
u16 fs_list_dir(const char *path, char *buf, int buf_size, char **buf_one_past_end)
{
    auto lookupres = lookup_path(path);
    if(!lookupres.found_inode) {
        return FS_STATUS_FILE_NOT_FOUND;
    }
    INode *dir = get_inode(lookupres.inode_num);
    ASSERT(dir->mode & EXT2_INODE_TYPE_DIR);

    u32 blockcount = inode_used_blocks(dir);
    u8 *block_buffer = (u8 *)kmalloc(blockcount * g_block_size_bytes, 4096);
    inode_read_data_blocks(dir, block_buffer, blockcount, 0);

    u8 *base_ptr = block_buffer;
    u8 *entry_ptr = base_ptr;
    u8 *end = entry_ptr + g_block_size_bytes * blockcount;
    char *dest_ptr = buf;
    while(entry_ptr < end) {
        auto entry = (DirectoryEntry *)entry_ptr;
        ASSERT(entry->name_length < EXT2_DIR_MAX_NAME_LENGTH);

        ASSERT(dest_ptr + entry->name_length +1 <= buf + buf_size);
        int i = 0;
        for(; i < entry->name_length; ++i) {
            dest_ptr[i] = entry->name[i];
        }
        dest_ptr[i] = 0;
        dest_ptr += i+1;

        entry_ptr += entry->length;
    }
    *buf_one_past_end = dest_ptr;

    kfree((vaddr)block_buffer);
    return FS_STATUS_OK;
}

FileSizeResult fs_file_size(const char *path)
{
    auto lookup = lookup_path(path);
    if(!lookup.found_inode) {
        return {};
    }
    INode *inode = get_inode(lookup.inode_num);
    u64 size = inode_size(inode);
    return {true, size};
}

// TODO return nread
u16 fs_read(const char *path, u8 *buffer, u64 offset, u64 size)
{
    auto lookup = lookup_path(path);
    if(!lookup.found_inode) {
        return FS_STATUS_FILE_NOT_FOUND;
    }
    INode *inode = get_inode(lookup.inode_num);
    u64 filesize = inode_size(inode);

    if(offset >= filesize) {
        return FS_STATUS_BAD_ARG;
    }
    u64 offset_to_eof = filesize - offset;
    u64 bytes = min(size, offset_to_eof);
    u32 blockcount = round_up_divide(bytes, g_block_size_bytes);

    u32 startblock = offset / g_block_size_bytes;
    u64 block_offset = offset % g_block_size_bytes;
    ASSERT(block_offset + bytes <= blockcount * g_block_size_bytes);

    // TODO try to do this with no copies (use VObject)
    u8 *block_buffer = (u8 *)kmalloc(blockcount * g_block_size_bytes, 4096);
    inode_read_data_blocks(inode, block_buffer, blockcount, startblock);
    memmove_workaround(buffer, block_buffer + block_offset, bytes);
    int bytes_remaining = size - bytes; // NOTE bytes is guaranteed to be <= size
    ASSERT(bytes_remaining >= 0);
    memset_workaround(buffer + bytes, 0, bytes_remaining);
    kfree((vaddr)block_buffer);
    return FS_STATUS_OK;
}

// TODO move to string utils
int rindex(const char *str, char c, int strlen)
{
    int i = strlen-1;
    ASSERT(str[i]); // make sure strlen didn't go too far and hit null terminator
    while(str[i] != c) { --i; }
    return i;
}

inline void path_lengths(const char *path, int *path_len, int *parent_path_len)
{
    int i = strlen_workaround(path) -1;

    while(i >= 0 && path[i] == '/') { --i; } // skip '/'s at end of path, e.g. /a/b/ and /a/b////
    *path_len = i +1;

    while(i >= 0 && path[i] != '/') { --i; }
    *parent_path_len = i + 1;
}

struct NewINodeResult {
    u32 inode_num = 0;
    INode *inode;
};
inline NewINodeResult __new_inode(u32 parent_inode_num, bool is_dir)
{
    u32 inode_num = alloc_inode();
    INode *inode = get_inode(inode_num);
    memset_workaround((u8 *)inode, 0, g_superblock.inode_size);
    inode->hardlink_count = 1;
    if(is_dir) {
        inode->mode = EXT2_INODE_TYPE_DIR | EXT2_INODE_ALL_PERMISSIONS;
        __inode_init_dir(inode, inode_num, parent_inode_num);
    } else {
        inode->mode = EXT2_INODE_TYPE_REG_FILE | EXT2_INODE_ALL_PERMISSIONS;
    }
    return {inode_num, inode};
}
// TODO the code to make a directory entry can be separated out for fs_mv
u16 fs_create(const char *path, u16 type, u32 inode_to_hardlink = 0)
{
    int path_len;
    int parent_path_len;
    path_lengths(path, &path_len, &parent_path_len);

    const char *newname = path + parent_path_len;
    u64 newname_len = path_len - parent_path_len;
    ASSERT(*newname != '/');
    if(newname_len >= EXT2_DIR_MAX_NAME_LENGTH) {
        return FS_STATUS_BAD_ARG;
    }

    ASSERT(type == FS_TYPE_DIR || type == FS_TYPE_REG_FILE);
    bool is_dir = type == FS_TYPE_DIR;
    if(!is_dir && newname[newname_len] == '/') {
        return FS_STATUS_BAD_ARG;
    }

    auto lookupresult = lookup_path(path, parent_path_len);
    if(!lookupresult.found_inode) {
        return FS_STATUS_FILE_NOT_FOUND;
    }
    u32 parent_inode_num = lookupresult.inode_num;
    INode *parent = get_inode(parent_inode_num);

    u64 min_entry_size = dir_entry_true_size(newname_len);

    u32 blockcount = inode_used_blocks(parent);
    u64 block_buffer_size = blockcount * g_block_size_bytes;
    u8 *block_buffer = (u8 *)kmalloc(blockcount * g_block_size_bytes, 4096);
    inode_read_data_blocks(parent, block_buffer, blockcount, 0);

    const u32 CASE_SLOT_NOT_FOUND = 0;
    const u32 CASE_SLOT_FOUND = 1;
    const u32 CASE_FILE_ALREADY_EXISTS = 2;
// TODO this can search from last block first, since last block is more likely to have free space
    u32 search_result = CASE_SLOT_NOT_FOUND;
    u8 *base_ptr = block_buffer;
    u8 *block_buffer_end = base_ptr + block_buffer_size;
    u8 *ptr = base_ptr;
    u8 *found_slot_ptr = 0;
    u64 unused_len = 0;
    while(ptr < block_buffer_end) {
        auto entry = (DirectoryEntry *)ptr;
        ASSERT(entry->name_length < EXT2_DIR_MAX_NAME_LENGTH);

        if(dir_entry_name_match(entry, newname, newname_len)) {
            search_result = CASE_FILE_ALREADY_EXISTS;
            break;
        }

        u64 true_len = dir_entry_true_size(entry->name_length);
        unused_len = entry->length - true_len;
        ASSERT(unused_len % 4 == 0);
        if(search_result != CASE_SLOT_FOUND && unused_len >= min_entry_size) {
            // NOTE this does not end the loop, since there could be an entry that
            //      comes after this with the same filename as newname, which is
            //      an error condition
            u64 old_len = entry->length;
            entry->length = true_len;
            found_slot_ptr = ptr + true_len;
            ptr += old_len;

            search_result = CASE_SLOT_FOUND;
        } else {
            ptr += entry->length;
        }
    }

    u16 direntry_type = is_dir ? EXT2_DIR_FTYPE_DIR : EXT2_DIR_FTYPE_REG_FILE;

    u16 return_status;
    switch(search_result) {
        case(CASE_SLOT_FOUND): {
            u32 inode_num = 0;
            if(inode_to_hardlink) {
                inode_num = inode_to_hardlink;
                INode *i = get_inode(inode_num);
                i->hardlink_count++;
                if(is_dir)
                    ASSERT(i->mode & EXT2_INODE_TYPE_DIR);
                else 
                    ASSERT(i->mode & EXT2_INODE_TYPE_REG_FILE);
            } else {
                auto inode_res = __new_inode(parent_inode_num, is_dir);
                inode_num = inode_res.inode_num;
            }

            auto newentry = (DirectoryEntry *)found_slot_ptr;

            memset_workaround(found_slot_ptr, 0, unused_len);
            newentry->inode_num = inode_num;
            newentry->length = unused_len;
            newentry->name_length = newname_len;
            newentry->file_type = direntry_type;
            memmove_workaround(newentry->name, (void *)newname, newname_len);

            // modified:
            //      new inode
            //      parent inode dir entry block
            u64 block_index = (found_slot_ptr - base_ptr) / g_block_size_bytes;
            u8 *write_to_block = base_ptr + block_index * g_block_size_bytes;
            writeback_inode_data_blocks(parent, write_to_block, block_index, 1);
            writeback_inode(inode_num);

            return_status = FS_STATUS_OK;
        } break;
        case(CASE_SLOT_NOT_FOUND): {
            // TODO can attempt to shrink unused space in the directory and retry, before allocating a new block
            u32 inode_num = 0;
            if(inode_to_hardlink) {
                inode_num = inode_to_hardlink;
                INode *i = get_inode(inode_num);
                i->hardlink_count++;
                if(is_dir)
                    ASSERT(i->mode & EXT2_INODE_TYPE_DIR);
                else 
                    ASSERT(i->mode & EXT2_INODE_TYPE_REG_FILE);
            } else {
                auto inode_res = __new_inode(parent_inode_num, is_dir);
                inode_num = inode_res.inode_num;
            }

            u8 *buffer = (u8 *)kmalloc(g_block_size_bytes, 4096); // TODO this can re-use the existing block_buffer instead of kmallocing a new buffer
            auto newentry = (DirectoryEntry *)buffer;

            // TODO duplication with while() loop above
            memset_workaround(buffer, 0, g_block_size_bytes);
            newentry->inode_num = inode_num;
            newentry->length = g_block_size_bytes;
            newentry->name_length = newname_len;
            newentry->file_type = direntry_type;
            memmove_workaround(newentry->name, (void *)newname, newname_len);

            __inode_ensure_blocks(parent, 1);
            u32 newblock_index = inode_used_blocks(parent);
            u64 new_parent_size = inode_size(parent) + g_block_size_bytes;
            __inode_set_size(parent, new_parent_size);

            // modified:
            //      new inode
            //      parent inode new block
            //      parent inode
            writeback_inode(parent_inode_num);
            writeback_inode_data_blocks(parent, buffer, newblock_index, 1);
            writeback_inode(inode_num);
            kfree((vaddr)buffer);

            return_status = FS_STATUS_OK;
        } break;
        case(CASE_FILE_ALREADY_EXISTS): {
            return_status = FS_STATUS_FILE_ALREADY_EXISTS;
        } break;
        default: {
            UNREACHABLE();
        } break;
    }

    kfree((vaddr)block_buffer);
    return return_status;
}

// TODO this can be separated into delete_entry and delete_blocks or something like this
//      the delete_entry part can be re-used by fs_move
// NOTE: dir_should_be_empty should only be used by functions like fs_mv, where it is guaranteed
//       that if the inode is a dir, there won't be a memory leak from the inode being de-allocated and losing
//       all its' inode references in its' dir entries
u16 fs_delete(const char *path, bool dir_should_be_empty = true)
{
    // TODO this should remove path from lookup table

    int path_len;
    int parent_path_len;
    path_lengths(path, &path_len, &parent_path_len);

    const char *delname = path + parent_path_len;
    u64 delname_len = path_len - parent_path_len;
    ASSERT(*delname != '/');
    bool is_dir = (delname[delname_len-1] == '/');

    auto lookupresult = lookup_path(path, parent_path_len);
    if(!lookupresult.found_inode) {
        return FS_STATUS_FILE_NOT_FOUND;
    }
    u32 parent_inode_num = lookupresult.inode_num;
    INode *parent = get_inode(parent_inode_num);
// ----------------------------------------------------------

    u32 blockcount = inode_used_blocks(parent);
    u64 block_buffer_size = blockcount * g_block_size_bytes;
    u8 *block_buffer = (u8 *)kmalloc(blockcount * g_block_size_bytes, 4096);
    inode_read_data_blocks(parent, block_buffer, blockcount, 0);

    const u32 CASE_FOUND_ENTRY = 0;
    const u32 CASE_FOUND_DIR_NOT_EMPTY = 1;
    const u32 CASE_NO_ENTRY_FOUND = 2;

    u8 *base_ptr = block_buffer;
    u8 *ptr = base_ptr;
    u8 *prev_ptr = ptr;
    u8 *block_buffer_end = base_ptr + block_buffer_size;
    u32 search_result = CASE_NO_ENTRY_FOUND;
    u32 inode_num = 0;
    INode *inode = 0;
    u64 blocks_to_free = 0;
    while(ptr < block_buffer_end) {
        auto entry = (DirectoryEntry *)ptr;
        ASSERT(entry->name_length < EXT2_DIR_MAX_NAME_LENGTH);

        if(dir_entry_name_match(entry, delname, delname_len, is_dir)) {
            inode_num = entry->inode_num;
            inode = get_inode(inode_num);

            if(entry->file_type == EXT2_DIR_FTYPE_DIR && dir_should_be_empty && !is_dir_empty(inode)) {
                search_result = CASE_FOUND_DIR_NOT_EMPTY;
            } else {
                search_result = CASE_FOUND_ENTRY;
            }

            break;
        }

        prev_ptr = ptr;
        ptr += entry->length;
    }

// ----------------------------------------------------------
    u16 return_status;
    switch(search_result) {
        case(CASE_FOUND_ENTRY): {
        // TODO this probably breaks aliasing rules
            // NOTE: there are 2 cases to consider during the loop:
            //      1: this iteration is not deleting the last dir entry
            //          -> deleted entry gets overwritten by the next entry
            //      2: this iteration is deleting the last dir entry
            //          -> loop ends
            //          -> the previous entry gets expanded to span the rest of the block
            //          -> there may be empty blocks after this current block which need to be freed
            u8 *deleted_entry_ptr = ptr;
            auto deleted_entry = (DirectoryEntry *)deleted_entry_ptr;
            u8 *slot_ptr = prev_ptr;
            auto slot = (DirectoryEntry *)slot_ptr;
            u64 one_past_end_block = round_up_divide(deleted_entry_ptr - base_ptr, g_block_size_bytes); 
            u8 *next_block_end = base_ptr + one_past_end_block * g_block_size_bytes;
            u8 *moved_ptr = deleted_entry_ptr + deleted_entry->length;
            auto moved = (DirectoryEntry *)moved_ptr;
            while(moved_ptr < block_buffer_end) {
                u64 len = dir_entry_true_size(moved->name_length);
                u8 *next_slot = slot_ptr + slot->length;
                u64 next_slot_len = next_block_end - next_slot;

                if(len <= next_slot_len) {
                    slot_ptr = next_slot;
                    slot = (DirectoryEntry *)slot_ptr;
                } else {
                    slot->length += next_slot_len;
                    slot_ptr = next_block_end;
                    slot = (DirectoryEntry *)slot_ptr;
                    next_block_end += g_block_size_bytes;
                }

                u64 old_len = moved->length;
                moved->length = len;
                memmove_workaround(slot_ptr, moved_ptr, len);
                moved_ptr += old_len;
                moved = (DirectoryEntry *)moved_ptr;
            }
            slot->length = next_block_end - slot_ptr;
// NOTE: this is skipped in the special case where there is only 1 entry in the whole block,
//       in which case the whole block (starting at slot_ptr) should be deleted
            if(slot_ptr != deleted_entry_ptr)
                slot_ptr += slot->length;
            u64 unused = block_buffer_end - slot_ptr;
            blocks_to_free = unused / g_block_size_bytes;
        // ----------------------------------------------------------

            u64 offset = deleted_entry_ptr - base_ptr;
            u32 startblock_index = offset / g_block_size_bytes;
            u64 writeback_count = blockcount - startblock_index - blocks_to_free;

            u8 *writeback_buffer = block_buffer + startblock_index * g_block_size_bytes;

            writeback_inode_data_blocks(parent, writeback_buffer, startblock_index, writeback_count);

            if(blocks_to_free > 0) {
                // TODO make sure this handles the case of all blocks freed properly
                u32 reserved = inode_reserved_blocks(parent);
                u32 used = inode_used_blocks(parent);
                u32 unused = reserved - used;
                __inode_discard_blocks_from_end(parent, blocks_to_free + unused);

                u64 size = inode_size(parent);
                size -= g_block_size_bytes * blocks_to_free;
                __inode_set_size(parent, size);

                writeback_inode(parent_inode_num);
            }

            inode->hardlink_count--;
            if(inode->hardlink_count == 0) {
                u32 reserved = inode_reserved_blocks(inode);
                __inode_discard_blocks_from_end(inode, reserved);
                __free_inode(inode_num);
            }
            writeback_inode(inode_num);
            return_status = FS_STATUS_OK;
        } break;

        case(CASE_FOUND_DIR_NOT_EMPTY): {
            return_status = FS_STATUS_DIR_NOT_EMPTY;
        } break;

        case(CASE_NO_ENTRY_FOUND): {
            return_status = FS_STATUS_FILE_NOT_FOUND;
        } break;

        default: {
            UNREACHABLE();
        } break;
    }
// ----------------------------------------------------------

    kfree((vaddr)block_buffer);
    return return_status;
}

u16 fs_write(const char *path, u8 *buffer, u64 offset, u64 size)
{
    auto res = lookup_path(path);
    if(!res.found_inode) {
        return FS_STATUS_FILE_NOT_FOUND;
    }
    INode *inode = get_inode(res.inode_num);

    u64 filesize = inode_size(inode);
    u64 start = min(filesize, offset);
    u64 end = offset + size;
    u64 zero_count = offset - start;

    u64 startblock = start / g_block_size_bytes;
    u64 one_past_end_block = round_up_divide(end, g_block_size_bytes);
    u64 blockcount = one_past_end_block - startblock;

    u64 reserved = inode_reserved_blocks(inode);
    if(one_past_end_block > reserved) {
        u64 must_alloc = one_past_end_block - reserved;
        if(must_alloc > g_superblock.free_block_count) {
            return FS_STATUS_OUT_OF_SPACE;
        }
        __inode_ensure_blocks(inode, must_alloc);
    }

    u8 *block_buffer = (u8 *)kmalloc(blockcount * g_block_size_bytes, 4096);
    if(startblock < inode_used_blocks(inode)) {
        u64 readend = min(end, filesize);
        u32 readend_block = round_up_divide(readend, g_block_size_bytes);
        u32 readblocks = readend_block - startblock;
        inode_read_data_blocks(inode, block_buffer, readblocks, startblock);
    }

    u64 in_block_start = start - startblock * g_block_size_bytes;
    u8 *ptr = block_buffer + in_block_start;
    memset_workaround(ptr, 0, zero_count);

// TODO do this without copy
    ptr += zero_count;
    memmove_workaround(ptr, buffer, size);

    writeback_inode_data_blocks(inode, block_buffer, startblock, blockcount);

    u64 new_size = max(filesize, end);
    __inode_set_size(inode, new_size);

    writeback_inode(res.inode_num);
    kfree((vaddr)block_buffer);
    return FS_STATUS_OK;
}

u16 fs_truncate(const char *path, u64 newsize)
{
    auto res = lookup_path(path);
    if(!res.found_inode) {
        return FS_STATUS_FILE_NOT_FOUND;
    }
    INode *inode = get_inode(res.inode_num);
    // TODO this can be checked without looking up the inode (this data is in the direntry)
    if((inode->mode & EXT2_INODE_TYPE_REG_FILE) == 0) {
        return FS_STATUS_WRONG_FILE_TYPE;
    }

    u64 filesize = inode_size(inode);
    if(filesize < newsize) // TODO should this fill the remaining space with zeroes?
        return FS_STATUS_BAD_ARG;
    
    u64 new_blockcount = round_up_divide(newsize, g_block_size_bytes);
    u64 reserved = inode_reserved_blocks(inode);
    if(reserved > new_blockcount) {
        u64 discard = reserved - new_blockcount;
        __inode_discard_blocks_from_end(inode, discard);
    }

    __inode_set_size(inode, newsize);
    writeback_inode(res.inode_num);
    return FS_STATUS_OK;
}

// TODO this makes fs_file_size() redundant
FileStatResult fs_stat(const char *path)
{
    auto res = lookup_path(path);
    if(!res.found_inode)
        return {};

    INode *inode = get_inode(res.inode_num);
    u64 size = inode_size(inode);

    bool is_dir = inode->mode & EXT2_INODE_TYPE_DIR;

    return {
        .found_file = true,
        .size = size,
        .is_dir = is_dir
    };
}

u16 fs_mv(const char *src_path, const char *dst_path)
{
    ASSERT(*src_path == '/');
    ASSERT(*dst_path == '/');
    auto res = lookup_path(src_path);
    if(!res.found_inode)
        return FS_STATUS_FILE_NOT_FOUND;

    int dst_len = strlen_workaround(dst_path);
    INode *inode = get_inode(res.inode_num);
    u16 type = (inode->mode & EXT2_INODE_TYPE_DIR) ? FS_TYPE_DIR : FS_TYPE_REG_FILE;
    if((type == FS_TYPE_REG_FILE) && (dst_path[dst_len-1] == '/'))
        return FS_STATUS_BAD_ARG;

    fs_create(dst_path, type, res.inode_num);
    int result = fs_delete(src_path, false); // since there are 2 hardlinks to the inode, this should not delete the underlying inode
    dbg_uint(result);
    ASSERT(result == FS_STATUS_OK);

    return FS_STATUS_OK;
}
