/* Definitions for Intel x86 running Syllable.
   Copyright (C) 1998-2019 Free Software Foundation, Inc.

This file is part of GCC.

GCC is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 3, or (at your option)
any later version.

GCC is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GCC; see the file COPYING3.  If not see
<http://www.gnu.org/licenses/>.  */

#define TARGET_OS_CPP_BUILTINS()				\
  do								\
    {								\
      builtin_define ("__ELF__");				\
      builtin_define_std ("unix");				\
      builtin_define ("__ATHEOS__");				\
      builtin_define ("__SYLLABLE__");				\
      builtin_define ("__syllable__");				\
      builtin_define ("__INTEL__");				\
      builtin_define ("_X86_");					\
      builtin_assert ("system=posix");				\
      builtin_assert ("system=unix");				\
    }								\
  while (0)

/* The SVR4 ABI for the i386 says that records and unions are returned
   in memory.  */
#undef DEFAULT_PCC_STRUCT_RETURN
#define DEFAULT_PCC_STRUCT_RETURN 1

#undef ASM_COMMENT_START
#define ASM_COMMENT_START "#"

#undef ASM_APP_ON
#define ASM_APP_ON "#APP\n"

#undef ASM_APP_OFF
#define ASM_APP_OFF "#NO_APP\n"

/* Output at beginning of assembler file.
   The .file command should always begin the output.  */
#define TARGET_ASM_FILE_START_FILE_DIRECTIVE true

#undef SIZE_TYPE
#define SIZE_TYPE "unsigned int"

#undef PTRDIFF_TYPE
#define PTRDIFF_TYPE "int"

#undef WCHAR_TYPE
#define WCHAR_TYPE "long int"

#undef WCHAR_TYPE_SIZE
#define WCHAR_TYPE_SIZE BITS_PER_WORD

#undef CPP_SPEC
#define CPP_SPEC \
  "%{posix:-D_POSIX_SOURCE} -D_REENTRANT %{kernel:-D__KERNEL__}"

/* Code is compiled position-independent by default, because the
   Syllable executable format expects it.  */
#undef CC1_SPEC
#define CC1_SPEC \
  "%(cc1_cpu) %{profile:-p} \
   %{!fno-pic:%{!fno-PIC:%{!fpic:%{!fPIC:%{!fpie:%{!fPIE:-fpic}}}}}}"

/* The GNU C++ standard library requires that these macros be defined.  */
#undef CPLUSPLUS_CPP_SPEC
#define CPLUSPLUS_CPP_SPEC "-D_GNU_SOURCE %(cpp)"

/* Provide a STARTFILE_SPEC appropriate for Syllable.  Here we add
   the magical crtbegin.o file (see crtstuff.c) which provides part
   of the support for getting C++ file-scope static objects
   constructed before entering `main'.  Everything on Syllable is
   linked position-independent, so the PIC flavour crtbeginS.o is
   used for both shared and non-shared links.  */
#undef	STARTFILE_SPEC
#define STARTFILE_SPEC \
  "%{!kernel: \
   %{!shared: \
     %{pg:gcrt1.o%s} %{!pg:%{p:gcrt1.o%s} \
      %{!p:%{profile:gcrt1.o%s} \
       %{!profile:crt1.o%s}}}} \
        crti.o%s %{!shared:crtbeginS.o%s} %{shared:crtbeginS.o%s}}"

/* Provide an ENDFILE_SPEC appropriate for Syllable.  Here we tack on
   the magical crtend.o file (see crtstuff.c) which provides part of
   the support for getting C++ file-scope static objects constructed
   before entering `main', followed by a normal Syllable "finalizer"
   file, `crtn.o'.  */
#undef	ENDFILE_SPEC
#define ENDFILE_SPEC \
  "%{!kernel:%{!shared:crtendS.o%s} %{shared:crtendS.o%s} crtn.o%s}"

#undef	LIB_SPEC
#define LIB_SPEC \
  "%{kernel:-lkernel} %{!kernel:%{shared: -lc}    %{!shared: %{mieee-fp:-lieee} %{pthread:-lpthread} \
   %{profile:-lc_p -lgcc_s} %{!profile: -lc}}}"

#undef  LIBGCC_SPEC
#define LIBGCC_SPEC \
  "%{!static:	%{!kernel:-lgcc_s}	%{kernel:-lgcc_static }} %{static:-lgcc_static}"

/* Provide a LINK_SPEC appropriate for Syllable.  Here we provide
   support for the special GCC options -static, -shared and -kernel.

   On Syllable -shared is always passed due to %{!r:-shared}.  This
   will go away in future versions but is currently needed for
   backwards compatibility.  */
#undef	LINK_SPEC
#define LINK_SPEC "%{!static:--eh-frame-hdr}	-m	elf_i386_syllable	%{shared:-shared}\
	%{!shared:     %{!static: 	%{rdynamic:-export-dynamic}}\
	%{static:-static}}	%{!r:-shared}	%{!plugin: %{!shared:-Bsymbolic --no-undefined}}\
	%{kernel:-shared}"

/* Indicate that jump tables go in the text section.  This is
   necessary when compiling PIC code.  */
#undef JUMP_TABLES_IN_TEXT_SECTION
#define JUMP_TABLES_IN_TEXT_SECTION (flag_pic)

/* A C statement (sans semicolon) to output to the stdio stream
   FILE the assembler definition of uninitialized global DECL named
   NAME whose size is SIZE bytes and alignment is ALIGN bytes.
   Try to use asm_output_aligned_bss to implement this macro.  */
#define ASM_OUTPUT_ALIGNED_BSS(FILE, DECL, NAME, SIZE, ALIGN) \
  asm_output_aligned_bss (FILE, DECL, NAME, SIZE, ALIGN)

/* A C statement to output to the stdio stream FILE an assembler
   command to advance the location counter to a multiple of 1<<LOG
   bytes if it is within MAX_SKIP bytes.

   This is used to align code labels according to Intel recommendations.  */
#ifdef HAVE_GAS_MAX_SKIP_P2ALIGN
#define ASM_OUTPUT_MAX_SKIP_ALIGN(FILE,LOG,MAX_SKIP)			\
  do {									\
    if ((LOG) != 0) {							\
      if ((MAX_SKIP) == 0 || (MAX_SKIP) >= (1 << (LOG)) - 1)		\
	fprintf ((FILE), "\t.p2align %d\n", (LOG));			\
      else								\
	fprintf ((FILE), "\t.p2align %d,,%d\n", (LOG), (MAX_SKIP));	\
    }									\
  } while (0)
#endif
