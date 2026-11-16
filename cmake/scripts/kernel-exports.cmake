# kernel.exports.c from ksyms.c: an empty function per KSYMBOL( name ) line,
# which linked into libkernel.so gives drivers the kernel's exported symbols
# (the kernel Makefile's grep | sed).
#   cmake -DKSYMS=ksyms.c -DOUT=kernel.exports.c -P kernel-exports.cmake
file(STRINGS ${KSYMS} lines)
set(out "")
foreach(line IN LISTS lines)
  if(line MATCHES "^[ \t]+KSYMBOL\\( *([^ )]*) *\\)")
    string(APPEND out "void ${CMAKE_MATCH_1}() {}\n")
  endif()
endforeach()
file(WRITE ${OUT}.tmp "${out}")
file(COPY_FILE ${OUT}.tmp ${OUT} ONLY_IF_DIFFERENT)
file(REMOVE ${OUT}.tmp)
