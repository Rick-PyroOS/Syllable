# Icni: puzzle game (Applications/Icni).  Its Makefile links the Pyro
# library (-lpyro); built against libsyllable here.
syllable_program(icni OUTPUT_NAME Icni SOURCES main.cpp win.cpp imageview.cpp node.cpp gameview.cpp
  LIBS libsyllable INSTALL applications/Icni)
file(GLOB _res RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/resources/*.png)
syllable_resources(icni FILES ${_res})
syllable_attribute(applications/Icni/Icni os::Category Games)
