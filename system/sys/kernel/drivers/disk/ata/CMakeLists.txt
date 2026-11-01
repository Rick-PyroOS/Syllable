# ATA/ATAPI disk driver (the older, non-busmanager one)
syllable_driver(ata_disk OUTPUT_NAME ata SOURCES ata.c ata-drive.c atapi-drive.c ata-io.c ata-probe.c ata-dma.c
  DEFINES __ENABLE_DEBUG__ INSTALL system/drivers/dev/disk)
