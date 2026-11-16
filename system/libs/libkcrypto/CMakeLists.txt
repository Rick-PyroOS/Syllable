# libkcrypto: BLAKE2s (from Linux lib/crypto, MIT), ChaCha20 and the
# entropy pool / fast-key-erasure generator behind /dev/random and
# /dev/urandom.  The same sources build twice: kcrypto-kernel for drivers and
# the kernel, kcrypto for userland.  test/Makefile builds the test on the host.
set(KCRYPTO_SOURCES blake2s.c blake2s-generic.c chacha20.c kcrypto.c)

syllable_static_library(kcrypto-kernel KERNEL SOURCES ${KCRYPTO_SOURCES}
  OPTIONS -O2 -Wall -fno-strict-aliasing)
syllable_static_library(kcrypto SOURCES ${KCRYPTO_SOURCES}
  OPTIONS -O2 -Wall -fno-strict-aliasing)
target_include_directories(kcrypto-kernel INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(kcrypto INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

# Tests, run on Syllable: the known-answer test (PYRO-KCRYPTO lines) and the
# /dev/random, /dev/urandom smoke test (PYRO-RANDOM lines)
syllable_program(kcrypto-test SOURCES test/kcrypto-test.c LIBS kcrypto
  INSTALL system/development/bin)
syllable_program(random-test SOURCES test/random-test.c
  INSTALL system/development/bin)
