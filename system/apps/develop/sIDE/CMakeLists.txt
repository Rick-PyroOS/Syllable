# sIDE: the project manager
syllable_program(side OUTPUT_NAME sIDE
  SOURCES AddWindow.cpp AddLibrary.cpp ProjectPrefs.cpp GroupWindow.cpp ProjectWindow.cpp
          NewProject.cpp project.cpp main.cpp
  OPTIONS -Wall LIBS libsyllable INSTALL applications/sIDE)
file(GLOB _res RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/images/*.png)
syllable_resources(side FILES ${_res})
syllable_catalogs(side NAME sIDE DIR resources LANGS da de el fr hu nl pt-br ru sv EMBED)
syllable_attribute(applications/sIDE/sIDE os::Category Development)
