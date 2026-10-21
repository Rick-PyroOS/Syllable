# Archiver (src/).  Its progs/makeself.sh (self-extractor) is not installed.
syllable_program(archiver OUTPUT_NAME Archiver
  SOURCES src/main.cpp src/toolbar.cpp src/archiver.cpp src/new.cpp src/options.cpp src/exe.cpp
          src/extract.cpp src/statusbar.cpp src/loadbitmap.cpp src/settings.cpp src/flowlayout.cpp
  INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}/src OPTIONS -w LIBS libsyllable INSTALL applications/Archiver)
file(GLOB _res RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/src/images/*.png)
syllable_resources(archiver FILES ${_res})
