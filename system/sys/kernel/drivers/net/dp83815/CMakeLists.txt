# National Semiconductor DP83815 Ethernet driver
# -O2: it needs optimising (the Makefile left -O to the environment's CFLAGS)
syllable_driver(dp83815 SOURCES dp83815.c DEFINES __ENABLE_DEBUG__ OPTIONS -O2 INSTALL system/drivers/dev/net/eth)
