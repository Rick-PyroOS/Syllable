# Yamaha YMF PCI audio driver
syllable_driver(ymfpci SOURCES ymfpci.c ac97_codec.c DEFINES __ENABLE_DEBUG__ DEBUG_LIMIT=KERN_INFO INSTALL system/drivers/dev/audio)
