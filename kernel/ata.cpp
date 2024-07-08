#pragma once
#include "kernel/types.h"
#include "kernel/asm.cpp"

// based on info and code from
//  https://wiki.osdev.org/PCI_IDE_Controller
//  https://github.com/SerenityOS/serenity/blob/master/Kernel/Devices/Storage/ATA/GenericIDE/Channel.cpp

const u8 COMMAND_IDENTIFY_PACKET = 0xa1;
const u8 COMMAND_IDENTIFY = 0xec;

const u8 COMMAND_PIO_READ_28BIT = 0x20;
const u8 COMMAND_PIO_WRITE_28BIT = 0x30;

const u8 COMMAND_PIO_READ_48BIT = 0x24;
const u8 COMMAND_PIO_WRITE_48BIT = 0x34;

struct IDEDevice
{
    bool is_present = false;

    static const u8 CHANNEL_PRIMARY = 0;
    static const u8 CHANNEL_SECONDARY = 1;
    u8 channel = 0;

    static const u8 CONNECTION_MASTER = 0;
    static const u8 CONNECTION_SLAVE = 1;
    u8 slave_bit = 0;

    static const u8 LBA_ON = 1;
    static const u8 LBA_OFF = 0;
    u8 lba_on = 1;

    enum Types
    {
        ATA,
        ATAPI
    };
    u8 type = Types::ATA;

    static const u16 PRIMARY_REG_BASE = 0x1f0;
    static const u16 PRIMARY_CONTROL_REG = 0x3f6;
    static const u16 SECONDARY_REG_BASE = 0x170;
    static const u16 SECONDARY_CONTROL_REG = 0x376;
    struct RegisterAddresses
    {
        u16 data;
        u16 error; // READ: error WRITE: feature
        u16 sector_count;
        u16 lba0; // bits [0, 7]: LBA0, bits [8, 15]: LBA3
        u16 lba1; // bits [0, 7]: LBA1, bits [8, 15]: LBA4
        u16 lba2; // bits [0, 7]: LBA2, bits [8, 15]: LBA5
        u16 drive_select;
        u16 command; // WRITE: command, READ: status & acknowledge interrupt
        u16 control; // WRITE: control, READ: altstatus (read status reg without acknowledging interrupt)
    };
    RegisterAddresses regs;

    u64 sector_count = 0;
    u64 size_bytes = 0;

    struct Commands
    {
        u8 identify_packet = COMMAND_IDENTIFY;
        u8 pio_read = COMMAND_PIO_READ_28BIT;
        u8 pio_write = COMMAND_PIO_WRITE_28BIT;
    };
    Commands commands;
    bool lba_48bit = false;

    static const int MODEL_STRING_LENGTH = 40;
    // https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/ata/ns-ata-_identify_device_data
    struct __attribute__((__packed__)) IdentityPacket
    {
        u16 general_config;         // 0
        u16 num_cylinders;          // 2
        u16 specific_config;        // 4
        u16 num_heads;              // 6
        u16 _reserved[2];           // 8
        u16 sectors_per_track;      // 12
        u16 vendor_specific1[3];    // 14
        u8 serial_number[20];       // 20
        u16 _reserved2[2];          // 40
        u16 _reserved3;             // 44
        u8 firmware_revision[8];    // 46
        u8 model_string[MODEL_STRING_LENGTH];   // 54
        u8 max_block_transfer;      // 94
        u8 vendor_specific2;        // 95
        u16 trusted_computing;      // 96
        struct
        {
            u8 current_long_phys_sector_align : 2;
            u8 _reserved4 : 6;
            u8 dma_supported : 1;
            u8 lba_supported : 1;
            u8 _ignored1 : 6;
            u16 _ignored2;
        } capabilities;                     // 98
        u16 _reserved5[2];                  // 102
        u16 _ignored3;                      // 106
        u16 num_current_cylinders;          // 108
        u16 num_current_heads;              // 110
        u16 num_current_sectors_per_track;  // 112
        u32 current_sector_capacity;        // 114
        u8 _ignored4[2];                    // 118
        u32 user_addressable_sectors;       // 120
        u16 _ignored5[7];                   // 124
        u16 additional_supported;           // 138
        u16 _reserved6[5];                  // 140
        u16 _ignored6;                      // 150
        u16 serial_ata_capabilities[2];     // 152
        u16 serial_ata_features_supported;  // 156
        u16 serial_ata_features_enabled;    // 158
        u16 _ignored7[2];                   // 160
        u16 command_set_support[3];         // 164
        u64 command_set_active1[3];         // 170
        u16 _ignored8;                      // 176
        u16 normal_security_erase_unit;     // 178
        u16 enchanced_security_erase_unit;  // 180
        u16 _ignored9[7];                   // 182
        u32 _ignored10;                     // 196
        u32 max_48bit_lba[2];               // 200
        u16 _ignored11[2];                  // 208
        u16 physical_logical_sector_sizes;  // 214
        u16 _ignored12[12];                 // 216
        u16 command_set_support_text;       // 230
        u16 command_set_active;
        u16 _ignored13[7];
        u16 security_status;
        u16 _ignored14[31];
        u16 cfa_power_model;
        u16 _ignored15[8];
        u16 data_set_management_features;
        u16 _ignored16[36];
        u16 sct_command_transport;
        u16 _ignored17[2];
        u16 block_alignment;
        u16 _ignored18[4];
        u16 nv_cache_capabilities;
        u16 _ignored19[4];
        u16 nv_cache_options[2];
        u16 _ignored20[2];
        u16 transport_major_version;
        u16 _ignored21[7];
        u32 _ignored22[2];
        u16 _ignored23[22];
    };
    IdentityPacket identity_packet;

    union Status
    {
        struct
        {
            u8 error : 1;
            u8 index : 1;
            u8 corrected_data : 1;      // CORR
            u8 data_request_ready : 1;  // DRQ
            u8 seek_complete : 1;       // DSC
            u8 write_fault : 1;         // DF
            u8 ready : 1;               // DRDY
            u8 busy : 1;
        } bitfield;
        u8 raw;
        Status(u8 val) : raw(val) {}
    };

    bool interrupts_enabled = true;
    void disable_interrupts();
    void enable_interrupts();
    void select_drive();
    void identify_command();
    Status read_status();
    bool is_atapi();
    void populate_identity_packet();
    void wait_400ns();
    void poll_busy(bool);
    void read_sectors_pio(u8*, u16, u64);
    void write_sectors_pio(u8*, u16, u64);
    void setup_readwrite(u16, u64);
};

static_assert(sizeof(IDEDevice::identity_packet) % 2 == 0); // must be aligned to 4 for insd instruction to work

const int NUM_DRIVES = 4;
IDEDevice ide_drives[NUM_DRIVES];

void IDEDevice::disable_interrupts()
{
    // write 2 to control register for channel;
    // NOTE: in the osdev article, this is set to 0x80 | 2, not sure what the 0x80 is for...
    out8(regs.control, 2);
    interrupts_enabled = false;
}

void IDEDevice::enable_interrupts()
{
    // write 2 to control register for channel;
    out8(regs.control, 0);
    interrupts_enabled = true;
}

void IDEDevice::select_drive()
{
    out8(regs.drive_select, 0xa | (lba_on << 6) | (slave_bit << 4));
}

void IDEDevice::identify_command()
{
    out8(regs.command, COMMAND_IDENTIFY);
}

IDEDevice::Status IDEDevice::read_status()
{
    Status status = in8(regs.command);
    return status;
}

// NOTE this comes directory from osdev PCI IDE Controller article
bool IDEDevice::is_atapi()
{
    auto status = read_status();
    if(!status.bitfield.error)
        return false;

    u8 cl = in8(regs.lba1);
    u8 ch = in8(regs.lba2);

    return (cl == 0x14 && ch == 0xeb) || (cl == 0x69 && ch == 0x96);
}

void IDEDevice::populate_identity_packet()
{
    ASSERT(sizeof(identity_packet) % 2 == 0);
    int word_count = sizeof(identity_packet) / 2;
    out8(regs.command, commands.identify_packet);
    wait_400ns();
    void *ptr = (void *)&identity_packet;
    ins16(regs.data, word_count, ptr);

    for(int i = 0; i < MODEL_STRING_LENGTH; i += 2) {
        u8 val1 = identity_packet.model_string[i];
        u8 val2 = identity_packet.model_string[i + 1];

        identity_packet.model_string[i] = val2;
        identity_packet.model_string[i + 1] = val1;
    }
}

void IDEDevice::wait_400ns()
{
    in8(regs.control);
    in8(regs.control);
    in8(regs.control);
    in8(regs.control);
}

void IDEDevice::poll_busy(bool wait_drq)
{
    wait_400ns();
    Status status = read_status();
    while(status.bitfield.busy) {
        status = read_status();
    }

    while(wait_drq && !status.bitfield.data_request_ready) {
        vga_print("wait DRQ\n");
        dbg_str("wait DRQ\n");
        status = read_status();
    }

    ASSERT(!status.bitfield.error);
    ASSERT(!status.bitfield.write_fault);
    //ASSERT(status.bitfield.data_request_ready); // drive should be ready for request by this point, according to osdev
}

void IDEDevice::setup_readwrite(u16 sector_count, u64 lba)
{
    select_drive();
    poll_busy(false);

    if(lba_48bit)
        ASSERT(lba + sector_count <= 0xffffffffffff);
    else
        ASSERT(lba + sector_count <= 0xfffffff);

    u8 sector_count_low = sector_count & 0xff;
    u8 sector_count_high = sector_count >> 8;

    u8 lba0 = lba & 0xff;
    u8 lba1 = (lba >> 8) & 0xff;
    u8 lba2 = (lba >> 16) & 0xff;
    u8 lba3 = (lba >> 24) & 0xff;
    u8 lba4 = (lba >> 32) & 0xff;
    u8 lba5 = (lba >> 40) & 0xff;
    // TODO in non lba_48bit, move 4 bits to hddselect register

    if(lba_48bit) {
        out8(regs.sector_count, sector_count_high);
        out8(regs.lba0, lba3);
        out8(regs.lba1, lba4);
        out8(regs.lba2, lba5);
    }
    out8(regs.sector_count, sector_count_low);
    out8(regs.lba0, lba0);
    out8(regs.lba1, lba1);
    out8(regs.lba2, lba2);
}

void IDEDevice::read_sectors_pio(u8 *buffer, u16 sector_count, u64 lba)
{
    setup_readwrite(sector_count, lba);
    out8(regs.command, commands.pio_read);
    // NOTE after experimentation, doing one ins16 for all sectors doesn't seem to work, must do one ins16 per sectors
    const u32 bytes_per_sector = 512;
    const u32 words_per_second = bytes_per_sector / 2;
    u8 *ptr = buffer;
    for(int i = 0; i < sector_count; ++i) {
        poll_busy(true);
        ins16(regs.data, words_per_second, ptr);
        ptr += bytes_per_sector;
    }
    ASSERT((u64)(ptr - buffer) / 512 == sector_count);
}

void IDEDevice::write_sectors_pio(u8 *buffer, u16 sector_count, u64 lba)
{
    setup_readwrite(sector_count, lba);
    out8(regs.command, commands.pio_write);
    // NOTE after experimentation, doing one ins16 for all sectors doesn't seem to work, must do one ins16 per sectors
    const u32 bytes_per_sector = 512;
    const u32 words_per_sector = bytes_per_sector / 2;
    u8 *ptr = buffer;
    for(int i = 0; i < sector_count; ++i) {
        poll_busy(true);
        outs16(regs.data, words_per_sector, ptr);
        ptr += bytes_per_sector;
    }
    ASSERT((u64)(ptr - buffer) / 512 == sector_count);
}

// NOTE this assumes all the files are only on hdd00
void read_sectors_pio(u8 *buffer, u16 sector_count, u64 lba)
{
    ide_drives[0].read_sectors_pio(buffer, sector_count, lba);
}
// NOTE this assumes all the files are only on hdd00
void write_sectors_pio(u8 *buffer, u16 sector_count, u64 lba)
{
    ide_drives[0].write_sectors_pio(buffer, sector_count, lba);
}

void init_ide_devices()
{
    for(int i = 0; i < NUM_DRIVES; ++i) {
        ide_drives[i] = IDEDevice();
        u8 channel = i & (1 << 1);
        u8 slave_bit = i & 1;
        ide_drives[i].channel = channel;
        ide_drives[i].slave_bit = slave_bit;

        u16 base = (channel == 0) ? IDEDevice::PRIMARY_REG_BASE : IDEDevice::SECONDARY_REG_BASE;
        u16 control = (channel == 0) ? IDEDevice::PRIMARY_CONTROL_REG : IDEDevice::SECONDARY_CONTROL_REG;
        ide_drives[i].regs = {
            .data           = (u16)(base + 0),
            .error          = (u16)(base + 1),
            .sector_count   = (u16)(base + 2),
            .lba0           = (u16)(base + 3),
            .lba1           = (u16)(base + 4),
            .lba2           = (u16)(base + 5),
            .drive_select   = (u16)(base + 6),
            .command        = (u16)(base + 7),
            .control        = control
        };

        ide_drives[i].select_drive();
        ide_drives[i].wait_400ns();
        ide_drives[i].identify_command();
        ide_drives[i].wait_400ns();
        auto status = ide_drives[i].read_status();
        if(status.raw == 0) // device is not present
            continue;

        ide_drives[i].is_present = true;
        ide_drives[i].disable_interrupts();
        ide_drives[i].wait_400ns();
        bool is_atapi = ide_drives[i].is_atapi();
        ide_drives[i].wait_400ns();
        if(is_atapi) {
            ide_drives[i].type = IDEDevice::Types::ATAPI;
            ide_drives[i].commands.identify_packet = COMMAND_IDENTIFY_PACKET;
        } else {
            ide_drives[i].type = IDEDevice::Types::ATA;
            ide_drives[i].commands.identify_packet = COMMAND_IDENTIFY;
        }
        ide_drives[i].populate_identity_packet();
        if(ide_drives[i].identity_packet.command_set_support[1] & (1 << 10)) {
            ide_drives[i].sector_count = ide_drives[i].identity_packet.max_48bit_lba[0];
            ide_drives[i].lba_48bit = true;
            ide_drives[i].commands.pio_read = COMMAND_PIO_READ_48BIT;
            ide_drives[i].commands.pio_write = COMMAND_PIO_WRITE_48BIT;
        } else {
            ide_drives[i].sector_count = ide_drives[i].identity_packet.user_addressable_sectors;
            ide_drives[i].lba_48bit = false;
            ide_drives[i].commands.pio_read = COMMAND_PIO_READ_28BIT;
            ide_drives[i].commands.pio_write = COMMAND_PIO_WRITE_28BIT;
        }
        ide_drives[i].size_bytes = (ide_drives[i].sector_count * 512) / 1024 / 1024;

    }
}
