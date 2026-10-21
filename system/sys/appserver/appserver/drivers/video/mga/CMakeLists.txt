# mga video driver (appserver add-on).  FreeType headers only.
syllable_plugin(video_mga OUTPUT_NAME mga SOURCES mga.cpp mga_crtc.cpp mga_gx00.cpp mga_tvp3026.cpp
  INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}/../../../server
           ${SYLLABLE_STAGING}/include/freetype2 ${SYLLABLE_STAGING}/include
  OPTIONS -fexceptions INSTALL system/drivers/appserver/video)
add_dependencies(video_mga freetype)
