# vmware video driver (appserver add-on).  FreeType headers only.
syllable_plugin(video_vmware OUTPUT_NAME vmware SOURCES vmware.cpp vmware_cursor.cpp vmware_fb.cpp vmware_fifo.cpp vmware_io.cpp
  INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}/../../../server
           ${SYLLABLE_STAGING}/include/freetype2 ${SYLLABLE_STAGING}/include
  INSTALL system/drivers/appserver/video)
add_dependencies(video_vmware freetype)
