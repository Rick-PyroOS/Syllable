# Blue window decorator (appserver add-on).  FreeType headers only.
syllable_plugin(deco_blue OUTPUT_NAME Blue SOURCES BlueDecorator.cpp
  INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}/../../../server
           ${SYLLABLE_STAGING}/include/freetype2 ${SYLLABLE_STAGING}/include
  INSTALL system/drivers/appserver/decorators)
add_dependencies(deco_blue freetype)
