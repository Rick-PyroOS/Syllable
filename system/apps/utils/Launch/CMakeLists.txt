# Launch: run a program by name
syllable_program(launch OUTPUT_NAME Launch
  SOURCES main.cpp application.cpp commonfuncs.cpp execute.cpp mainwindow.cpp
  OPTIONS -ffast-math LIBS libsyllable INSTALL applications)
file(GLOB _res RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/images/*.png)
syllable_resources(launch FILES ${_res})
syllable_catalogs(launch NAME Launch DIR resources LANGS da de el es hu nl pl pt-br ru sv EMBED)
