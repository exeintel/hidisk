#include "lib.h"

static int get_physical_index(const char *drive, DWORD *index) {
    char path[32];
    HANDLE hVol;
    VOLUME_DISK_EXTENTS extents;
    DWORD bytes;

    snprintf(path, sizeof(path), "\\\\.\\%s", drive);
    hVol = CreateFileA(path, 0, FILE_SHARE_READ | FILE_SHARE_WRITE,
                       NULL, OPEN_EXISTING, 0, NULL);
    if (hVol == INVALID_HANDLE_VALUE) return HI_ERR_NOT_FOUND;

    if (!DeviceIoControl(hVol, IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS,
                         NULL, 0, &extents, sizeof(extents), &bytes, NULL)) {
        CloseHandle(hVol);
        return HI_ERR_GENERAL;
    }
    *index = extents.Extents[0].DiskNumber;
    CloseHandle(hVol);
    return HI_OK;
}

static int run_diskpart_script(const char *script) {
    char temp_path[MAX_PATH];
    char temp_file[MAX_PATH];
    char cmd[MAX_PATH + 64];

    GetTempPathA(MAX_PATH, temp_path);
    GetTempFileNameA(temp_path, "HDI", 0, temp_file);

    FILE *f = fopen(temp_file, "w");
    if (!f) return HI_ERR_GENERAL;
    fprintf(f, "%s", script);
    fclose(f);

    snprintf(cmd, sizeof(cmd), "diskpart /s \"%s\" > nul 2>&1", temp_file);
    int r = system(cmd);
    DeleteFileA(temp_file);
    return (r == 0) ? HI_OK : HI_ERR_GENERAL;
}

static int save_volume_guid(const char *drive, const char *guid) {
    char path[MAX_PATH];
    GetEnvironmentVariableA("APPDATA", path, MAX_PATH);
    strncat(path, "\\hiDisk.cfg", MAX_PATH - strlen(path) - 1);
    FILE *f = fopen(path, "w");
    if (!f) return HI_ERR_GENERAL;
    fprintf(f, "%c:=%s\n", drive[0], guid);
    fclose(f);
    return HI_OK;
}

static int load_volume_guid(const char *drive, char *guid, DWORD size) {
    char path[MAX_PATH];
    char line[MAX_PATH];
    GetEnvironmentVariableA("APPDATA", path, MAX_PATH);
    strncat(path, "\\hiDisk.cfg", MAX_PATH - strlen(path) - 1);
    FILE *f = fopen(path, "r");
    if (!f) return HI_ERR_NOT_FOUND;
    while (fgets(line, sizeof(line), f)) {
        if (line[0] == drive[0] && line[1] == ':' && line[2] == '=') {
            line[strcspn(line, "\r\n")] = '\0';
            strncpy(guid, line + 3, size - 1);
            guid[size - 1] = '\0';
            fclose(f);
            return HI_OK;
        }
    }
    fclose(f);
    return HI_ERR_NOT_FOUND;
}

int hi_is_admin(void) {
    HANDLE hToken;
    TOKEN_ELEVATION elevation;
    DWORD size;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken))
        return 0;
    if (!GetTokenInformation(hToken, TokenElevation, &elevation, sizeof(elevation), &size)) {
        CloseHandle(hToken);
        return 0;
    }
    CloseHandle(hToken);
    return elevation.TokenIsElevated ? 1 : 0;
}

BOOL hi_drive_exists(const char *drive) {
    DWORD mask = GetLogicalDrives();
    int bit = toupper(drive[0]) - 'A';
    if (bit < 0 || bit > 25) return FALSE;
    return (mask & (1 << bit)) ? TRUE : FALSE;
}

void print_size(ULONGLONG bytes) {
    if (bytes >= 1073741824ULL)
        printf("%.2f GB", (double)bytes / 1073741824.0);
    else if (bytes >= 1048576ULL)
        printf("%.2f MB", (double)bytes / 1048576.0);
    else if (bytes >= 1024ULL)
        printf("%.2f KB", (double)bytes / 1024.0);
    else
        printf("%llu B", bytes);
}

int hi_get_info(const char *drive) {
    char root[8];
    char fs[MAX_FS_TYPE];
    char label[MAX_LABEL];
    DWORD serial, max_comp, flags;
    ULARGE_INTEGER free_avail, total, total_free;
    DWORD spc, bps, free_clusters, total_clusters;
    char vol_path[32];
    HANDLE hVol;
    DISK_GEOMETRY geom;
    DWORD bytes;
    UINT dt;

    snprintf(root, sizeof(root), "%c:\\", drive[0]);

    printf("=== Disk Information: %s ===\n\n", root);

    dt = GetDriveTypeA(root);
    printf("Drive Type: ");
    switch (dt) {
        case DRIVE_REMOVABLE: printf("Removable (USB/Floppy)\n"); break;
        case DRIVE_FIXED:     printf("Fixed (Hard Disk)\n"); break;
        case DRIVE_REMOTE:    printf("Network Drive\n"); break;
        case DRIVE_CDROM:     printf("CD-ROM/DVD\n"); break;
        case DRIVE_RAMDISK:   printf("RAM Disk\n"); break;
        case DRIVE_NO_ROOT_DIR: printf("Not Available\n"); break;
        default: printf("Unknown\n"); break;
    }

    if (!GetVolumeInformationA(root, label, MAX_LABEL, &serial, &max_comp, &flags, fs, MAX_FS_TYPE)) {
        strcpy(label, "(none)");
        strcpy(fs, "(unknown)");
    }
    printf("Volume Label:  %s\n", label);
    printf("Serial Number: %04X-%04X\n", (unsigned int)HIWORD(serial), (unsigned int)LOWORD(serial));
    printf("File System:   %s\n", fs);

    if (GetDiskFreeSpaceExA(root, &free_avail, &total, &total_free)) {
        printf("Total Size:    ");
        print_size(total.QuadPart);
        printf("\nFree Space:    ");
        print_size(free_avail.QuadPart);
        printf(" (%.1f%%)\n", total.QuadPart > 0 ? (double)free_avail.QuadPart / total.QuadPart * 100.0 : 0.0);
    }

    if (GetDiskFreeSpaceA(root, &spc, &bps, &free_clusters, &total_clusters)) {
        printf("Bytes/Sector:  %lu\n", bps);
        printf("Sectors/Cluster: %lu\n", spc);
        printf("Total Clusters: %lu\n", total_clusters);
    }

    snprintf(vol_path, sizeof(vol_path), "\\\\.\\%s", drive);
    hVol = CreateFileA(vol_path, 0, FILE_SHARE_READ | FILE_SHARE_WRITE,
                       NULL, OPEN_EXISTING, 0, NULL);
    if (hVol != INVALID_HANDLE_VALUE) {
        if (DeviceIoControl(hVol, IOCTL_DISK_GET_DRIVE_GEOMETRY,
                            NULL, 0, &geom, sizeof(geom), &bytes, NULL)) {
            printf("\nPhysical Geometry:\n");
            printf("  Cylinders:     %llu\n", (unsigned long long)geom.Cylinders.QuadPart);
            printf("  Tracks/Cylinder: %lu\n", geom.TracksPerCylinder);
            printf("  Sectors/Track: %lu\n", geom.SectorsPerTrack);
            printf("  Bytes/Sector:  %lu\n", geom.BytesPerSector);
            printf("  Media Type:    ");
            switch (geom.MediaType) {
                case FixedMedia:       printf("Fixed Hard Disk\n"); break;
                case RemovableMedia:   printf("Removable Media\n"); break;
                case F3_1Pt44_512:     printf("3.5\" 1.44MB Floppy\n"); break;
                case F3_2Pt88_512:     printf("3.5\" 2.88MB Floppy\n"); break;
                case F3_20Pt8_512:     printf("3.5\" 20.8MB Floppy\n"); break;
                case F3_720_512:       printf("3.5\" 720KB Floppy\n"); break;
                case F5_1Pt2_512:      printf("5.25\" 1.2MB Floppy\n"); break;
                case F5_360_512:       printf("5.25\" 360KB Floppy\n"); break;
                case F5_320_512:       printf("5.25\" 320KB Floppy\n"); break;
                case F5_180_512:       printf("5.25\" 180KB Floppy\n"); break;
                case F5_160_512:       printf("5.25\" 160KB Floppy\n"); break;
                default:               printf("Unknown\n"); break;
            }
        }
        CloseHandle(hVol);
    }

    printf("\nMount Points:\n");
    {
        char vols[4096];
        HANDLE hFind = FindFirstVolumeA(vols, 4096);
        if (hFind != INVALID_HANDLE_VALUE) {
            do {
                int vl = strlen(vols);
                if (vl > 0 && vols[vl-1] == '\\') vols[vl-1] = '\0';
                char paths[4096];
                DWORD plen;
                if (GetVolumePathNamesForVolumeNameA(vols, paths, 4096, &plen)) {
                    char *p = paths;
                    while (*p) {
                        if (strnicmp(p, root, 2) == 0)
                            printf("  %s -> %s\n", p, vols);
                        p += strlen(p) + 1;
                    }
                }
            } while (FindNextVolumeA(hFind, vols, 4096));
            FindVolumeClose(hFind);
        }
    }

    printf("\n");
    return HI_OK;
}

int hi_format(const char *drive, const char *fs) {
    if (!hi_is_admin()) {
        printf("Error: Administrator privileges required for formatting.\n");
        return HI_ERR_NO_ADMIN;
    }
    char cmd[MAX_PATH + 32];
    const char *fs_param = NULL;
    if (_stricmp(fs, "ntfs") == 0) fs_param = "NTFS";
    else if (_stricmp(fs, "fat32") == 0) fs_param = "FAT32";
    else if (_stricmp(fs, "fat") == 0 || _stricmp(fs, "fat12") == 0) fs_param = "FAT";
    else if (_stricmp(fs, "exfat") == 0) fs_param = "exFAT";
    else if (_stricmp(fs, "reFS") == 0 || _stricmp(fs, "refs") == 0) fs_param = "ReFS";
    else {
        printf("Error: Unsupported filesystem '%s'. Supported: ntfs, fat32, fat, fat12, exfat, refs\n", fs);
        return HI_ERR_PARAM;
    }

    printf("Formatting %s as %s...\n", drive, fs_param);
    snprintf(cmd, sizeof(cmd), "cmd /c format %s /FS:%s /Q /Y > nul 2>&1", drive, fs_param);
    int r = system(cmd);
    if (r == 0) printf("Format complete.\n");
    else printf("Format failed (error code %d).\n", r);
    return (r == 0) ? HI_OK : HI_ERR_GENERAL;
}

int hi_delete_partition(const char *drive) {
    if (!hi_is_admin()) { printf("Admin rights required.\n"); return HI_ERR_NO_ADMIN; }
    char script[512];
    char vol[4];
    strncpy(vol, drive, 2);
    vol[2] = '\0';
    snprintf(script, sizeof(script),
             "select volume %s\ndelete partition\n", vol);
    printf("Deleting partition %s...\n", vol);
    int r = run_diskpart_script(script);
    if (r == HI_OK) printf("Partition deleted successfully.\n");
    else printf("Failed to delete partition. Try using Administrator shell.\n");
    return r;
}

int hi_create_partition(const char *drive, DWORD size_mb) {
    if (!hi_is_admin()) { printf("Admin rights required.\n"); return HI_ERR_NO_ADMIN; }
    DWORD phys;
    int r = get_physical_index(drive, &phys);
    if (r != HI_OK) { printf("Drive not found.\n"); return r; }

    char script[512];
    snprintf(script, sizeof(script),
             "select disk %lu\ncreate partition primary size=%lu\n"
             "assign\nformat fs=ntfs quick\n",
             phys, size_mb);
    printf("Creating %lu MB partition on disk %lu...\n", size_mb, phys);
    r = run_diskpart_script(script);
    if (r == HI_OK) printf("Partition created and formatted.\n");
    else printf("Failed to create partition.\n");
    return r;
}

int hi_offline(const char *drive) {
    char root[8], vol_path[32], vol_name[MAX_PATH];
    HANDLE hVol;

    snprintf(root, sizeof(root), "%c:\\", drive[0]);
    snprintf(vol_path, sizeof(vol_path), "\\\\.\\%s", drive);
    hVol = CreateFileA(vol_path, 0, FILE_SHARE_READ | FILE_SHARE_WRITE,
                       NULL, OPEN_EXISTING, 0, NULL);
    if (hVol != INVALID_HANDLE_VALUE) {
        if (GetVolumeNameForVolumeMountPointA(root, vol_name, MAX_PATH))
            save_volume_guid(drive, vol_name);
        CloseHandle(hVol);
    }

    char dr[4];
    snprintf(dr, sizeof(dr), "%c:", drive[0]);
    if (!DefineDosDevice(DDD_REMOVE_DEFINITION, dr, NULL)) {
        printf("Failed to hide drive %s (error %lu)\n", dr, GetLastError());
        return HI_ERR_GENERAL;
    }
    printf("Drive %s is now hidden from Explorer.\n", dr);
    return HI_OK;
}

int hi_online(const char *drive) {
    char root[8], dr[4], target[MAX_PATH * 2], guid[MAX_PATH];
    snprintf(root, sizeof(root), "%c:\\", drive[0]);
    snprintf(dr, sizeof(dr), "%c:", drive[0]);

    if (hi_drive_exists(drive)) {
        printf("Drive %s already exists.\n", dr);
        return HI_OK;
    }

    if (load_volume_guid(drive, guid, MAX_PATH) == HI_OK) {
        snprintf(target, sizeof(target), "\\\\?\\%s", guid);
        int l = strlen(target);
        if (l > 0 && target[l-1] != '\\') strcat(target, "\\");
        if (SetVolumeMountPointA(root, target)) {
            printf("Drive %s restored.\n", dr);
            return HI_OK;
        }
    }

    {
        char vols[4096];
        HANDLE hFind = FindFirstVolumeA(vols, 4096);
        if (hFind != INVALID_HANDLE_VALUE) {
            do {
                int vl = strlen(vols);
                if (vl > 0 && vols[vl-1] == '\\') vols[vl-1] = '\0';
                char paths[4096];
                DWORD plen;
                if (GetVolumePathNamesForVolumeNameA(vols, paths, 4096, &plen)) {
                    if (paths[0] == '\0') {
                        snprintf(target, sizeof(target), "\\\\?\\%s\\", vols);
                        if (SetVolumeMountPointA(root, target)) {
                            printf("Drive %s restored (assigned to %s).\n", dr, vols);
                            FindVolumeClose(hFind);
                            return HI_OK;
                        }
                    }
                }
            } while (FindNextVolumeA(hFind, vols, 4096));
            FindVolumeClose(hFind);
        }
    }

    printf("No unmounted volume found for %s. Use Disk Management.\n", dr);
    return HI_ERR_GENERAL;
}

int hi_defrag(const char *drive) {
    if (!hi_is_admin()) { printf("Admin rights required.\n"); return HI_ERR_NO_ADMIN; }
    char cmd[MAX_PATH + 16];
    char vol[4];
    strncpy(vol, drive, 2);
    vol[2] = '\0';
    printf("Defragmenting %s...\n", vol);
    snprintf(cmd, sizeof(cmd), "defrag %s /U /V > nul 2>&1", vol);
    int r = system(cmd);
    if (r == 0) printf("Defragmentation complete.\n");
    else printf("Defrag failed (error %d). Use Admin shell if needed.\n", r);
    return (r == 0) ? HI_OK : HI_ERR_GENERAL;
}

int hi_kill_mbr(const char *drive) {
    if (!hi_is_admin()) { printf("Admin rights required.\n"); return HI_ERR_NO_ADMIN; }
    DWORD phys;
    int r = get_physical_index(drive, &phys);
    if (r != HI_OK) { printf("Drive not found.\n"); return r; }

    printf("WARNING: This will destroy the MBR (boot sector) on disk %lu!\n", phys);
    printf("Type 'YES' to confirm: ");
    char confirm[16];
    if (!fgets(confirm, sizeof(confirm), stdin)) return HI_ERR_GENERAL;
    confirm[strcspn(confirm, "\r\n")] = '\0';
    if (strcmp(confirm, "YES") != 0) {
        printf("Cancelled.\n");
        return HI_OK;
    }

    char path[32];
    snprintf(path, sizeof(path), "\\\\.\\PhysicalDrive%lu", phys);
    HANDLE hDisk = CreateFileA(path, GENERIC_READ | GENERIC_WRITE,
                               FILE_SHARE_READ | FILE_SHARE_WRITE,
                               NULL, OPEN_EXISTING, 0, NULL);
    if (hDisk == INVALID_HANDLE_VALUE) {
        printf("Cannot open physical disk %lu (error %lu)\n", phys, GetLastError());
        return HI_ERR_GENERAL;
    }

    BYTE zero[512];
    memset(zero, 0, 512);
    DWORD written;
    if (!WriteFile(hDisk, zero, 512, &written, NULL) || written != 512) {
        printf("Failed to write MBR.\n");
        CloseHandle(hDisk);
        return HI_ERR_GENERAL;
    }
    CloseHandle(hDisk);
    printf("MBR on disk %lu has been erased (sector 0 zeroed).\n", phys);
    return HI_OK;
}

int hi_resize(const char *drive, DWORD size_mb) {
    if (!hi_is_admin()) { printf("Admin rights required.\n"); return HI_ERR_NO_ADMIN; }
    char vol[4];
    strncpy(vol, drive, 2);
    vol[2] = '\0';

    char script[512];
    int r;

    printf("Resizing volume %s...\n", vol);

    snprintf(script, sizeof(script),
             "select volume %s\nshrink desired=%lu noerr\n", vol, size_mb);
    r = run_diskpart_script(script);
    if (r == HI_OK) { printf("Volume shrunk by %lu MB.\n", size_mb); return HI_OK; }

    snprintf(script, sizeof(script),
             "select volume %s\nextend size=%lu noerr\n", vol, size_mb);
    r = run_diskpart_script(script);
    if (r == HI_OK) { printf("Volume extended by %lu MB.\n", size_mb); return HI_OK; }

    printf("Resize failed. The volume may not support resize.\n");
    return HI_ERR_GENERAL;
}

int hi_check(const char *drive) {
    char cmd[MAX_PATH + 32];
    snprintf(cmd, sizeof(cmd), "chkdsk %s", drive);
    printf("Checking %s...\n", drive);
    int r = system(cmd);
    return (r == 0) ? HI_OK : HI_ERR_GENERAL;
}

int hi_set_label(const char *drive, const char *label) {
    char root[8];
    snprintf(root, sizeof(root), "%c:\\", drive[0]);
    if (!SetVolumeLabelA(root, label)) {
        printf("Failed to set label (error %lu)\n", GetLastError());
        return HI_ERR_GENERAL;
    }
    printf("Volume label set to '%s'.\n", label);
    return HI_OK;
}

int hi_backup(const char *drive, const char *file) {
    char path[32];
    HANDLE hVol, hFile;
    DWORD bytes_read, bytes_written;
    BYTE buffer[65536];
    ULARGE_INTEGER total_size;
    ULONGLONG total_read = 0;

    snprintf(path, sizeof(path), "\\\\.\\%s", drive);
    hVol = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                       NULL, OPEN_EXISTING, 0, NULL);
    if (hVol == INVALID_HANDLE_VALUE) {
        printf("Cannot open %s (error %lu)\n", path, GetLastError());
        return HI_ERR_NOT_FOUND;
    }

    hFile = CreateFileA(file, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                        FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        printf("Cannot create %s\n", file);
        CloseHandle(hVol);
        return HI_ERR_GENERAL;
    }

    GetDiskFreeSpaceExA(path, NULL, &total_size, NULL);
    printf("Backing up %s to %s...\n", drive, file);
    printf("Total size: ");
    print_size(total_size.QuadPart);
    printf("\n");

    while (ReadFile(hVol, buffer, sizeof(buffer), &bytes_read, NULL) && bytes_read > 0) {
        WriteFile(hFile, buffer, bytes_read, &bytes_written, NULL);
        total_read += bytes_read;
        printf("\r%llu MB / %llu MB", total_read / 1048576, total_size.QuadPart / 1048576);
    }
    printf("\nBackup complete (%llu bytes).\n", total_read);

    CloseHandle(hVol);
    CloseHandle(hFile);
    return HI_OK;
}

int hi_restore(const char *drive, const char *file) {
    if (!hi_is_admin()) { printf("Admin rights required.\n"); return HI_ERR_NO_ADMIN; }
    char path[32];
    HANDLE hVol, hFile;
    DWORD bytes_read, bytes_written;
    BYTE buffer[65536];
    ULONGLONG total_written = 0;

    snprintf(path, sizeof(path), "\\\\.\\%s", drive);
    hVol = CreateFileA(path, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
                       NULL, OPEN_EXISTING, 0, NULL);
    if (hVol == INVALID_HANDLE_VALUE) {
        printf("Cannot open %s (error %lu)\n", path, GetLastError());
        return HI_ERR_NOT_FOUND;
    }

    hFile = CreateFileA(file, GENERIC_READ, FILE_SHARE_READ, NULL,
                        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        printf("Cannot open %s\n", file);
        CloseHandle(hVol);
        return HI_ERR_GENERAL;
    }

    printf("Restoring %s from %s...\n", drive, file);
    while (ReadFile(hFile, buffer, sizeof(buffer), &bytes_read, NULL) && bytes_read > 0) {
        WriteFile(hVol, buffer, bytes_read, &bytes_written, NULL);
        total_written += bytes_read;
        printf("\r%llu MB restored", total_written / 1048576);
    }
    printf("\nRestore complete (%llu bytes).\n", total_written);

    CloseHandle(hVol);
    CloseHandle(hFile);
    return HI_OK;
}

int hi_clean(const char *drive) {
    if (!hi_is_admin()) { printf("Admin rights required.\n"); return HI_ERR_NO_ADMIN; }
    DWORD phys;
    int r = get_physical_index(drive, &phys);
    if (r != HI_OK) { printf("Drive not found.\n"); return r; }

    printf("WARNING: This will COMPLETELY ERASE disk %lu by zero-filling every sector!\n", phys);
    printf("Type 'YES' to confirm: ");
    char confirm[16];
    if (!fgets(confirm, sizeof(confirm), stdin)) return HI_ERR_GENERAL;
    confirm[strcspn(confirm, "\r\n")] = '\0';
    if (strcmp(confirm, "YES") != 0) { printf("Cancelled.\n"); return HI_OK; }

    char path[32];
    snprintf(path, sizeof(path), "\\\\.\\PhysicalDrive%lu", phys);
    HANDLE hDisk = CreateFileA(path, GENERIC_READ | GENERIC_WRITE,
                               FILE_SHARE_READ | FILE_SHARE_WRITE,
                               NULL, OPEN_EXISTING, 0, NULL);
    if (hDisk == INVALID_HANDLE_VALUE) {
        printf("Cannot open physical disk %lu\n", phys);
        return HI_ERR_GENERAL;
    }

    DISK_GEOMETRY geom;
    DWORD bytes;
    if (!DeviceIoControl(hDisk, IOCTL_DISK_GET_DRIVE_GEOMETRY, NULL, 0,
                         &geom, sizeof(geom), &bytes, NULL)) {
        CloseHandle(hDisk);
        return HI_ERR_GENERAL;
    }

    ULONGLONG total_sectors = geom.Cylinders.QuadPart * geom.TracksPerCylinder * geom.SectorsPerTrack;
    ULONGLONG total_bytes = total_sectors * geom.BytesPerSector;
    printf("Disk size: ");
    print_size(total_bytes);
    printf(", %llu sectors\n", total_sectors);

    BYTE *zero = (BYTE*)malloc(geom.BytesPerSector * 1024);
    if (!zero) { CloseHandle(hDisk); return HI_ERR_GENERAL; }
    memset(zero, 0, geom.BytesPerSector * 1024);

    DWORD sectors_per_write = 1024;
    ULONGLONG written = 0;
    DWORD write_size = geom.BytesPerSector * sectors_per_write;
    DWORD chunk_written;

    LARGE_INTEGER pos;
    pos.QuadPart = 0;
    SetFilePointerEx(hDisk, pos, NULL, FILE_BEGIN);

    ULONGLONG chunks = total_sectors / sectors_per_write;
    for (ULONGLONG i = 0; i < chunks; i++) {
        if (!WriteFile(hDisk, zero, write_size, &chunk_written, NULL)) break;
        written += chunk_written / geom.BytesPerSector;
        printf("\rCleaning... %llu%%", written * 100 / total_sectors);
    }
    ULONGLONG remaining = total_sectors % sectors_per_write;
    if (remaining > 0) {
        WriteFile(hDisk, zero, (DWORD)(remaining * geom.BytesPerSector), &chunk_written, NULL);
        written += remaining;
    }

    printf("\rClean complete. %llu sectors zeroed.\n", written);
    free(zero);
    CloseHandle(hDisk);
    return HI_OK;
}

int hi_mount(const char *drive, const char *folder) {
    char root[8];
    snprintf(root, sizeof(root), "%c:\\", drive[0]);

    char target[MAX_PATH];
    snprintf(target, sizeof(target), "\\\\?\\%s\\", folder);
    int l = strlen(target);
    if (l > 0 && target[l-1] != '\\') strcat(target, "\\");

    if (!SetVolumeMountPointA(target, root)) {
        printf("Failed to mount %s to %s (error %lu)\n", drive, folder, GetLastError());
        return HI_ERR_GENERAL;
    }
    printf("Drive %s mounted to %s\n", drive, folder);
    return HI_OK;
}

int hi_unmount(const char *drive) {
    char root[8];
    snprintf(root, sizeof(root), "%c:\\", drive[0]);

    if (!DeleteVolumeMountPointA(root)) {
        printf("Failed to unmount %s (error %lu)\n", root, GetLastError());
        return HI_ERR_GENERAL;
    }
    printf("Volume %s unmounted.\n", root);
    return HI_OK;
}

int hi_export_csv(const char *drive, const char *file) {
    DWORD phys;
    int r = get_physical_index(drive, &phys);
    if (r != HI_OK) { printf("Drive not found.\n"); return r; }

    MBR mbr;
    char path[32];
    snprintf(path, sizeof(path), "\\\\.\\PhysicalDrive%lu", phys);
    HANDLE hDisk = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                               NULL, OPEN_EXISTING, 0, NULL);
    if (hDisk == INVALID_HANDLE_VALUE) {
        printf("Cannot open physical disk.\n");
        return HI_ERR_GENERAL;
    }

    DWORD bytes;
    if (!ReadFile(hDisk, &mbr, sizeof(mbr), &bytes, NULL) || bytes != sizeof(mbr)) {
        printf("Failed to read MBR.\n");
        CloseHandle(hDisk);
        return HI_ERR_GENERAL;
    }
    CloseHandle(hDisk);

    FILE *f = fopen(file, "w");
    if (!f) { printf("Cannot create %s\n", file); return HI_ERR_GENERAL; }

    fprintf(f, "Partition,Status,Type,LBA_Start,Sectors,SizeMB\n");
    for (int i = 0; i < 4; i++) {
        if (mbr.partition[i].type == 0) continue;
        ULONGLONG size_bytes = (ULONGLONG)mbr.partition[i].sectors * 512ULL;
        fprintf(f, "%d,0x%02X,0x%02X,%lu,%lu,%.0f\n",
                i + 1,
                mbr.partition[i].status,
                mbr.partition[i].type,
                mbr.partition[i].lba_first,
                mbr.partition[i].sectors,
                (double)size_bytes / 1048576.0);
    }
    fclose(f);
    printf("Partition table exported to %s\n", file);
    return HI_OK;
}

int hi_import_cfg(const char *drive, const char *file) {
    if (!hi_is_admin()) { printf("Admin rights required.\n"); return HI_ERR_NO_ADMIN; }
    DWORD phys;
    int r = get_physical_index(drive, &phys);
    if (r != HI_OK) { printf("Drive not found.\n"); return r; }

    FILE *f = fopen(file, "r");
    if (!f) { printf("Cannot open %s\n", file); return HI_ERR_GENERAL; }

    printf("WARNING: This will recreate the partition table on disk %lu!\n", phys);
    printf("Type 'YES' to confirm: ");
    char confirm[16];
    if (!fgets(confirm, sizeof(confirm), stdin)) { fclose(f); return HI_ERR_GENERAL; }
    confirm[strcspn(confirm, "\r\n")] = '\0';
    if (strcmp(confirm, "YES") != 0) { fclose(f); printf("Cancelled.\n"); return HI_OK; }

    char line[256];
    char script[4096] = {0};
    char temp[256];
    int part_no = 0;

    snprintf(script, sizeof(script), "select disk %lu\nclean\n", phys);

    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\r\n")] = '\0';
        if (line[0] == '#' || line[0] == '\0') continue;

        char type_str[16] = {0};
        DWORD size_mb = 0;
        if (sscanf(line, " %15[^,], %lu", type_str, &size_mb) >= 1) {
            if (size_mb == 0) continue;
            part_no++;
            snprintf(temp, sizeof(temp),
                     "create partition primary size=%lu\n", size_mb);
            strncat(script, temp, sizeof(script) - strlen(script) - 1);

            if (_stricmp(type_str, "ntfs") == 0) {
                strncat(script, "format fs=ntfs quick\n", sizeof(script) - strlen(script) - 1);
            } else if (_stricmp(type_str, "fat32") == 0) {
                strncat(script, "format fs=fat32 quick\n", sizeof(script) - strlen(script) - 1);
            }
            strncat(script, "assign\n", sizeof(script) - strlen(script) - 1);
        }
    }
    fclose(f);

    if (part_no == 0) { printf("No valid partitions in config file.\n"); return HI_ERR_PARAM; }

    printf("Importing %d partition(s) to disk %lu...\n", part_no, phys);
    r = run_diskpart_script(script);
    if (r == HI_OK) printf("Partition table imported successfully.\n");
    else printf("Import failed.\n");
    return r;
}

int hi_clone(const char *src_drive, const char *dst_drive) {
    if (!hi_is_admin()) { printf("Admin rights required.\n"); return HI_ERR_NO_ADMIN; }
    char src_path[32], dst_path[32];
    HANDLE hSrc, hDst;
    DWORD br, bw;
    BYTE buffer[65536];
    ULONGLONG total = 0;

    snprintf(src_path, sizeof(src_path), "\\\\.\\%s", src_drive);
    snprintf(dst_path, sizeof(dst_path), "\\\\.\\%s", dst_drive);

    hSrc = CreateFileA(src_path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (hSrc == INVALID_HANDLE_VALUE) { printf("Cannot open source %s\n", src_drive); return HI_ERR_NOT_FOUND; }

    hDst = CreateFileA(dst_path, GENERIC_WRITE, FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    if (hDst == INVALID_HANDLE_VALUE) { printf("Cannot open destination %s\n", dst_drive); CloseHandle(hSrc); return HI_ERR_NOT_FOUND; }

    printf("Cloning %s -> %s...\n", src_drive, dst_drive);
    while (ReadFile(hSrc, buffer, sizeof(buffer), &br, NULL) && br > 0) {
        WriteFile(hDst, buffer, br, &bw, NULL);
        total += br;
        printf("\r%llu MB copied", total / 1048576);
    }
    printf("\nClone complete (%llu bytes).\n", total);
    CloseHandle(hSrc);
    CloseHandle(hDst);
    return HI_OK;
}

int hi_sectors(const char *drive, DWORD lba, DWORD count) {
    char path[32];
    BYTE buffer[65536];
    DWORD bytes;
    HANDLE hVol;

    snprintf(path, sizeof(path), "\\\\.\\%s", drive);
    hVol = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                       NULL, OPEN_EXISTING, 0, NULL);
    if (hVol == INVALID_HANDLE_VALUE) {
        printf("Cannot open %s\n", drive);
        return HI_ERR_NOT_FOUND;
    }

    DISK_GEOMETRY geom;
    if (!DeviceIoControl(hVol, IOCTL_DISK_GET_DRIVE_GEOMETRY, NULL, 0,
                         &geom, sizeof(geom), &bytes, NULL)) {
        CloseHandle(hVol);
        return HI_ERR_GENERAL;
    }

    DWORD max_read = sizeof(buffer) / geom.BytesPerSector;
    DWORD to_read = (count < max_read) ? count : max_read;

    LARGE_INTEGER offset;
    offset.QuadPart = (ULONGLONG)lba * geom.BytesPerSector;
    if (!SetFilePointerEx(hVol, offset, NULL, FILE_BEGIN)) {
        printf("Seek error.\n");
        CloseHandle(hVol);
        return HI_ERR_GENERAL;
    }

    if (!ReadFile(hVol, buffer, to_read * geom.BytesPerSector, &bytes, NULL)) {
        printf("Read error (error %lu)\n", GetLastError());
        CloseHandle(hVol);
        return HI_ERR_GENERAL;
    }

    DWORD sectors_read = bytes / geom.BytesPerSector;
    printf("Sectors %lu-%lu (%lu bytes, %lu sectors):\n\n", lba, lba + sectors_read - 1, bytes, sectors_read);
    for (DWORD i = 0; i < sectors_read; i++) {
        printf("=== Sector %lu (offset %llu) ===\n", lba + i,
               (ULONGLONG)(lba + i) * geom.BytesPerSector);
        if (i == 0) {
            BYTE *s = buffer + i * geom.BytesPerSector;
            for (DWORD j = 0; j < geom.BytesPerSector; j += 16) {
                printf("%08lX: ", j);
                for (DWORD k = 0; k < 16 && j + k < geom.BytesPerSector; k++)
                    printf("%02X ", s[j + k]);
                printf(" ");
                for (DWORD k = 0; k < 16 && j + k < geom.BytesPerSector; k++)
                    printf("%c", isprint(s[j + k]) ? s[j + k] : '.');
                printf("\n");
            }
        }
    }

    CloseHandle(hVol);
    return HI_OK;
}

int hi_help(void) {
    printf("hiDisk v1.0 - Disk Management Utility\n");
    printf("Developed by ExEintel\n\n");
    printf("Usage:\n");
    printf("  hiDisk.exe <drive:> <key> [value]\n\n");
    printf("Keys:\n");
    printf("  -i                   Show disk information\n");
    printf("  -fr <fs>             Format drive (ntfs, fat32, fat, fat12, exfat, refs)\n");
    printf("  -rm                  Delete partition\n");
    printf("  -c <size_MB>         Create partition\n");
    printf("  -off                 Hide drive (remove from Explorer)\n");
    printf("  -on                  Restore hidden drive\n");
    printf("  -df                  Defragment drive\n");
    printf("  -kill mbr            Erase MBR (boot sector)\n");
    printf("  -resize <size_MB>    Resize partition (extend/shrink)\n");
    printf("  -check               Check drive for errors\n");
    printf("  -label <name>        Set volume label\n");
    printf("  -backup <file>       Create disk image (.img/.iso)\n");
    printf("  -restore <file>      Restore disk image\n");
    printf("  -clean               Zero-fill entire disk\n");
    printf("  -mount <folder>      Mount drive to folder\n");
    printf("  -unmount             Unmount drive\n");
    printf("  -export <file.csv>   Export partition table to CSV\n");
    printf("  -import <file.cfg>   Import partition config\n");
    printf("  -clone <dst:>        Clone drive to another\n");
    printf("  -sectors <lba> <n>   Read raw sectors (hex dump)\n");
    printf("  -help                Show this help\n\n");
    printf("Exit Codes:\n");
    printf("  0  Success\n");
    printf("  1  Invalid parameters\n");
    printf("  2  Drive not found\n");
    printf("  3  Insufficient privileges\n");
    printf("  99 General error\n\n");
    printf("Examples:\n");
    printf("  hiDisk.exe E: -i\n");
    printf("  hiDisk.exe E: -fr ntfs\n");
    printf("  hiDisk.exe E: -c 4096\n");
    printf("  hiDisk.exe A: -fr fat12\n");
    printf("  hiDisk.exe E: -label MYDRIVE\n");
    printf("  hiDisk.exe E: -backup backup.iso\n");
    printf("  hiDisk.exe E: -sectors 0 3\n");
    printf("  hiDisk.exe \\\\.\\PhysicalDrive0 -kill mbr\n\n");
    return HI_OK;
}

int hi_read_mbr(const char *drive, MBR *mbr) {
    char path[32];
    HANDLE hDisk;
    DWORD bytes;
    DWORD phys;

    if (get_physical_index(drive, &phys) != HI_OK) {
        snprintf(path, sizeof(path), "\\\\.\\%s", drive);
    } else {
        snprintf(path, sizeof(path), "\\\\.\\PhysicalDrive%lu", phys);
    }

    hDisk = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                        NULL, OPEN_EXISTING, 0, NULL);
    if (hDisk == INVALID_HANDLE_VALUE) return HI_ERR_NOT_FOUND;

    BOOL ok = ReadFile(hDisk, mbr, sizeof(MBR), &bytes, NULL) && bytes == sizeof(MBR);
    CloseHandle(hDisk);
    return ok ? HI_OK : HI_ERR_GENERAL;
}

int hi_write_mbr(const char *drive, MBR *mbr) {
    char path[32];
    HANDLE hDisk;
    DWORD bytes;
    DWORD phys;

    if (get_physical_index(drive, &phys) != HI_OK) return HI_ERR_NOT_FOUND;
    snprintf(path, sizeof(path), "\\\\.\\PhysicalDrive%lu", phys);

    hDisk = CreateFileA(path, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
                        NULL, OPEN_EXISTING, 0, NULL);
    if (hDisk == INVALID_HANDLE_VALUE) return HI_ERR_NO_ADMIN;

    BOOL ok = WriteFile(hDisk, mbr, sizeof(MBR), &bytes, NULL) && bytes == sizeof(MBR);
    CloseHandle(hDisk);
    return ok ? HI_OK : HI_ERR_GENERAL;
}
