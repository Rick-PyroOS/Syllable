# WarpSpeed window decorator (appserver add-on).  FreeType headers only.
syllable_plugin(deco_warpspeed OUTPUT_NAME WarpSpeed SOURCES warpspeeddecorator.cpp
  INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}/../../../server
           ${SYLLABLE_STAGING}/include/freetype2 ${SYLLABLE_STAGING}/include
  INSTALL system/drivers/appserver/decorators)
add_dependencies(deco_warpspeed freetype)
