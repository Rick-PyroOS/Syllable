# Intel High Definition Audio driver
syllable_driver(hda SOURCES audio.c hda_codec.c hda.c INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}/.. INSTALL system/drivers/dev/audio)
