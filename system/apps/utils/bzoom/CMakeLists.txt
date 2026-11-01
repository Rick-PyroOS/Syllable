# BZoom: screen magnifier
syllable_program(bzoom OUTPUT_NAME BZoom SOURCES main.cpp gui/zoomview.cpp
  LIBS libsyllable m STRIP INSTALL applications/BZoom)
syllable_resources(bzoom FILES icon48x48.png)
syllable_catalogs(bzoom NAME BZoom DIR resources LANGS bg da de el es fr hu nl pt-br ru sv EMBED)
