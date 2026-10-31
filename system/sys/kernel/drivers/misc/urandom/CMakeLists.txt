# /dev/urandom: never blocks.  Its own copy of the generator in ../random/rng.c
# (separate pool and key from /dev/random).  Installed at the top of
# system/drivers/dev, which devfs scans on any /dev lookup.
syllable_driver(urandom SOURCES urandom.c ../random/rng.c
  INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}/../random LIBS kcrypto-kernel INSTALL system/drivers/dev)
