# Syllable/i386.  Every image, executables included, is an ELF shared
# object that the kernel loads at 0x80000000 (GCC always passes -shared).
source_sh ${srcdir}/emulparams/plt_unwind.sh
source_sh ${srcdir}/emulparams/extern_protected_data.sh
source_sh ${srcdir}/emulparams/dynamic_undefined_weak.sh
source_sh ${srcdir}/emulparams/call_nop.sh
SCRIPT_NAME=elf
OUTPUT_FORMAT="elf32-i386"
NO_RELA_RELOCS=yes
MAXPAGESIZE=0x1000
COMMONPAGESIZE=0x1000
TEXT_START_ADDR=0x80000000
SHLIB_TEXT_START_ADDR=0x80000000
NONPAGED_TEXT_START_ADDR=0x80000000
ARCH=i386
MACHINE=
NOP=0x90909090
TEMPLATE_NAME=elf
EXTRA_EM_FILE="elf-x86"
GENERATE_SHLIB_SCRIPT=yes
NO_SMALL_DATA=yes
# The kernel loader runs DT_INIT only, never DT_INIT_ARRAY: keep .ctors/.dtors
# as they are instead of folding them into .init_array/.fini_array
# (overrides ld's --enable-initfini-array default for this emulation).
ENABLE_INITFINI_ARRAY=no
