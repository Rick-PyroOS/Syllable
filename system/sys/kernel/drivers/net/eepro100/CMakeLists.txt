# Intel EtherExpress Pro/100 driver
# -O2: it needs optimising (the Makefile left -O to the environment's CFLAGS)
syllable_driver(eepro100 SOURCES eepro100.c OPTIONS -O2 INSTALL system/drivers/dev/net/eth)
