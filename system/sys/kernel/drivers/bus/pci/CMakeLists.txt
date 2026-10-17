# PCI bus manager (pci_irq_router.c is not part of it)
syllable_driver(pci
  SOURCES pci.c pci_acpi_link.c pci_acpi_router.c pci_registers.c
  INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}/../acpi/include
  DEFINES __ENABLE_DEBUG__
  OPTIONS -Wstrict-prototypes -fno-strict-aliasing -Wno-trigraphs -fno-common
  INSTALL system/drivers/dev/bus)
