# libsyllable's unit tests (Google Test, the googletest port): os::JSON
# (with JSONPath and JSON Schema) and os::XML (with XPath and validation).
# They need no app server: run them from a Terminal, all or some,
#
#   /system/development/tests/LibsyllableTests
#   /system/development/tests/LibsyllableTests --gtest_filter='XML*'
#
# Always built, so they keep compiling; SYLLABLE_TESTS=ON (sb configure
# -DSYLLABLE_TESTS=ON) also puts them on the image.
option(SYLLABLE_TESTS "Install the test programs in /system/development/tests" OFF)
set(_install)
if(SYLLABLE_TESTS)
  set(_install INSTALL system/development/tests)
endif()
syllable_program(libsyllable_tests OUTPUT_NAME LibsyllableTests
  SOURCES json_test.cpp jsonpath_test.cpp jsonschema_test.cpp
          xml_test.cpp xmlschema_test.cpp
  OPTIONS -O2 -Wall
  LIBS libsyllable syllable::gtest_main ${_install})

# Google Test's headers by -I, not -isystem (see system/libs/guitest)
set_target_properties(libsyllable_tests PROPERTIES NO_SYSTEM_FROM_IMPORTED ON)
