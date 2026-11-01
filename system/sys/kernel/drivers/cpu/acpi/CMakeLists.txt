# ACPI processor driver (C- and P-states)
syllable_driver(acpi_cpu SOURCES acpi_processor.c acpi_cstate.c acpi_pstate.c
  INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}/../../bus/acpi/include INSTALL system/drivers/dev)
