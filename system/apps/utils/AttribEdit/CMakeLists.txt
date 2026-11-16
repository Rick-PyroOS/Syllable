# AttribEdit: attribute editor (the Makefile copied it to /bin; it has no icons)
syllable_program(attribedit OUTPUT_NAME AttribEdit SOURCES AttribEdit.cpp etextview.cpp
  OPTIONS -O2 -Wall LIBS libsyllable INSTALL system/programs)
