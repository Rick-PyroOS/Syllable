# PS/2 mouse input driver (appserver add-on)
syllable_plugin(ps2mouse SOURCES ps2mouse.cpp
  INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}/../../../server
  LIBS libsyllable INSTALL system/drivers/appserver/input)
