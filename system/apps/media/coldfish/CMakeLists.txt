# ColdFish: the audio player
syllable_program(coldfish OUTPUT_NAME ColdFish SOURCES lcd.cpp SelectWin.cpp ColdFish.cpp cimagebutton.cpp
  OPTIONS -ffast-math LIBS libsyllable libmedia INSTALL applications/media)
file(GLOB _res RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/images/*.png)
syllable_resources(coldfish FILES ${_res})
syllable_catalogs(coldfish NAME coldfish DIR resources LANGS da de es fr hu nl pl pt-br ru sv EMBED)
syllable_attribute(applications/media/ColdFish os::Category Media)
