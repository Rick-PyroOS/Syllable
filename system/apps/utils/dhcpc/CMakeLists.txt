# DHCP client (network-start.sh)
syllable_program(dhcpc SOURCES client.c dhcp.c packet.c interface.c state.c inbound.c ntp.c
  DEFINES ENABLE_DEBUG DEBUG_LEVEL=0 STRIP INSTALL system/programs)
