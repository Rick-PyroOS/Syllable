# Syllable icon and Windows icon translators (libsyllable add-ons)
foreach(t icontrans winicontrans)
  syllable_library(${t} SONAME ${t}.so SOURCES ${t}.cpp LIBS libsyllable INSTALL system/extensions/translators)
endforeach()
