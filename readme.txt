================================================================
  hiDisk v1.0 - Disk Management Utility
  Developed by ExEintel
================================================================

DESCRIPTION
-----------
hiDisk is a command-line disk management utility for Windows.
It supports formatting, partition management, disk imaging,
floppy disk operations (FAT12), defragmentation, and more.

SYSTEM REQUIREMENTS
-------------------
- OS: Windows 7/8/10/11 (x86/x64)
- Compiler: MinGW-w64 GCC (for building from source)
- Admin rights required for most operations (format, partition, etc.)

FILES
-----
hiDisk\Source\main.c       - Main entry point and command parser
hiDisk\Source\lib.c        - Core disk operation library
hiDisk\Source\lib.h        - Library header
hiDisk\Source\floppy.c     - Floppy disk (FAT12) operations
hiDisk\Source\floppy.h     - Floppy header
hiDisk\Source\logo.rc      - Resource script for icon
hiDisk\readme.txt          - This file

COMMAND SYNTAX
--------------
  hiDisk.exe <drive:> <key> [value]

KEYS
----
  -i                   Show complete disk information
  -fr <fs>             Format drive (ntfs, fat32, fat, fat12, exfat, refs)
  -rm                  Delete the partition associated with the drive letter
  -c <size_MB>         Create a new partition on the disk
  -off                 Hide drive (remove from Explorer)
  -on                  Restore hidden drive
  -df                  Defragment the drive
  -kill mbr            Erase the Master Boot Record (sector 0)
  -resize <size_MB>    Extend or shrink partition
  -check               Check drive for file system errors
  -label <name>        Set volume label
  -backup <file>       Create raw disk image (.img/.iso)
  -restore <file>      Restore disk from image
  -clean               Zero-fill every sector on the disk
  -mount <folder>      Mount volume to a directory
  -unmount             Unmount the volume
  -export <file.csv>   Export partition table to CSV
  -import <file.cfg>   Import partition configuration
  -clone <dst:>        Clone source drive to destination
  -sectors <lba> <n>   Display raw hex dump of sectors
  -ls                  List files on FAT12 floppy disk
  -help                Show this help message

EXIT CODES
----------
  0   Success
  1   Invalid parameters or syntax error
  2   Drive or device not found
  3   Insufficient privileges (not running as admin)
  99  General/system error

EXAMPLES
--------
  Show disk information:
    hiDisk.exe E: -i

  Quick-format drive E: as NTFS:
    hiDisk.exe E: -fr ntfs

  Format floppy as FAT12:
    hiDisk.exe A: -fr fat12

  Create 8 GB partition on disk containing D::
    hiDisk.exe D: -c 8192

  Delete partition assigned to E::
    hiDisk.exe E: -rm

  Hide drive E: from Explorer:
    hiDisk.exe E: -off

  Restore hidden drive:
    hiDisk.exe E: -on

  Set volume label:
    hiDisk.exe E: -label MY_DATA

  Backup entire drive to image file:
    hiDisk.exe E: -backup backup.iso

  Restore from image:
    hiDisk.exe E: -restore backup.iso

  Zero-fill entire disk (IRREVERSIBLE):
    hiDisk.exe E: -clean

  Read first 3 sectors in hex:
    hiDisk.exe E: -sectors 0 3

  Export partition table:
    hiDisk.exe E: -export partitions.csv

  Import partition config:
    hiDisk.exe E: -import config.cfg

  Clone drive E: to drive F::
    hiDisk.exe E: -clone F:

  List files on floppy:
    hiDisk.exe A: -ls

  Erase MBR (boot sector):
    hiDisk.exe E: -kill mbr

IMPORT CONFIG FORMAT (for -import)
-----------------------------------
  # Partition table configuration
  # Format: <filesystem>,<size_MB>
  ntfs,40960
  fat32,8192
  ntfs,102400

  This will create three partitions with automatic assignment
  and formatting.

FLOPPY DISK SUPPORT
-------------------
  hiDisk supports 3.5" 1.44MB floppy disks (FAT12):
  - Use drive letters A: or B:
  - Format as FAT12: hiDisk.exe A: -fr fat12
  - List directory:  hiDisk.exe A: -ls
  - Create image:    hiDisk.exe A: -backup floppy.img
  - View info:       hiDisk.exe A: -i

COMPILATION (MinGW-w64 GCC)
----------------------------

  Basic build (without icon):
    gcc -o hiDisk.exe Source\main.c Source\lib.c Source\floppy.c ^
        -lsetupapi -O2 -s -Wall -Wextra

  Build with icon (requires logo.ico):
    windres Source\logo.rc -O coff -o logo_res.o
    gcc -o hiDisk.exe Source\main.c Source\lib.c Source\floppy.c ^
        logo_res.o -lsetupapi -O2 -s -Wall -Wextra
    del logo_res.o

  Build with debug symbols:
    gcc -o hiDisk.exe Source\main.c Source\lib.c Source\floppy.c ^
        -lsetupapi -O0 -g -Wall -Wextra

  For floppydisk.dll (separate DLL for floppy operations):
    gcc -o floppydisk.dll Source\floppy.c Source\lib.c ^
        -lsetupapi -O2 -s -shared -Wall -Wextra

NOTES
-----
  - Most operations require Administrator privileges.
  - Always backup important data before partition operations.
  - The -clean command is irreversible -- all data will be lost.
  - -kill mbr only zeroes the first 512 bytes (sector 0).
  - Floppy support requires a physical floppy drive or USB floppy.
  - For -mount, the target folder must exist and be an empty NTFS directory.
  - Use -on only after -off; the volume GUID is saved in %%APPDATA%%\hiDisk.cfg.

LICENSE
-------
  Freeware. Use at your own risk. No warranty expressed or implied.
  Copyright (C) ExEintel.
