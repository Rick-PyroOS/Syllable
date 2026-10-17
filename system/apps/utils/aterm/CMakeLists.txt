# Terminal, installed as /applications/system/Terminal (and system/programs/aterm)
syllable_program(aterm OUTPUT_NAME Terminal SOURCES main.cpp tview.cpp settings.cpp swindow.cpp
  OPTIONS -fexceptions -ffast-math LIBS libsyllable INSTALL applications/system)
syllable_resources(aterm FILES icon48x48.png)
syllable_attribute(applications/system/Terminal os::Category "System Tools")
install(CODE "file(CREATE_LINK /applications/system/Terminal
  \$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/system/programs/aterm SYMBOLIC)")
