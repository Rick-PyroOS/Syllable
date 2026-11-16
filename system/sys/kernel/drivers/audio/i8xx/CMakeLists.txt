# Intel ICH (i8xx) AC'97 audio driver
syllable_driver(i8xx SOURCES audio.c ac97audio.c i8xx.c INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}/.. INSTALL system/drivers/dev/audio)
