# Mulvane window decorator (appserver add-on).  FreeType headers only.
syllable_plugin(deco_mulvane OUTPUT_NAME Mulvane SOURCES MulvaneDecorator.cpp
  INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}/../../../server
           ${SYLLABLE_STAGING}/include/freetype2 ${SYLLABLE_STAGING}/include
  LIBS libsyllable syllable::z INSTALL system/drivers/appserver/decorators)
add_dependencies(deco_mulvane freetype)
