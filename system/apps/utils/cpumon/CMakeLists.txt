# CPU Monitor (applications/system/information/CPU Monitor, linked as system/programs/cpumon)
syllable_program(cpumon SOURCES main.cpp graphview.cpp barview.cpp LIBS libsyllable)
syllable_resources(cpumon FILES icon48x48.png)
syllable_catalogs(cpumon NAME CPU-Monitor DIR resources LANGS da de el es fr hu nl pt-br ru sv EMBED)
install(PROGRAMS $<TARGET_FILE:cpumon> DESTINATION applications/system/information RENAME "CPU Monitor")
syllable_attribute("applications/system/information/CPU Monitor" os::Category "System Tools")
install(CODE "file(MAKE_DIRECTORY \$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/system/programs)
  file(CREATE_LINK \"/applications/system/information/CPU Monitor\"
  \$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/system/programs/cpumon SYMBOLIC)")
