# NTFS file system (read-only)
syllable_driver(ntfs SOURCES ntfs.c util.c inode.c attr.c unistr.c support.c super.c dir.c
  DEFINES __ENABLE_DEBUG__ OPTIONS -O2 -fno-strict-aliasing INSTALL system/drivers/fs)
