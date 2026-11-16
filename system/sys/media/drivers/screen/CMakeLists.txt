# Video output to the screen
syllable_library(screen SONAME screen.so SOURCES yuv2rgb.c rgb2rgb.c screen.cpp
  OPTIONS -ffast-math -fno-PIC LIBS libsyllable libmedia STRIP INSTALL system/extensions/media)
