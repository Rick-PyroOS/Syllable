# ext2 file system
syllable_driver(ext2 SOURCES ext2.c INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}/.. OPTIONS -Wno-multichar
  INSTALL system/drivers/fs)
