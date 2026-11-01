# Assistant (the makefile's sources; it #includes its providers)
syllable_program(assistant SOURCES main.cpp common.cpp application.cpp settings.cpp provider_thread.cpp mainwindow.cpp top_view.cpp syll_gui/listview.cpp gui/row.cpp syll_gui/textview.cpp 
  OPTIONS -O2 -Wall -fexceptions LIBS libsyllable INSTALL system/programs)
file(GLOB _png RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}/images ${CMAKE_CURRENT_SOURCE_DIR}/images/*.png)
syllable_resources(assistant FILES ${_png})
syllable_attribute(system/programs/assistant os::Category "System Tools")

add_subdirectory(providers)
