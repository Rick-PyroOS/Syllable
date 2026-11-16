# Win98 window decorator (appserver add-on).  FreeType headers only.
syllable_plugin(deco_win98 OUTPUT_NAME Win98 SOURCES Win98Decorator.cpp
  INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}/../../../server
           ${SYLLABLE_STAGING}/include/freetype2 ${SYLLABLE_STAGING}/include
  INSTALL system/drivers/appserver/decorators)
add_dependencies(deco_win98 freetype)
