# System tools.  init is the first userland process (the kernel runs
# /boot/system/programs/init); the others need libsyllable and follow it.
syllable_program(init SOURCES init.c INSTALL system/programs)
