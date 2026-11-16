# CodeView: syntax-highlighting text view (libcodeview.so.2), used by Sourcery
syllable_library(codeview SONAME libcodeview.so.2 LINKS libcodeview.so
  SOURCES inneredit.cpp codeview.cpp format_c.cpp format_java.cpp format_perl.cpp format_html.cpp format_ruby.cpp
  OPTIONS -Wall LIBS libsyllable INSTALL system/libraries)
# users include <codeview/codeview.h>
set(_view ${CMAKE_CURRENT_BINARY_DIR}/include)
file(MAKE_DIRECTORY ${_view})
file(CREATE_LINK ${CMAKE_CURRENT_SOURCE_DIR} ${_view}/codeview SYMBOLIC)
target_include_directories(codeview INTERFACE ${_view})
install(FILES codeview.h format.h format_c.h format_java.h format_perl.h format_html.h format_ruby.h
  DESTINATION system/development/headers/codeview)
