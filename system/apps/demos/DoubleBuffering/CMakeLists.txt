# DoubleBuffering: program
# Converted from its Makefile (Neovim: Convert to CMakeLists.txt). Check it, then
# add_subdirectory() it from the CMakeLists.txt above if that doesn't yet.

syllable_program(doublebuffering OUTPUT_NAME DoubleBuffering SOURCES main.cpp win.cpp
  OPTIONS -Wall LIBS libsyllable INSTALL applications/Demos)
syllable_resources(doublebuffering FILES icon48x48.png)
syllable_attribute(applications/Demos/DoubleBuffering os::Category Demos)

