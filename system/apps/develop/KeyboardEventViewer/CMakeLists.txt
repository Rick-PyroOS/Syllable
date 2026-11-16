# Keyboard event viewer
syllable_program(keyboard_event_viewer OUTPUT_NAME KeyboardEventViewer
  SOURCES main.cpp application.cpp mainwindow.cpp view.cpp common.cpp
  OPTIONS -O2 -Wall LIBS libsyllable INSTALL applications/sIDE)
syllable_resources(keyboard_event_viewer FILES icon48x48.png)
syllable_attribute(applications/sIDE/KeyboardEventViewer os::Category Development)
