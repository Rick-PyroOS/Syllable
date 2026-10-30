# Memory Monitor (applications/system/information/Memory Monitor, linked as system/programs/memmon)
syllable_program(memmon SOURCES main.cpp LIBS libsyllable)
syllable_resources(memmon FILES icon48x48.png)
syllable_catalogs(memmon NAME MemoryMonitor DIR resources LANGS da de el es fr hu nl pt-br ru sv EMBED)
install(PROGRAMS $<TARGET_FILE:memmon> DESTINATION applications/system/information RENAME "Memory Monitor")
syllable_attribute("applications/system/information/Memory Monitor" os::Category "System Tools")
install(CODE "file(MAKE_DIRECTORY \$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/system/programs)
  file(CREATE_LINK \"/applications/system/information/Memory Monitor\"
  \$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/system/programs/memmon SYMBOLIC)")
