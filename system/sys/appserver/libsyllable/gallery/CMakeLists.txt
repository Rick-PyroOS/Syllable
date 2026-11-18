# Widget Gallery built against libsyllable, installed as a demo.
# Installed as Demos/Libraries/Syllable/Gallery so the libos build of the
# same gallery (cairo branch: libos/gallery -> Demos/Libraries/Pyro/Gallery)
# can sit beside it for comparison.  Added from
# system/apps/demos/CMakeLists.txt.

syllable_program(gallery_libsyllable OUTPUT_NAME Gallery
  SOURCES main.cpp application.cpp common.cpp mainwindow.cpp
          views/view.cpp views/button/view.cpp views/controls/view.cpp
          views/dates/view.cpp views/label/view.cpp views/lists/view.cpp
          views/menus/view.cpp views/textview/view.cpp views/views/view.cpp
  OPTIONS -Wall -O2 LIBS libsyllable INSTALL applications/Demos/Libraries/Syllable)
syllable_resources(gallery_libsyllable FILES
  resources/icons/align_bottom.png resources/icons/align_center.png
  resources/icons/align_left.png resources/icons/align_right.png
  resources/icons/align_top.png resources/icons/icon.png
  resources/icons/splash.png resources/icons/toggle_off.png
  resources/icons/toggle_on.png)
syllable_attribute(applications/Demos/Libraries/Syllable/Gallery os::Category Demos)
