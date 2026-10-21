# libdl: dlopen() and friends on top of Syllable's image loader
syllable_library(libdl SONAME libdl.so.3 SOURCES dlfcn.c LINKS libdl.so INSTALL system/libraries)
install(FILES dlfcn.h DESTINATION system/development/headers)
# Ports that link static libraries using dlopen() (OpenSSL's libcrypto) find
# it in the staging sysroot as -ldl
add_custom_command(TARGET libdl POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E create_symlink $<TARGET_FILE:libdl> ${SYLLABLE_STAGING}/lib/libdl.so)
