# bochs video driver (appserver add-on): QEMU standard VGA, bochs-display and
# QXL in VGA mode, through the Bochs VBE "DISPI" registers.  FreeType headers only.
syllable_plugin(video_bochs OUTPUT_NAME bochs SOURCES bochs.cpp
  INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}/../../../server
           ${SYLLABLE_STAGING}/include/freetype2 ${SYLLABLE_STAGING}/include
  INSTALL system/drivers/appserver/video)
add_dependencies(video_bochs freetype)
