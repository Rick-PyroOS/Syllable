# radeon video driver (appserver add-on).  FreeType headers only.
syllable_plugin(video_radeon OUTPUT_NAME radeon SOURCES radeon.cpp radeon_accel.cpp radeon_misc.cpp radeon_monitor.cpp radeon_pll.cpp radeon_rom.cpp radeon_video.cpp
  INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}/../../../server
           ${SYLLABLE_STAGING}/include/freetype2 ${SYLLABLE_STAGING}/include
  INSTALL system/drivers/appserver/video)
add_dependencies(video_radeon freetype)
install(FILES radeon.cfg DESTINATION system/config/drivers/appserver/video)
