# libpthread: POSIX threads on Syllable's threads and semaphores
syllable_library(libpthread SONAME libpthread.so.2
  SOURCES attr.c cond.c mutex.c pthread.c rwlock.c bits.c
  INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}/inc
  LINKS libpthread.so INSTALL system/libraries)
