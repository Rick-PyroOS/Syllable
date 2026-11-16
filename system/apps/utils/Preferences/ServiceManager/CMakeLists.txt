# Makefile for ServiceManager
# Converted from its Makefile (Neovim: Convert to CMakeLists.txt). Check it, then
# add_subdirectory() it from the CMakeLists.txt above if that doesn't yet.

syllable_program(servicemanager OUTPUT_NAME ServiceManager SOURCES main.cpp application.cpp
  mainwindowLayout.cpp Executer/commonfuncs.cpp Executer/execute.cpp OPTIONS -Wall -O2
  LIBS libsyllable)
syllable_resources(servicemanager FILES icon48x48.png)
syllable_attribute(applications/Noros/ServiceManager os::Category "System Tools")
install(PROGRAMS $<TARGET_FILE:servicemanager> DESTINATION applications/preferences RENAME "Service Manager")

