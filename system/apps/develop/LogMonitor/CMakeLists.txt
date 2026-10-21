# Log monitor.  Its main catalog is LogMonitor.catfish at the top (the
# Makefile looked for it in resources/); the Makefile added the ru catalog
# to sIDE by mistake, so it is embedded here instead.
syllable_program(log_monitor OUTPUT_NAME LogMonitor SOURCES main.cpp tailwin.cpp
  LIBS libsyllable STRIP INSTALL applications/sIDE)
file(GLOB _res RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/images/*.png)
syllable_resources(log_monitor FILES ${_res})
syllable_catalogs(log_monitor NAME LogMonitor DIR resources EXT catfish MAIN LogMonitor.catfish LANGS ru EMBED)
syllable_attribute(applications/sIDE/LogMonitor os::Category Development)
