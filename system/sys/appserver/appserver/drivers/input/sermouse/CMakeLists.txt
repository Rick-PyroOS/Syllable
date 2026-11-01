# sermouse input driver (appserver add-on)
syllable_plugin(sermouse SOURCES sermouse.cpp
  INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}/../../../server
  LIBS libsyllable INSTALL system/drivers/appserver/input)
