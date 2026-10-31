#ifndef LIB_H
#define LIB_H

#define _WIN32_WINNT 0x0600

#include <windows.h>
#include <winioctl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define HI_OK            0
#define HI_ERR_PARAM     1
#define HI_ERR_NOT_FOUND 2
#define HI_ERR_NO_ADMIN  3
#define HI_ERR_GENERAL   99

#define MAX_FS_TYPE 32
#define MAX_LABEL   256

#pragma pack(push, 1)
typedef struct {
    BYTE boot_code[446];
    struct {
        BYTE status;
        BYTE chs_first[3];
        BYTE type;
        BYTE chs_last[3];
        DWORD lba_first;
        DWORD sectors;
    } partition[4];
    WORD signature;
} MBR;
#pragma pack(pop)

typedef struct {
    ULONGLONG total_sectors;
    DWORD bytes_per_sector;
    DWORD sectors_per_cluster;
    ULONGLONG total_bytes;
    ULONGLONG free_bytes;
    char file_system[MAX_FS_TYPE];
    char volume_label[MAX_LABEL];
    DWORD volume_serial;
    UINT drive_type;
} DISK_INFO;

int hi_is_admin(void);
BOOL hi_drive_exists(const char *drive);

int hi_get_info(const char *drive);
int hi_format(const char *drive, const char *fs);
int hi_delete_partition(const char *drive);
int hi_create_partition(const char *drive, DWORD size_mb);
int hi_offline(const char *drive);
int hi_online(const char *drive);
int hi_defrag(const char *drive);
int hi_kill_mbr(const char *drive);
int hi_resize(const char *drive, DWORD size_mb);
int hi_check(const char *drive);
int hi_set_label(const char *drive, const char *label);
int hi_backup(const char *drive, const char *file);
int hi_restore(const char *drive, const char *file);
int hi_clean(const char *drive);
int hi_mount(const char *drive, const char *folder);
int hi_unmount(const char *drive);
int hi_export_csv(const char *drive, const char *file);
int hi_import_cfg(const char *drive, const char *file);
int hi_clone(const char *src_drive, const char *dst_drive);
int hi_sectors(const char *drive, DWORD lba, DWORD count);
int hi_help(void);

int hi_read_mbr(const char *drive, MBR *mbr);
int hi_write_mbr(const char *drive, MBR *mbr);
void print_size(ULONGLONG bytes);

#endif
