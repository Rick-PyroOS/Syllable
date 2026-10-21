# AView: image viewer (applications/AView/AView, linked as system/programs/aview)
syllable_program(aview OUTPUT_NAME AView SOURCES main.cpp appwindow.cpp iview.cpp statusbar.cpp
  LIBS libsyllable INSTALL applications/AView)
file(GLOB _res RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/icons/*.png)
syllable_resources(aview FILES ${_res})
syllable_catalogs(aview NAME AView DIR resources LANGS bg da de el es fr hu nl pl pt-br ru sv EMBED)
install(CODE "file(CREATE_LINK /applications/AView/AView
  \$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/system/programs/aview SYMBOLIC)")
