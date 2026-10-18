# Trident 4DWave audio driver
# -O2: it needs optimising (the Makefile left -O to the environment's CFLAGS)
syllable_driver(trident SOURCES ac97_codec.c trident.c OPTIONS -O2 INSTALL system/drivers/dev/audio)
