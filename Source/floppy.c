#include "floppy.h"

int hi_is_floppy(const char *drive) {
    char root[8];
    snprintf(root, sizeof(root), "%c:\\", drive[0]);
    UINT dt = GetDriveTypeA(root);
    return (dt == DRIVE_REMOVABLE &&
            (drive[0] == 'A' || drive[0] == 'a' ||
             drive[0] == 'B' || drive[0] == 'b')) ? 1 : 0;
}

static int floppy_read_sector(HANDLE hDrive, BYTE *buffer, DWORD sector) {
    LARGE_INTEGER offset;
    offset.QuadPart = (ULONGLONG)sector * 512;
    if (!SetFilePointerEx(hDrive, offset, NULL, FILE_BEGIN))
        return HI_ERR_GENERAL;
    DWORD bytes;
    if (!ReadFile(hDrive, buffer, 512, &bytes, NULL) || bytes != 512)
        return HI_ERR_GENERAL;
    return HI_OK;
}

static int floppy_write_sector(HANDLE hDrive, BYTE *buffer, DWORD sector) {
    LARGE_INTEGER offset;
    offset.QuadPart = (ULONGLONG)sector * 512;
    if (!SetFilePointerEx(hDrive, offset, NULL, FILE_BEGIN))
        return HI_ERR_GENERAL;
    DWORD bytes;
    if (!WriteFile(hDrive, buffer, 512, &bytes, NULL) || bytes != 512)
        return HI_ERR_GENERAL;
    return HI_OK;
}

int hi_floppy_format(const char *drive) {
    if (!hi_is_admin()) {
        printf("Administrator privileges required.\n");
        return HI_ERR_NO_ADMIN;
    }
    printf("Formatting floppy %s as FAT12...\n", drive);

    BYTE sector[512];
    memset(sector, 0, 512);

    sector[0] = 0xEB;
    sector[1] = 0x3C;
    sector[2] = 0x90;
    memcpy(sector + 3, "hiDisk  ", 8);

    FAT12_BPB *bpb = (FAT12_BPB *)sector;

    bpb->bytes_per_sector = 512;
    bpb->sectors_per_cluster = 1;
    bpb->reserved_sectors = 1;
    bpb->num_fats = 2;
    bpb->root_entries = 224;
    bpb->total_sectors_16 = 2880;
    bpb->media_descriptor = 0xF0;
    bpb->sectors_per_fat = 9;
    bpb->sectors_per_track = 18;
    bpb->num_heads = 2;
    bpb->hidden_sectors = 0;
    bpb->total_sectors_32 = 0;
    bpb->drive_number = 0;
    bpb->reserved = 0;
    bpb->boot_signature = 0x29;
    bpb->volume_serial = GetTickCount();
    memcpy(bpb->volume_label, "hiDISK_FLP ", 11);
    memcpy(bpb->fs_type, "FAT12   ", 8);
    sector[510] = 0x55;
    sector[511] = 0xAA;

    char path[16];
    snprintf(path, sizeof(path), "\\\\.\\%s", drive);
    HANDLE hDrive = CreateFileA(path, GENERIC_READ | GENERIC_WRITE,
                                FILE_SHARE_READ | FILE_SHARE_WRITE,
                                NULL, OPEN_EXISTING, 0, NULL);
    if (hDrive == INVALID_HANDLE_VALUE) {
        printf("Cannot open floppy drive %s\n", drive);
        return HI_ERR_NOT_FOUND;
    }

    if (floppy_write_sector(hDrive, sector, 0) != HI_OK) {
        printf("Failed to write boot sector.\n");
        CloseHandle(hDrive);
        return HI_ERR_GENERAL;
    }

    DWORD fat_size = 9 * 512;
    BYTE *fat = (BYTE *)malloc(fat_size);
    if (!fat) { CloseHandle(hDrive); return HI_ERR_GENERAL; }
    memset(fat, 0, fat_size);

    fat[0] = 0xF0;
    fat[1] = 0xFF;
    fat[2] = 0xFF;

    for (BYTE fat_num = 0; fat_num < 2; fat_num++) {
        for (DWORD s = 1 + (DWORD)fat_num * 9; s < 1 + (DWORD)((DWORD)fat_num + 1) * 9; s++) {
            floppy_write_sector(hDrive, fat + (s - 1 - fat_num * 9) * 512, s);
        }
    }

    BYTE root_sector[512];
    memset(root_sector, 0, 512);
    DWORD root_start = 1 + 2 * 9;
    for (DWORD s = 0; s < 14; s++) {
        floppy_write_sector(hDrive, root_sector, root_start + s);
    }

    BYTE data_sector[512];
    memset(data_sector, 0, 512);
    DWORD data_start = root_start + 14;
    for (DWORD s = 0; s < 2847; s++) {
        floppy_write_sector(hDrive, data_sector, data_start + s);
    }

    free(fat);
    CloseHandle(hDrive);
    printf("Floppy %s formatted as FAT12 (1.44MB).\n", drive);
    return HI_OK;
}

int hi_floppy_create_image(const char *drive, const char *img_file) {
    char path[16];
    snprintf(path, sizeof(path), "\\\\.\\%s", drive);
    HANDLE hDrive = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                                NULL, OPEN_EXISTING, 0, NULL);
    if (hDrive == INVALID_HANDLE_VALUE) {
        printf("Cannot open floppy drive %s\n", drive);
        return HI_ERR_NOT_FOUND;
    }

    HANDLE hFile = CreateFileA(img_file, GENERIC_WRITE, 0, NULL,
                               CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        printf("Cannot create %s\n", img_file);
        CloseHandle(hDrive);
        return HI_ERR_GENERAL;
    }

    BYTE buffer[512];
    DWORD total = 0;
    printf("Reading floppy %s to %s...\n", drive, img_file);

    for (DWORD s = 0; s < 2880; s++) {
        if (floppy_read_sector(hDrive, buffer, s) != HI_OK) {
            printf("Read error at sector %lu\n", s);
            break;
        }
        DWORD written;
        WriteFile(hFile, buffer, 512, &written, NULL);
        total++;
        if (total % 144 == 0) printf("\r%lu%%", total * 100 / 2880);
    }
    printf("\rImage created: %lu sectors (%lu KB)\n", total, total / 2);

    CloseHandle(hFile);
    CloseHandle(hDrive);
    return HI_OK;
}

int hi_floppy_list(const char *drive) {
    char path[16];
    snprintf(path, sizeof(path), "\\\\.\\%s", drive);
    HANDLE hDrive = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                                NULL, OPEN_EXISTING, 0, NULL);
    if (hDrive == INVALID_HANDLE_VALUE) {
        printf("Cannot open floppy drive %s\n", drive);
        return HI_ERR_NOT_FOUND;
    }

    BYTE sector[512];
    if (floppy_read_sector(hDrive, sector, 0) != HI_OK) {
        CloseHandle(hDrive);
        return HI_ERR_GENERAL;
    }

    FAT12_BPB *bpb = (FAT12_BPB *)sector;
    if (bpb->signature != 0xAA55) {
        printf("Not a valid FAT12 floppy (no boot signature).\n");
        CloseHandle(hDrive);
        return HI_ERR_GENERAL;
    }

    DWORD root_dir_sectors = (bpb->root_entries * 32 + bpb->bytes_per_sector - 1) / bpb->bytes_per_sector;
    DWORD root_dir_lba = bpb->reserved_sectors + bpb->num_fats * bpb->sectors_per_fat;

    printf("\nFloppy %s - Root Directory:\n", drive);
    printf("%-12s %-4s %8s %s\n", "Name", "Attr", "Size", "Date");
    printf("----------------------------------------\n");

    DWORD total_files = 0;
    for (DWORD s = 0; s < root_dir_sectors; s++) {
        BYTE dir_sector[512];
        if (floppy_read_sector(hDrive, dir_sector, root_dir_lba + s) != HI_OK)
            break;

        FAT12_DIR_ENTRY *entries = (FAT12_DIR_ENTRY *)dir_sector;
        for (int i = 0; i < 16; i++) {
            if (entries[i].name[0] == 0x00) break;
            if (entries[i].name[0] == 0xE5) continue;
            if (entries[i].attributes == ATTR_LONG_NAME) continue;
            if (entries[i].attributes & ATTR_VOLUME_ID) continue;

            char name[13];
            int len = 0;
            for (int j = 0; j < 8 && entries[i].name[j] != ' '; j++)
                name[len++] = entries[i].name[j];
            name[len] = '\0';

            char ext[4] = "";
            int elen = 0;
            for (int j = 0; j < 3 && entries[i].ext[j] != ' '; j++)
                ext[elen++] = entries[i].ext[j];
            ext[elen] = '\0';

            char attr_str[16] = "";
            if (entries[i].attributes & ATTR_DIRECTORY) strcat(attr_str, "DIR ");
            if (entries[i].attributes & ATTR_READ_ONLY) strcat(attr_str, "R ");
            if (entries[i].attributes & ATTR_HIDDEN) strcat(attr_str, "H ");
            if (entries[i].attributes & ATTR_SYSTEM) strcat(attr_str, "S ");
            if (entries[i].attributes & ATTR_ARCHIVE) strcat(attr_str, "A ");
            if (attr_str[0] == '\0') strcpy(attr_str, "-");

            WORD date = entries[i].last_mod_date;
            int day = date & 0x1F;
            int month = (date >> 5) & 0x0F;
            int year = 1980 + ((date >> 9) & 0x7F);

            if (elen > 0)
                printf("%-8s.%-3s %-4s %8lu  %04d-%02d-%02d\n",
                       name, ext, attr_str, entries[i].file_size,
                       year, month, day);
            else
                printf("%-12s %-4s %8lu  %04d-%02d-%02d\n",
                       name, attr_str, entries[i].file_size,
                       year, month, day);

            total_files++;
        }
    }

    printf("----------------------------------------\n");
    printf("Total: %lu file(s)\n", total_files);
    printf("\n");

    CloseHandle(hDrive);
    return HI_OK;
}

int hi_floppy_info(const char *drive) {
    char path[16];
    snprintf(path, sizeof(path), "\\\\.\\%s", drive);
    HANDLE hDrive = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                                NULL, OPEN_EXISTING, 0, NULL);
    if (hDrive == INVALID_HANDLE_VALUE) {
        printf("Cannot open floppy drive %s\n", drive);
        return HI_ERR_NOT_FOUND;
    }

    BYTE sector[512];
    if (floppy_read_sector(hDrive, sector, 0) != HI_OK) {
        CloseHandle(hDrive);
        return HI_ERR_GENERAL;
    }
    CloseHandle(hDrive);

    FAT12_BPB *bpb = (FAT12_BPB *)sector;

    printf("\n=== Floppy Disk Info: %s ===\n\n", drive);
    printf("OEM Label:       %.8s\n", bpb->oem);
    printf("Bytes/Sector:    %u\n", bpb->bytes_per_sector);
    printf("Sectors/Cluster: %u\n", bpb->sectors_per_cluster);
    printf("Reserved Sectors: %u\n", bpb->reserved_sectors);
    printf("Number of FATs:  %u\n", bpb->num_fats);
    printf("Root Entries:    %u\n", bpb->root_entries);
    printf("Total Sectors:   %lu\n", (unsigned long)(bpb->total_sectors_16 ? bpb->total_sectors_16 : bpb->total_sectors_32));
    printf("Media Desc:      0x%02X\n", bpb->media_descriptor);
    printf("Sectors/FAT:     %u\n", bpb->sectors_per_fat);
    printf("Sectors/Track:   %u\n", bpb->sectors_per_track);
    printf("Number of Heads: %u\n", bpb->num_heads);
    printf("Volume Label:    %.11s\n", bpb->volume_label);
    printf("Filesystem:      %.8s\n", bpb->fs_type);

    DWORD total_sectors = bpb->total_sectors_16 ? bpb->total_sectors_16 : bpb->total_sectors_32;
    DWORD total_bytes = total_sectors * bpb->bytes_per_sector;
    printf("Capacity:        ");
    print_size(total_bytes);
    printf(" (%lu KB)\n", (unsigned long)(total_bytes / 1024));

    DWORD root_dir_sectors = (bpb->root_entries * 32 + bpb->bytes_per_sector - 1) / bpb->bytes_per_sector;
    DWORD data_sectors = total_sectors - (bpb->reserved_sectors + bpb->num_fats * bpb->sectors_per_fat + root_dir_sectors);
    DWORD clusters = data_sectors / bpb->sectors_per_cluster;
    printf("Data Clusters:   %lu (", clusters);
    if (clusters < 4085) printf("FAT12");
    else if (clusters < 65525) printf("FAT16");
    else printf("FAT32");
    printf(")\n");

    printf("Boot Signature:  0x%04X %s\n", bpb->signature,
           bpb->signature == 0xAA55 ? "(valid)" : "(invalid)");

    printf("\n");
    return HI_OK;
}
