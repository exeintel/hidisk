#ifndef FLOPPY_H
#define FLOPPY_H

#include "lib.h"

#pragma pack(push, 1)
typedef struct {
    BYTE jmp[3];
    BYTE oem[8];
    WORD bytes_per_sector;
    BYTE sectors_per_cluster;
    WORD reserved_sectors;
    BYTE num_fats;
    WORD root_entries;
    WORD total_sectors_16;
    BYTE media_descriptor;
    WORD sectors_per_fat;
    WORD sectors_per_track;
    WORD num_heads;
    DWORD hidden_sectors;
    DWORD total_sectors_32;
    BYTE drive_number;
    BYTE reserved;
    BYTE boot_signature;
    DWORD volume_serial;
    BYTE volume_label[11];
    BYTE fs_type[8];
    BYTE boot_code[448];
    WORD signature;
} FAT12_BPB;
#pragma pack(pop)

typedef struct {
    BYTE name[8];
    BYTE ext[3];
    BYTE attributes;
    BYTE reserved;
    BYTE creation_time_tenths;
    WORD creation_time;
    WORD creation_date;
    WORD last_access_date;
    WORD first_cluster_hi;
    WORD last_mod_time;
    WORD last_mod_date;
    WORD first_cluster;
    DWORD file_size;
} FAT12_DIR_ENTRY;

#define ATTR_READ_ONLY  0x01
#define ATTR_HIDDEN     0x02
#define ATTR_SYSTEM     0x04
#define ATTR_VOLUME_ID  0x08
#define ATTR_DIRECTORY  0x10
#define ATTR_ARCHIVE    0x20
#define ATTR_LONG_NAME  0x0F

int hi_is_floppy(const char *drive);
int hi_floppy_format(const char *drive);
int hi_floppy_create_image(const char *drive, const char *img_file);
int hi_floppy_list(const char *drive);
int hi_floppy_info(const char *drive);

#endif
