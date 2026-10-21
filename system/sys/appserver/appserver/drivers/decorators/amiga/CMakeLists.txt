# Amiga window decorator (appserver add-on).  FreeType headers only.
syllable_plugin(deco_amiga OUTPUT_NAME Amiga SOURCES amigadecorator.cpp
  INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}/../../../server
           ${SYLLABLE_STAGING}/include/freetype2 ${SYLLABLE_STAGING}/include
  INSTALL system/drivers/appserver/decorators)
add_dependencies(deco_amiga freetype)
