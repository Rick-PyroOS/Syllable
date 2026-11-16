# /dev/random: the generator in rng.c (BLAKE2s pool and ChaCha20 from
# libkcrypto, seeded from RDSEED/RDRAND and CPU timestamp jitter), waiting
# until seeded from 256 credited bits.  /dev/urandom is the separate urandom
# driver (../urandom), which builds its own copy of rng.c.  Installed at the
# top of system/drivers/dev, which devfs scans on any /dev lookup.
syllable_driver(random SOURCES random.c rng.c LIBS kcrypto-kernel INSTALL system/drivers/dev)
