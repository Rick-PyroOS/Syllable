# CharMap: character map
syllable_program(charmap OUTPUT_NAME CharMap SOURCES main.cpp mainwin.cpp charmap.cpp
  LIBS libsyllable INSTALL applications/CharMap)
file(GLOB _res RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/images/*.png)
syllable_resources(charmap FILES ${_res})
syllable_catalogs(charmap NAME CharMap DIR resources LANGS bg da de el es fr hu nl pt-br ru sv EMBED)
