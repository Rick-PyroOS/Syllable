# libguitest: Google Test (the googletest port, cmake/Ports.cmake) for GUI
# code: tests run inside an os::Application, with helpers to post messages to
# windows and views, wait until they've been handled, and read the result
# under the window's lock (guitest.h).  Static; its main() is guitest_main.cpp.
#
# A test program:
#   syllable_program(my_tests SOURCES my_test.cpp LIBS libguitest)
# then run it in a Syllable session (it needs the app server).
syllable_static_library(libguitest OUTPUT_NAME guitest
  SOURCES guitest.cpp guitest_main.cpp
  OPTIONS -O2 -Wall -fexceptions
  LIBS libsyllable syllable::gtest)
target_include_directories(libguitest INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(libguitest INTERFACE libsyllable syllable::gtest)

# For programs built on Syllable: #include <guitest.h>, -lguitest -lgtest -lsyllable
install(TARGETS libguitest DESTINATION system/resources/googletest/1.8.1/lib)
install(FILES guitest.h DESTINATION system/resources/googletest/1.8.1/include)
install(CODE "file(MAKE_DIRECTORY \$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/usr/index/lib
  \$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/usr/index/include)
  file(CREATE_LINK /system/resources/googletest/1.8.1/lib/libguitest.a
  \$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/usr/index/lib/libguitest.a SYMBOLIC)
  file(CREATE_LINK /system/resources/googletest/1.8.1/include/guitest.h
  \$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/usr/index/include/guitest.h SYMBOLIC)")

# The example (example/counter_test.cpp) is always built, so the helpers keep
# compiling; SYLLABLE_TESTS=ON (sb configure -DSYLLABLE_TESTS=ON) also puts it
# on the image, as /system/development/tests/GuiTestExample.
option(SYLLABLE_TESTS "Install the test programs in /system/development/tests" OFF)
set(_install)
if(SYLLABLE_TESTS)
  set(_install INSTALL system/development/tests)
endif()
syllable_program(guitest_example OUTPUT_NAME GuiTestExample
  SOURCES example/counter_test.cpp
  OPTIONS -O2 -Wall
  LIBS libguitest ${_install})

# Google Test's headers by -I, not -isystem: GCC 4.1.2 for Syllable takes
# -isystem headers as extern "C", which C++ headers can't be.  (sb compile
# builds this folder against the tree's SDK, where syllable::gtest is an
# imported target, and an imported target's include folders are -isystem.)
set_target_properties(libguitest guitest_example PROPERTIES NO_SYSTEM_FROM_IMPORTED ON)
