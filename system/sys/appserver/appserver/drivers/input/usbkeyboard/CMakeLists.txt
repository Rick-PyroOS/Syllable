# usbkeyboard input driver (appserver add-on)
syllable_plugin(usbkeyboard SOURCES keyboard.cpp
  INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}/../../../server
  LIBS libsyllable INSTALL system/drivers/appserver/input)
