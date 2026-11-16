# sis3xx video driver (appserver add-on).  FreeType headers only.
syllable_plugin(video_sis3xx OUTPUT_NAME sis3xx SOURCES init301.c init.c sis_video.cpp sis.c sis3xx.cpp
  INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}/../../../server
           ${SYLLABLE_STAGING}/include/freetype2 ${SYLLABLE_STAGING}/include
  INSTALL system/drivers/appserver/video)
add_dependencies(video_sis3xx freetype)
