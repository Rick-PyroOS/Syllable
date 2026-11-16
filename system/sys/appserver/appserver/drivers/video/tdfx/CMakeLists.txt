# tdfx video driver (appserver add-on).  FreeType headers only.
syllable_plugin(video_tdfx OUTPUT_NAME tdfx SOURCES tdfx.cpp gtf.cpp
  INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}/../../../server
           ${SYLLABLE_STAGING}/include/freetype2 ${SYLLABLE_STAGING}/include
  LIBS libsyllable INSTALL system/drivers/appserver/video)
add_dependencies(video_tdfx freetype)
