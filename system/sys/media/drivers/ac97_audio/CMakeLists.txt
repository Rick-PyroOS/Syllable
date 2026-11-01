# AC'97 audio output (for the kernel's audio drivers)
syllable_library(ac97_audio SONAME ac97_audio.so SOURCES ac97.cpp ac97channel.cpp
  LIBS libsyllable libmedia STRIP INSTALL system/extensions/media)
syllable_attribute(system/extensions/media/ac97_audio.so os::Category AudioDriver)
