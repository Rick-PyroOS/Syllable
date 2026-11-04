# Rounded: demo of a borderless window with rounded corners
syllable_program(rounded OUTPUT_NAME Rounded SOURCES main.cpp
  OPTIONS -O2 -Wall LIBS libsyllable INSTALL applications/Demos)
syllable_resources(rounded FILES icon48x48.png)
syllable_attribute(applications/Demos/Rounded os::Category Demos)
