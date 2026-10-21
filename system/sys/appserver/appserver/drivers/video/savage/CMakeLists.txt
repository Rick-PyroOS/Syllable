# savage video driver (appserver add-on).  FreeType headers only.
syllable_plugin(video_savage OUTPUT_NAME savage SOURCES savage_driver.cpp savage_accel.cpp savage_video.cpp
  INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}/../../../server
           ${SYLLABLE_STAGING}/include/freetype2 ${SYLLABLE_STAGING}/include
  INSTALL system/drivers/appserver/video)
add_dependencies(video_savage freetype)
