# 3Com 3c59x/3c90x Ethernet driver
# -O2: it needs optimising (the Makefile left -O to the environment's CFLAGS)
syllable_driver(3c59x SOURCES 3c59x.c OPTIONS -O2 INSTALL system/drivers/dev/net/eth)
