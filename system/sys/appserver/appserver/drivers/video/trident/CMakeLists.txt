# trident video driver (appserver add-on).  FreeType headers only.
syllable_plugin(video_trident OUTPUT_NAME trident SOURCES trident.cpp
  INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}/../../../server
           ${SYLLABLE_STAGING}/include/freetype2 ${SYLLABLE_STAGING}/include
  INSTALL system/drivers/appserver/video)
add_dependencies(video_trident freetype)
