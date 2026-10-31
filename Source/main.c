#include "lib.h"
#include "floppy.h"

static void print_banner(void) {
    printf("hiDisk v1.0 - Disk Management Utility\n");
    printf("Developed by ExEintel\n");
    printf("----------------------------------------\n\n");
}

static int needs_admin(const char *cmd) {
    const char *admin_cmds[] = {
        "-fr", "-rm", "-c", "-df", "-kill", "-resize",
        "-restore", "-clean", "-import", "-clone", NULL
    };
    for (int i = 0; admin_cmds[i]; i++) {
        if (_stricmp(cmd, admin_cmds[i]) == 0) return 1;
    }
    return 0;
}

int main(int argc, char *argv[]) {
    print_banner();

    if (argc < 2) {
        hi_help();
        return HI_ERR_PARAM;
    }

    if (strcmp(argv[1], "-help") == 0 || strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "/?") == 0) {
        return hi_help();
    }

    if (argc < 3) {
        printf("Error: Missing command. Usage: hiDisk.exe <drive:> <key> [value]\n");
        return HI_ERR_PARAM;
    }

    const char *drive = argv[1];
    const char *cmd = argv[2];
    const char *value = (argc > 3) ? argv[3] : NULL;
    const char *value2 = (argc > 4) ? argv[4] : NULL;

    if (needs_admin(cmd) && !hi_is_admin()) {
        printf("Error: This command requires administrator privileges.\n");
        printf("Please run hiDisk.exe as Administrator.\n");
        return HI_ERR_NO_ADMIN;
    }

    if (drive[1] != ':') {
        printf("Error: Invalid drive format. Use format X:\n");
        return HI_ERR_PARAM;
    }

    int is_floppy = hi_is_floppy(drive);
    int r = HI_OK;

    if (strcmp(cmd, "-i") == 0) {
        if (is_floppy) {
            hi_floppy_info(drive);
        }
        r = hi_get_info(drive);

    } else if (strcmp(cmd, "-fr") == 0) {
        if (!value) {
            printf("Error: -fr requires a filesystem type (ntfs, fat32, fat12, etc.)\n");
            return HI_ERR_PARAM;
        }
        if (is_floppy || (_stricmp(value, "fat12") == 0)) {
            r = hi_floppy_format(drive);
        } else {
            r = hi_format(drive, value);
        }

    } else if (strcmp(cmd, "-rm") == 0) {
        r = hi_delete_partition(drive);

    } else if (strcmp(cmd, "-c") == 0) {
        if (!value) {
            printf("Error: -c requires size in MB\n");
            return HI_ERR_PARAM;
        }
        DWORD size = atol(value);
        if (size < 8) {
            printf("Error: Minimum partition size is 8 MB\n");
            return HI_ERR_PARAM;
        }
        r = hi_create_partition(drive, size);

    } else if (strcmp(cmd, "-off") == 0) {
        r = hi_offline(drive);

    } else if (strcmp(cmd, "-on") == 0) {
        r = hi_online(drive);

    } else if (strcmp(cmd, "-df") == 0) {
        r = hi_defrag(drive);

    } else if (strcmp(cmd, "-kill") == 0) {
        if (value && _stricmp(value, "mbr") == 0) {
            r = hi_kill_mbr(drive);
        } else {
            printf("Error: -kill requires 'mbr' argument\n");
            return HI_ERR_PARAM;
        }

    } else if (strcmp(cmd, "-resize") == 0) {
        if (!value) {
            printf("Error: -resize requires size in MB\n");
            return HI_ERR_PARAM;
        }
        DWORD size = atol(value);
        r = hi_resize(drive, size);

    } else if (strcmp(cmd, "-check") == 0) {
        r = hi_check(drive);

    } else if (strcmp(cmd, "-label") == 0) {
        if (!value) {
            printf("Error: -label requires a name\n");
            return HI_ERR_PARAM;
        }
        r = hi_set_label(drive, value);

    } else if (strcmp(cmd, "-backup") == 0) {
        if (!value) {
            printf("Error: -backup requires output filename\n");
            return HI_ERR_PARAM;
        }
        if (is_floppy) {
            r = hi_floppy_create_image(drive, value);
        } else {
            r = hi_backup(drive, value);
        }

    } else if (strcmp(cmd, "-restore") == 0) {
        if (!value) {
            printf("Error: -restore requires input filename\n");
            return HI_ERR_PARAM;
        }
        r = hi_restore(drive, value);

    } else if (strcmp(cmd, "-clean") == 0) {
        r = hi_clean(drive);

    } else if (strcmp(cmd, "-mount") == 0) {
        if (!value) {
            printf("Error: -mount requires a folder path\n");
            return HI_ERR_PARAM;
        }
        r = hi_mount(drive, value);

    } else if (strcmp(cmd, "-unmount") == 0) {
        r = hi_unmount(drive);

    } else if (strcmp(cmd, "-export") == 0) {
        if (!value) {
            printf("Error: -export requires filename.csv\n");
            return HI_ERR_PARAM;
        }
        r = hi_export_csv(drive, value);

    } else if (strcmp(cmd, "-import") == 0) {
        if (!value) {
            printf("Error: -import requires filename.cfg\n");
            return HI_ERR_PARAM;
        }
        r = hi_import_cfg(drive, value);

    } else if (strcmp(cmd, "-clone") == 0) {
        if (!value) {
            printf("Error: -clone requires destination drive (e.g. Y:)\n");
            return HI_ERR_PARAM;
        }
        r = hi_clone(drive, value);

    } else if (strcmp(cmd, "-sectors") == 0) {
        if (!value || !value2) {
            printf("Error: -sectors requires LBA and count\n");
            return HI_ERR_PARAM;
        }
        DWORD lba = atol(value);
        DWORD count = atol(value2);
        r = hi_sectors(drive, lba, count);

    } else if (strcmp(cmd, "-list") == 0 || strcmp(cmd, "-ls") == 0) {
        if (is_floppy) {
            r = hi_floppy_list(drive);
        } else {
            printf("File listing is only supported for floppy disks.\n");
            r = HI_ERR_PARAM;
        }

    } else if (strcmp(cmd, "-help") == 0 || strcmp(cmd, "-h") == 0) {
        r = hi_help();

    } else {
        printf("Error: Unknown command '%s'. Use -help for usage.\n", cmd);
        r = HI_ERR_PARAM;
    }

    return r;
}
