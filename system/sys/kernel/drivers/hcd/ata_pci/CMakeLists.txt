# PCI ATA controllers (generic, VIA, Intel, SiS, AMD)
syllable_driver(ata_pci
  SOURCES ata_pci.c ata_via.c ata_intel.c ata_sis.c ata_amd.c
  INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}/../../bus/ata
  DEFINES __ENABLE_DEBUG__
  INSTALL system/drivers/dev/hcd)
