# Photon window decorator (appserver add-on).  FreeType headers only.
syllable_plugin(photon OUTPUT_NAME Photon SOURCES PhotonDecorator.cpp
  INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}/../../../server
           ${SYLLABLE_STAGING}/include/freetype2 ${SYLLABLE_STAGING}/include
  INSTALL system/drivers/appserver/decorators)
add_dependencies(photon freetype)
