# VIA Rhine Ethernet driver
# -O2: it needs optimising (the Makefile left -O to the environment's CFLAGS)
syllable_driver(via SOURCES via.c OPTIONS -O2 INSTALL system/drivers/dev/net/eth)
