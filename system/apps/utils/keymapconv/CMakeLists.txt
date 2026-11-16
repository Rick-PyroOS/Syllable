# keymapconv: keymap converter (the native one; the host one is in tools/host)
syllable_program(keymapconv_native OUTPUT_NAME keymapconv SOURCES keymapconv.cpp
  LIBS libsyllable INSTALL system/programs)
