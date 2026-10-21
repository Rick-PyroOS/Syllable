# SCSI bus driver (disks, CD-ROMs, generic)
syllable_driver(scsi SOURCES scsi.c scsi_common.c scsi_generic.c scsi_disk.c scsi_cdrom.c
  DEFINES __ENABLE_DEBUG__ INSTALL system/drivers/dev/bus)
