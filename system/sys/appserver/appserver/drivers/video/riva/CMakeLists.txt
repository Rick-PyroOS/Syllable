# riva video driver (appserver add-on).  FreeType headers only.
syllable_plugin(video_riva OUTPUT_NAME riva SOURCES riva.cpp riva_hw.c
  INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}/../../../server
           ${SYLLABLE_STAGING}/include/freetype2 ${SYLLABLE_STAGING}/include
  INSTALL system/drivers/appserver/video)
add_dependencies(video_riva freetype)
