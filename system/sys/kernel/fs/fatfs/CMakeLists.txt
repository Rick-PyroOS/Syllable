# FAT file system
syllable_driver(fatfs
  SOURCES dosfs.c dir.c dlist.c fat.c file.c iter.c util.c vcache.c version.c attr.c
          textencoding/encodings.cpp textencoding/EncodingConversions.cpp textencoding/UnicodeMappings.cpp
  INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}/textencoding ${CMAKE_CURRENT_SOURCE_DIR}/..
  OPTIONS -Wno-multichar -fno-strict-aliasing
  INSTALL system/drivers/fs)
