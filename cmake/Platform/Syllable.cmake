# Platform definition for Syllable (Pyro): ELF, shared libraries with sonames,
# no executable suffix.  Syllable's GCC spec handles the rest (its linker
# emulation is elf_i386_syllable; -kernel and -plugin are driver options).

include(Platform/UnixPaths)

set(CMAKE_SHARED_LIBRARY_PREFIX "lib")
set(CMAKE_SHARED_LIBRARY_SUFFIX ".so")
set(CMAKE_STATIC_LIBRARY_PREFIX "lib")
set(CMAKE_STATIC_LIBRARY_SUFFIX ".a")
set(CMAKE_EXECUTABLE_SUFFIX "")

foreach(lang C CXX)
  set(CMAKE_SHARED_LIBRARY_${lang}_FLAGS "-fPIC")
  set(CMAKE_SHARED_LIBRARY_CREATE_${lang}_FLAGS "-shared")
  set(CMAKE_SHARED_LIBRARY_SONAME_${lang}_FLAG "-Wl,-soname,")
  set(CMAKE_SHARED_MODULE_CREATE_${lang}_FLAGS "-shared")
endforeach()
