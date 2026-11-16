# slbmgr: program
# Converted from its Makefile (Neovim: Convert to CMakeLists.txt). Check it, then
# add_subdirectory() it from the CMakeLists.txt above if that doesn't yet.
# TODO(make): command not converted: install: ln -sf "/applications/system/information/"System Information"" /system/programs/SlbMgr

syllable_program(system_information SOURCES main.cpp sysinfopanel.cpp drivespanel.cpp
  processpanel.cpp memview.cpp cpuview.cpp performancepanel.cpp devicespanel.cpp LIBS libsyllable
  STRIP INSTALL applications/system/information)
set_target_properties(system_information PROPERTIES OUTPUT_NAME "System Information")
file(GLOB _files RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/icons/*.png)
syllable_resources(system_information FILES ${_files})
syllable_catalogs(system_information NAME SysInfo DIR resources LANGS da de es fr hu nl ru sv EMBED)
syllable_attribute("applications/system/information/System Information" os::Category "System Tools")
install(CODE "file(MAKE_DIRECTORY \$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/system/programs)
  file(CREATE_LINK /applications/DeviceInformation
  \$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/system/programs/slbmgr SYMBOLIC)")
