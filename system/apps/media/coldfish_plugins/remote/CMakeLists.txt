# Remote-control plugin for ColdFish
syllable_library(coldfish_remote SONAME Remote SOURCES Remote.cpp
  INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}/../../coldfish
  LIBS libsyllable STRIP INSTALL applications/media/plug-ins)
