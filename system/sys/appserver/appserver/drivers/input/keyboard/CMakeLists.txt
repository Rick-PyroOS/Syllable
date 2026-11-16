# Keyboard input driver (appserver add-on)
syllable_plugin(keyboard SOURCES keyboard.cpp
  INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}/../../../server
  LIBS libsyllable INSTALL system/drivers/appserver/input)
