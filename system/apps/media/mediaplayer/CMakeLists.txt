# Media Player
syllable_program(mediaplayer OUTPUT_NAME MediaPlayer SOURCES seekslider.cpp cimagebutton.cpp MediaPlayer.cpp
  OPTIONS -ffast-math LIBS libsyllable libmedia INSTALL applications/media)
file(GLOB _res RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/images/*.png)
syllable_resources(mediaplayer FILES ${_res})
syllable_catalogs(mediaplayer NAME mediaplayer DIR resources LANGS da de es fr hu nl pl pt-br ru sv EMBED)
syllable_attribute(applications/media/MediaPlayer os::Category Media)
