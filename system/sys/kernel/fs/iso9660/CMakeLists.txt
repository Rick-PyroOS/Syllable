# ISO 9660 (CD-ROM) file system
syllable_driver(iso9660 SOURCES iso.c kernel_interface.c OPTIONS -fno-strict-aliasing
  INSTALL system/drivers/fs)
target_link_options(iso9660 PRIVATE -lgcc)
