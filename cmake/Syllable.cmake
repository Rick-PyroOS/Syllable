# Helpers for building Syllable components.  Each carries the flags the
# component's original Makefile used, so a CMakeLists.txt only lists sources,
# extra includes and where the result is installed (relative to the boot
# volume: system/drivers/dev/..., system/programs, ...).
#
#   syllable_driver(NAME SOURCES ... INSTALL dir [OUTPUT_NAME n] [INCLUDES ...]
#                   [DEFINES ...] [OPTIONS ...])
#       Kernel driver or filesystem: compiled and linked with -kernel
#       (Syllable's GCC: -D__KERNEL__, linked as a shared object against
#       libkernel.so).  Sees the kernel's header view (below).
#
#   syllable_program(NAME SOURCES ... INSTALL dir [OUTPUT_NAME n] [INCLUDES ...]
#                    [DEFINES ...] [OPTIONS ...] [LIBS ...])
#       Userland program.
#
#   syllable_library(NAME SONAME libfoo.so.N SOURCES ... INSTALL dir
#                    [LINKS name...] [INCLUDES ...] [DEFINES ...] [OPTIONS ...] [LIBS ...])
#       Shared library, built and installed under its soname (as the Makefiles
#       do), with LINKS (e.g. libfoo.so) as symlinks to it.
#
#   syllable_plugin(NAME SOURCES ... INSTALL dir [OUTPUT_NAME n] [INCLUDES ...]
#                   [DEFINES ...] [OPTIONS ...] [LIBS ...])
#       Add-on loaded by a program (appserver drivers, decorators): linked with
#       -plugin -Bsymbolic -nodefaultlibs.
#
#   syllable_catalogs(TARGET NAME name DIR dir [LANGS lang...] [EMBED] [INSTALL dir]
#                     [EXT ext] [MAIN file])
#       Compile DIR/NAME.cd (with its header DIR/NAME.h, which sources include
#       as "DIR/NAME.h") and DIR/LANG/NAME.cd with the host catcomp, in the
#       build tree.  EMBED adds them to TARGET's resources after it links;
#       INSTALL installs them (NAME.catalog, LANG/NAME.catalog).  EXT: the
#       source extension (default cd; some apps use catfish).  MAIN: the main
#       catalog's source, if it is not DIR/NAME.EXT.
#
#   syllable_resources(TARGET [FILES ...] [ADD name=file ...])
#       Replace TARGET's resources with FILES after it links (host rescopy -r),
#       then add the ADD resources under their own names (rescopy -a).
#       Call before syllable_catalogs(... EMBED), as the Makefiles do.
#
#   syllable_attribute(PATH NAME VALUE)
#       Set attribute NAME on the installed file or directory PATH (relative to the boot
#       volume) at install time (host addattrib: a user.* xattr, which afstool
#       carries into AFS).

# Userland targets see the Syllable headers through C_INCLUDE_PATH and
# CPLUS_INCLUDE_PATH, as the Makefiles did: system headers (so GCC overlooks
# what it overlooks in system headers), and, unlike -isystem, not wrapped in
# extern "C" for C++ by this target's GCC.
#
# Every target helper also takes STRIP: strip it after linking (the
# Makefiles' install -s / strip).
#
# SYLLABLE_DEBUG_INFO (sb configure -DSYLLABLE_DEBUG_INFO=ON): userland
# programs, libraries and plugins are built with debug info (DWARF 2, which
# the Mac's gdb reads; GCC 4.1.2's plain -g is stabs) and are not stripped, so
# gdb through gdbserver shows their source.  ./sb image mirrors the stage to
# out/sysroot for the Mac's gdb.
option(SYLLABLE_DEBUG_INFO "Userland built with debug info and not stripped" OFF)

# Header views.  Userland code includes the tree's headers as they are
# (system/sys/include/syllable).  Kernel code was written when <syllable/...>
# was the atheos/ headers (sys/include's own deps target did
# `ln -sf atheos syllable`); the tree now has a separate, newer syllable/ for
# userland, so kernel targets get a directory in which syllable -> atheos,
# searched first.

set(SYLLABLE_HEADERS ${PROJECT_SOURCE_DIR}/system/sys/include/syllable)
set(SYLLABLE_KERNEL_HEADER_VIEW ${CMAKE_BINARY_DIR}/kernel-headers)
file(MAKE_DIRECTORY ${SYLLABLE_KERNEL_HEADER_VIEW})
file(CREATE_LINK ${SYLLABLE_HEADERS}/atheos ${SYLLABLE_KERNEL_HEADER_VIEW}/syllable SYMBOLIC)

# Where libkernel.so (the kernel's exported symbols, linked by -kernel) is built
set(SYLLABLE_KERNEL_LIBDIR ${CMAKE_BINARY_DIR}/system/sys/kernel/kernel)

set(_SYLLABLE_ONE INSTALL OUTPUT_NAME SONAME)
set(_SYLLABLE_MULTI SOURCES INCLUDES DEFINES OPTIONS LIBS LINKS)

# Settings shared by every kind of target (A_* from the caller's parse)
macro(_syllable_common name)
  target_include_directories(${name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${A_INCLUDES})
  target_compile_definitions(${name} PRIVATE ${A_DEFINES})
  target_compile_options(${name} PRIVATE ${A_OPTIONS})
  target_link_libraries(${name} PRIVATE ${A_LIBS})
  if(A_OUTPUT_NAME)
    set_target_properties(${name} PROPERTIES OUTPUT_NAME ${A_OUTPUT_NAME})
  endif()
  if(SYLLABLE_DEBUG_INFO AND _syllable_userland)
    target_compile_options(${name} PRIVATE -gdwarf-2)
  elseif(A_STRIP)
    # stripped in the build tree (the Makefiles' install -s), so installing is
    # a plain copy that only happens when the target changed
    add_custom_command(TARGET ${name} POST_BUILD COMMAND ${CMAKE_STRIP} $<TARGET_FILE:${name}>)
  endif()
  if(A_INSTALL)
    install(TARGETS ${name} DESTINATION ${A_INSTALL})
  endif()
endmacro()

function(syllable_userland_headers target)
  set_target_properties(${target} PROPERTIES
    C_COMPILER_LAUNCHER "${CMAKE_COMMAND};-E;env;C_INCLUDE_PATH=${SYLLABLE_HEADERS}"
    CXX_COMPILER_LAUNCHER "${CMAKE_COMMAND};-E;env;CPLUS_INCLUDE_PATH=${SYLLABLE_HEADERS}")
endfunction()

function(syllable_driver name)
  cmake_parse_arguments(A "STRIP" "${_SYLLABLE_ONE}" "${_SYLLABLE_MULTI}" ${ARGN})
  add_executable(${name} ${A_SOURCES})
  _syllable_common(${name})
  target_include_directories(${name} BEFORE PRIVATE ${SYLLABLE_KERNEL_HEADER_VIEW})
  target_include_directories(${name} SYSTEM PRIVATE ${SYLLABLE_HEADERS})
  target_compile_options(${name} PRIVATE -kernel -fno-PIC)
  target_link_options(${name} PRIVATE -kernel -L${SYLLABLE_KERNEL_LIBDIR})
  add_dependencies(${name} libkernel)
endfunction()

function(syllable_program name)
  cmake_parse_arguments(A "STRIP" "${_SYLLABLE_ONE}" "${_SYLLABLE_MULTI}" ${ARGN})
  add_executable(${name} ${A_SOURCES})
  set(_syllable_userland TRUE)
  _syllable_common(${name})
  syllable_userland_headers(${name})
  target_compile_options(${name} PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-fexceptions>)
endfunction()

function(syllable_library name)
  cmake_parse_arguments(A "STRIP" "${_SYLLABLE_ONE}" "${_SYLLABLE_MULTI}" ${ARGN})
  if(NOT A_SONAME)
    message(FATAL_ERROR "syllable_library(${name}): SONAME is required")
  endif()
  add_library(${name} SHARED ${A_SOURCES})
  set(A_OUTPUT_NAME)
  set(_syllable_userland TRUE)
  _syllable_common(${name})
  syllable_userland_headers(${name})
  target_compile_options(${name} PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-fexceptions>)
  set_target_properties(${name} PROPERTIES PREFIX "" SUFFIX "" OUTPUT_NAME ${A_SONAME} NO_SONAME TRUE)
  target_link_options(${name} PRIVATE "-Wl,-soname=${A_SONAME}")
  foreach(link ${A_LINKS})
    install(CODE "file(CREATE_LINK ${A_SONAME} \$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/${A_INSTALL}/${link} SYMBOLIC)")
  endforeach()
endfunction()

function(syllable_plugin name)
  cmake_parse_arguments(A "STRIP" "${_SYLLABLE_ONE}" "${_SYLLABLE_MULTI}" ${ARGN})
  add_executable(${name} ${A_SOURCES})
  set(_syllable_userland TRUE)
  _syllable_common(${name})
  syllable_userland_headers(${name})
  target_link_options(${name} PRIVATE -plugin -Xlinker -Bsymbolic -nodefaultlibs)
endfunction()

# rescopy embeds resources with objcopy: the target's
set(SYLLABLE_TOOL_PATH ${CMAKE_BINARY_DIR}/tool-path)
file(MAKE_DIRECTORY ${SYLLABLE_TOOL_PATH})
file(CREATE_LINK ${CMAKE_OBJCOPY} ${SYLLABLE_TOOL_PATH}/objcopy SYMBOLIC)
set(SYLLABLE_RESCOPY_CMD ${CMAKE_COMMAND} -E env PATH=${SYLLABLE_TOOL_PATH}:$ENV{PATH} ${SYLLABLE_RESCOPY})

function(syllable_resources target)
  cmake_parse_arguments(A "" "" "FILES;ADD" ${ARGN})
  set(files)
  foreach(f ${A_FILES})
    list(APPEND files ${CMAKE_CURRENT_SOURCE_DIR}/${f})
  endforeach()
  set(added)
  foreach(spec ${A_ADD})
    string(REGEX REPLACE "^([^=]*)=(.*)$" "\\1=${CMAKE_CURRENT_SOURCE_DIR}/\\2" spec "${spec}")
    list(APPEND added ${spec})
  endforeach()
  # -a only adds to an existing resource section, so the first step is -r
  set(mode -r)
  if(files)
    add_custom_command(TARGET ${target} POST_BUILD
      COMMAND ${SYLLABLE_RESCOPY_CMD} -r $<TARGET_FILE:${target}> ${files}
      COMMENT "Adding resources to ${target}" VERBATIM)
    set(mode -a)
  endif()
  if(added)
    add_custom_command(TARGET ${target} POST_BUILD
      COMMAND ${SYLLABLE_RESCOPY_CMD} ${mode} $<TARGET_FILE:${target}> ${added}
      COMMENT "Adding named resources to ${target}" VERBATIM)
  endif()
  set_property(TARGET ${target} PROPERTY SYLLABLE_HAS_RESOURCES TRUE)
  add_dependencies(${target} hosttools)
endfunction()

function(syllable_catalogs target)
  cmake_parse_arguments(A "EMBED" "NAME;DIR;INSTALL;EXT;MAIN" "LANGS" ${ARGN})
  if(NOT A_EXT)
    set(A_EXT cd)
  endif()
  set(src ${CMAKE_CURRENT_SOURCE_DIR}/${A_DIR})
  set(bin ${CMAKE_CURRENT_BINARY_DIR}/${A_DIR})
  set(main ${src}/${A_NAME}.${A_EXT})
  if(A_MAIN)
    set(main ${CMAKE_CURRENT_SOURCE_DIR}/${A_MAIN})
  endif()
  set(outputs ${bin}/${A_NAME}.catalog ${bin}/${A_NAME}.h)
  add_custom_command(OUTPUT ${outputs}
    COMMAND ${CMAKE_COMMAND} -E copy ${main} ${bin}/${A_NAME}.${A_EXT}
    COMMAND ${SYLLABLE_CATCOMP} -c ${A_NAME}.${A_EXT}
    WORKING_DIRECTORY ${bin}
    DEPENDS ${main} hosttools
    COMMENT "Compiling ${A_DIR}/${A_NAME}.${A_EXT}")
  foreach(lang ${A_LANGS})
    add_custom_command(OUTPUT ${bin}/${lang}/${A_NAME}.catalog
      COMMAND ${CMAKE_COMMAND} -E make_directory ${bin}/${lang}
      COMMAND ${CMAKE_COMMAND} -E copy ${src}/${lang}/${A_NAME}.${A_EXT} ${bin}/${lang}/${A_NAME}.${A_EXT}
      COMMAND ${SYLLABLE_CATCOMP} -t ${A_NAME}.${A_EXT}
      WORKING_DIRECTORY ${bin}/${lang}
      DEPENDS ${src}/${lang}/${A_NAME}.${A_EXT} hosttools
      COMMENT "Compiling ${A_DIR}/${lang}/${A_NAME}.${A_EXT}")
    list(APPEND outputs ${bin}/${lang}/${A_NAME}.catalog)
  endforeach()
  add_custom_target(${target}_catalogs DEPENDS ${outputs})
  add_dependencies(${target} ${target}_catalogs)
  target_include_directories(${target} PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
  if(A_EMBED)
    # -r if syllable_resources() gave the target no resource section yet
    get_property(has TARGET ${target} PROPERTY SYLLABLE_HAS_RESOURCES)
    set(mode -a)
    if(NOT has)
      set(mode -r)
    endif()
    set(args ${mode} $<TARGET_FILE:${target}> ${bin}/${A_NAME}.catalog)
    foreach(lang ${A_LANGS})
      list(APPEND args ${lang}/${A_NAME}.catalog=${bin}/${lang}/${A_NAME}.catalog)
    endforeach()
    add_custom_command(TARGET ${target} POST_BUILD
      COMMAND ${SYLLABLE_RESCOPY_CMD} ${args}
      COMMENT "Adding catalogs to ${target}" VERBATIM)
  endif()
  if(A_INSTALL)
    install(FILES ${bin}/${A_NAME}.catalog DESTINATION ${A_INSTALL})
    foreach(lang ${A_LANGS})
      install(FILES ${bin}/${lang}/${A_NAME}.catalog DESTINATION ${A_INSTALL}/${lang})
    endforeach()
  endif()
endfunction()

function(syllable_attribute path name value)
  install(CODE "execute_process(COMMAND ${PROJECT_SOURCE_DIR}/cmake/scripts/set-attribute.sh ${SYLLABLE_ADDATTRIB}
    \"\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/${path}\" \"${name}\" \"${value}\" COMMAND_ERROR_IS_FATAL ANY)")
endfunction()
