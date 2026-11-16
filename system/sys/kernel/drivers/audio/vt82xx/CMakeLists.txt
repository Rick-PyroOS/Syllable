# VIA VT82xx AC'97 audio driver
syllable_driver(vt82xx SOURCES audio.c ac97audio.c vt82xx.c INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}/.. INSTALL system/drivers/dev/audio)
