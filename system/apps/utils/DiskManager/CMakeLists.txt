# Disk Manager (applications/system/Disk Manager)
syllable_program(diskmanager OUTPUT_NAME DiskManager SOURCES main.cpp diskview.cpp partitionview.cpp
  LIBS libsyllable)
file(GLOB _res RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/icons/*.png)
syllable_resources(diskmanager FILES ${_res})
syllable_catalogs(diskmanager NAME DiskManager DIR resources LANGS da de es fr hu nl pl sv EMBED)
install(PROGRAMS $<TARGET_FILE:diskmanager> DESTINATION applications/system RENAME "Disk Manager")
syllable_attribute("applications/system/Disk Manager" os::Category "System Tools")
