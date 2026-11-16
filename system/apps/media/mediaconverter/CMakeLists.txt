# Media Converter
syllable_program(mediaconverter OUTPUT_NAME MediaConverter SOURCES MediaConverter.cpp
  OPTIONS -ffast-math LIBS libsyllable libmedia INSTALL applications/media)
syllable_attribute(applications/media/MediaConverter os::Category Media)
