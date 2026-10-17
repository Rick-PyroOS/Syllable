# libdl: dlopen() and friends on top of Syllable's image loader
syllable_library(libdl SONAME libdl.so.3 SOURCES dlfcn.c LINKS libdl.so INSTALL system/libraries)
install(FILES dlfcn.h DESTINATION system/development/headers)
