# Creative EMU10K1 (SB Live!) audio driver; sources in src/ (its Makefile: MIXER=y, DEBUG=n)
syllable_driver(emu10k1
  SOURCES src/cardwo.c src/efxmgr.c src/emuadxmg.c src/hwaccess.c src/irqmgr.c src/timer.c
          src/voicemgr.c src/audio.c src/main.c src/mixer.c src/ac97_codec.c
  INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}/src
  OPTIONS -Wstrict-prototypes -Wno-trigraphs -fno-strict-aliasing -fno-common
          -mpreferred-stack-boundary=2 -ffreestanding
  INSTALL system/drivers/dev/audio)
