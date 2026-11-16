# mach64 video driver (appserver add-on).  FreeType headers only.
syllable_plugin(video_mach64 OUTPUT_NAME mach64 SOURCES mach64_vesa.cpp mach64_accel.cpp mach64_crtc.cpp mach64_io.cpp mach64_misc.cpp mach64_video.cpp mach64_ct.cpp mach64.cpp
  INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}/../../../server
           ${SYLLABLE_STAGING}/include/freetype2 ${SYLLABLE_STAGING}/include
  INSTALL system/drivers/appserver/video)
add_dependencies(video_mach64 freetype)
