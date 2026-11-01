# Blur visualisation plugin for ColdFish
syllable_library(coldfish_blur SONAME Blur
  SOURCES bitmap.cpp blur.cpp color.cpp config.cpp img.cpp loop.cpp paste.cpp render.cpp text.cpp blursk.cpp
  INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}/../../coldfish
  LIBS libsyllable STRIP INSTALL applications/media/plug-ins)
