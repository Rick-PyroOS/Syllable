# i855 video driver (appserver add-on).  FreeType headers only.
syllable_plugin(video_i855 OUTPUT_NAME i855 SOURCES i855.cpp i855_video.cpp
  INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}/../../../server
           ${SYLLABLE_STAGING}/include/freetype2 ${SYLLABLE_STAGING}/include
  INSTALL system/drivers/appserver/video)
add_dependencies(video_i855 freetype)
