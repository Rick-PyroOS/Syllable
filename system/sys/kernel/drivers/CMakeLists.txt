# Kernel drivers.  ps2, bus/pci, bus/ata, hcd/ata_pci and disk/bios are
# needed to boot from an ATA disk.
add_subdirectory(ps2)
add_subdirectory(bus)
add_subdirectory(hcd)
add_subdirectory(cpu)
add_subdirectory(disk)
add_subdirectory(graphics)
add_subdirectory(net)
add_subdirectory(audio)
add_subdirectory(sound)
