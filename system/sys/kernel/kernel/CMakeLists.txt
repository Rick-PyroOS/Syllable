# The Syllable kernel: kernel.so, libkernel.so (its exported symbols, which
# -kernel links drivers against) and the eth_if network interface driver.
# Flags, object order and the link follow the original Makefile.

set(KERNEL_FLAGS
  -Wall -O2 -std=gnu99 -fno-optimize-sibling-calls -Wstrict-prototypes
  -D__KERNEL__ -D__BUILD_KERNEL__ -ffreestanding -no-fPIC
  -mpreferred-stack-boundary=2 -fno-strict-aliasing)

# Generated sources go in gen/; smp.c includes "objs/smp_entry.hex"
set(GEN ${CMAKE_CURRENT_BINARY_DIR}/gen)
file(MAKE_DIRECTORY ${GEN}/objs)

set(KERNEL_INCLUDES
  ${SYLLABLE_KERNEL_HEADER_VIEW} ${CMAKE_CURRENT_SOURCE_DIR} ${GEN})

# ---- SMP trampoline: nasm, then bintohex (a host tool) into C data --------------

find_program(NASM nasm REQUIRED)
add_custom_command(OUTPUT ${GEN}/objs/smp_entry.bin
  COMMAND ${NASM} ${CMAKE_CURRENT_SOURCE_DIR}/smp_entry.asm -o ${GEN}/objs/smp_entry.bin
  DEPENDS smp_entry.asm)
add_custom_command(OUTPUT ${GEN}/objs/smp_entry.hex
  COMMAND ${SYLLABLE_BINTOHEX} ${GEN}/objs/smp_entry.bin ${GEN}/objs/smp_entry.hex
  DEPENDS ${GEN}/objs/smp_entry.bin hosttools)

# ---- Kernel objects, in the Makefile's link order -----------------------------

set(KERNEL_SOURCES
  cpuid.c descriptors.c kernel.c dbconsole.c ksyms.c init.c fork.c
  debug.c timer.c process.c threads.c sema.c msgport.c memory.c strace.c
  smp.c apic.c udelay.c fault.c exec.c pci.c pgroup.c kmalloc.c swap.c dma.c
  elf.c timecalc.c virt86.c irq.c array.c schedule.c psignals.c mman.c
  areas.c vmalloc.c pagefault.c bcache.c bcalloc.c vesa_gfx.c random.c
  devices.c config.c ptrace.c save_i387.c resource.c
  lib/ct_flags.c lib/ct_upper.c lib/ct_lower.c lib/vsprintf.c lib/string.c
  lib/strstr.c lib/qsort.c lib/gcc34_memcpy.c lib/strlcpy.c lib/strlcat.c
  vfs/vfs.c vfs/inode.c vfs/nodemonitor.c vfs/attributes.c vfs/flock.c
  vfs/root.c vfs/dev.c vfs/fifo.c vfs/pty.c vfs/aio.c
  net/core.c net/interface.c net/route.c net/ip.c net/udp.c net/tcp.c
  net/tcp_input.c net/tcp_output.c net/socket.c net/packets.c net/raw.c
  nls/nls_conv.c nls/nls_iso8859-1.c nls/nls_iso8859-2.c nls/nls_iso8859-3.c
  nls/nls_ascii.c nls/nls_utf16.c nls/nls_cp1250.c nls/nls_cp1251.c
  nls/nls_cp1255.c nls/nls_koi8-u.c nls/nls_koi8-r.c nls/nls_cp437.c
  nls/nls_cp850.c nls/nls_cp852.c)
add_library(kernel_objs OBJECT ${KERNEL_SOURCES})
target_compile_options(kernel_objs PRIVATE ${KERNEL_FLAGS})
target_include_directories(kernel_objs PRIVATE ${KERNEL_INCLUDES})
target_include_directories(kernel_objs SYSTEM PRIVATE ${SYLLABLE_HEADERS})
set_source_files_properties(smp.c PROPERTIES OBJECT_DEPENDS ${GEN}/objs/smp_entry.hex)

# intel.s and syscall.s: preprocessed assembler, with only the include path
# (the Makefile's AFLAGS), not the kernel's C flags
set(ASM_INCLUDES)
foreach(dir ${KERNEL_INCLUDES})
  list(APPEND ASM_INCLUDES -I${dir})
endforeach()
set(ASM_OBJS)
foreach(asm intel syscall)
  set(obj ${CMAKE_CURRENT_BINARY_DIR}/${asm}.o)
  add_custom_command(OUTPUT ${obj}
    COMMAND ${CMAKE_C_COMPILER} -c ${ASM_INCLUDES} -isystem ${SYLLABLE_HEADERS}
            -x assembler-with-cpp ${CMAKE_CURRENT_SOURCE_DIR}/${asm}.s -o ${obj}
    DEPENDS ${asm}.s IMPLICIT_DEPENDS C ${CMAKE_CURRENT_SOURCE_DIR}/${asm}.s
    COMMENT "Assembling ${asm}.s")
  list(APPEND ASM_OBJS ${obj})
endforeach()
list(GET ASM_OBJS 0 INTEL_O)
list(GET ASM_OBJS 1 SYSCALL_O)

# ---- kernel.so ---------------------------------------------------------------------
# version.c (build date and time) is compiled as part of the link, so it is
# fresh whenever the kernel is relinked, as the Makefile's "touch version.c" did.

set(KERNEL_SO ${CMAKE_CURRENT_BINARY_DIR}/kernel.so)
add_custom_command(OUTPUT ${KERNEL_SO}
  COMMAND ${PROJECT_SOURCE_DIR}/cmake/scripts/link-kernel.sh
          ${CMAKE_C_COMPILER} ${CMAKE_LINKER} ${CMAKE_CURRENT_SOURCE_DIR}/kernel.lnk
          ${CMAKE_CURRENT_SOURCE_DIR}/version.c ${KERNEL_SO} ${INTEL_O} ${SYSCALL_O}
          -- ${KERNEL_FLAGS} ${ASM_INCLUDES} -isystem ${SYLLABLE_HEADERS}
          -- $<TARGET_OBJECTS:kernel_objs>
  DEPENDS kernel_objs $<TARGET_OBJECTS:kernel_objs> ${ASM_OBJS} version.c kernel.lnk
          ${PROJECT_SOURCE_DIR}/cmake/scripts/link-kernel.sh
  COMMAND_EXPAND_LISTS VERBATIM
  COMMENT "Linking kernel.so")
add_custom_target(kernel ALL DEPENDS ${KERNEL_SO})
install(PROGRAMS ${KERNEL_SO} DESTINATION system)
install(CODE "execute_process(COMMAND ${CMAKE_STRIP} --strip-all \$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/system/kernel.so)")

# ---- libkernel.so --------------------------------------------------------------------

set(EXPORTS ${CMAKE_CURRENT_BINARY_DIR}/kernel.exports.c)
add_custom_command(OUTPUT ${EXPORTS}
  COMMAND ${CMAKE_COMMAND} -DKSYMS=${CMAKE_CURRENT_SOURCE_DIR}/ksyms.c -DOUT=${EXPORTS}
          -P ${PROJECT_SOURCE_DIR}/cmake/scripts/kernel-exports.cmake
  DEPENDS ksyms.c ${PROJECT_SOURCE_DIR}/cmake/scripts/kernel-exports.cmake)

set(LIBKERNEL ${CMAKE_CURRENT_BINARY_DIR}/libkernel.so)
add_custom_command(OUTPUT ${LIBKERNEL}
  COMMAND ${CMAKE_C_COMPILER} -nostdlib -w ${EXPORTS} -o ${LIBKERNEL}
  DEPENDS ${EXPORTS}
  COMMENT "Linking libkernel.so")
add_custom_target(libkernel ALL DEPENDS ${LIBKERNEL})
install(PROGRAMS ${LIBKERNEL} DESTINATION system/libraries)

# ---- eth_if: compiled with the kernel's flags, linked like a driver --------------------

add_executable(eth_if net/eth_interface.c net/arp.c)
target_compile_options(eth_if PRIVATE ${KERNEL_FLAGS})
target_include_directories(eth_if PRIVATE ${KERNEL_INCLUDES})
target_include_directories(eth_if SYSTEM PRIVATE ${SYLLABLE_HEADERS})
target_link_options(eth_if PRIVATE -kernel -L${CMAKE_CURRENT_BINARY_DIR})
add_dependencies(eth_if libkernel)
install(TARGETS eth_if DESTINATION system/drivers/net/if)
