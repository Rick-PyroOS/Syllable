# ATA/ATAPI bus manager
syllable_driver(ata
  SOURCES ata.c ata_port.c ata_probe.c ata_io.c ata_command.c ata_drive.c atapi_drive.c atapi.c
  DEFINES __ENABLE_DEBUG__
  INSTALL system/drivers/dev/bus)
