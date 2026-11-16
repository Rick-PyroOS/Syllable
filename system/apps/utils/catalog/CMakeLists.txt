# catalogtool and catcomp, the native catalog tools (the host ones are in tools/host)
syllable_program(catalogtool SOURCES catalogtool.cpp ccatalog.cpp LIBS libsyllable INSTALL system/programs)
syllable_program(catcomp_native OUTPUT_NAME catcomp SOURCES catcomp.cpp ccatalog.cpp
  LIBS libsyllable INSTALL system/programs)
syllable_catalogs(catcomp_native NAME catcomp DIR resources LANGS da de el fr hu nl pt-br ru sv EMBED)
