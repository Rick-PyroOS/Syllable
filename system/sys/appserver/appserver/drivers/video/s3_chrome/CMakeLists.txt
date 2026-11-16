# s3_chrome video driver (appserver add-on).  FreeType headers only.
syllable_plugin(video_s3_chrome OUTPUT_NAME s3_chrome SOURCES s3_chrome.cpp
  INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}/../../../server
           ${SYLLABLE_STAGING}/include/freetype2 ${SYLLABLE_STAGING}/include
  INSTALL system/drivers/appserver/video)
add_dependencies(video_s3_chrome freetype)
