# The developer documentation (the Makefiles' doc and install-doc targets):
# Doxygen's HTML of libsyllable, libmedia and the kernel's driver API and
# system calls, in /documentation/developer, linked from the desktop
# (system/image/home/*/Desktop/Developer-Documentation.html).
#
# Made with a Doxygen built for the build host (the image has none).  The
# Doxyfiles are the tree's own; only where they read from and write to is
# changed: the headers from the source tree, not /system/development/headers.

set(_doxygen ${CMAKE_BINARY_DIR}/host-doxygen/bin/doxygen)
ExternalProject_Add(doxygen_host
  PREFIX ${CMAKE_BINARY_DIR}/ports/doxygen_host
  URL https://github.com/doxygen/doxygen/releases/download/Release_1_9_8/doxygen-1.9.8.src.tar.gz
  URL_HASH SHA256=05e3d228e8384b5f3af9c8fd6246d22804acb731a3a24ce285c8986ed7e14f62
  DOWNLOAD_DIR ${CMAKE_BINARY_DIR}/downloads
  DOWNLOAD_EXTRACT_TIMESTAMP ON
  INSTALL_DIR ${CMAKE_BINARY_DIR}/host-doxygen
  # the build host's compilers, not the toolchain file's; bison from the host tools
  CONFIGURE_COMMAND ${CMAKE_COMMAND} -E env --unset=TARGET PATH=/opt/syllable/host/bin:$ENV{PATH}
                    ${CMAKE_COMMAND} -S <SOURCE_DIR> -B <BINARY_DIR> -G Ninja
                    -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=/usr/bin/gcc
                    -DCMAKE_CXX_COMPILER=/usr/bin/g++ -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>
  BUILD_COMMAND ${CMAKE_COMMAND} -E env --unset=TARGET PATH=/opt/syllable/host/bin:$ENV{PATH}
                ${CMAKE_COMMAND} --build <BINARY_DIR> -j ${SYLLABLE_JOBS}
  INSTALL_COMMAND ${CMAKE_COMMAND} --install <BINARY_DIR>
  BUILD_BYPRODUCTS ${_doxygen}
  LOG_CONFIGURE ON LOG_BUILD ON LOG_INSTALL ON LOG_OUTPUT_ON_FAILURE ON)

set(_docs ${CMAKE_BINARY_DIR}/documentation)
set(_hdr ${PROJECT_SOURCE_DIR}/system/sys/include/syllable)
set(_doc_stamps "")

# _syllable_doc(NAME DIR DOXYFILE "TAG=value" ...): DIR's DOXYFILE, run in DIR,
# into ${_docs}/NAME/html, with the given tags overridden.  Plain HTML for
# Webster: the menus, sections, search and tree view of today's Doxygen are
# JavaScript it can't run.
function(_syllable_doc name dir doxyfile)
  set(out ${_docs}/${name})
  set(over "OUTPUT_DIRECTORY=${out}\nHTML_OUTPUT=html\nGENERATE_LATEX=NO\nQUIET=YES\nWARNINGS=NO\nWARN_LOGFILE=${out}.log")
  string(APPEND over "\nHTML_DYNAMIC_MENUS=NO\nHTML_DYNAMIC_SECTIONS=NO\nSEARCHENGINE=NO\nGENERATE_TREEVIEW=NO\nHTML_CODE_FOLDING=NO")
  foreach(tag ${ARGN})
    string(APPEND over "\n${tag}")
  endforeach()
  # rewritten only when it changes, so a change rebuilds the documentation
  file(CONFIGURE OUTPUT ${CMAKE_BINARY_DIR}/documentation-${name}.over CONTENT "${over}\n" @ONLY)
  file(GLOB_RECURSE srcs CONFIGURE_DEPENDS ${dir}/*.c ${dir}/*.cpp ${dir}/*.h ${dir}/*.ddoc)
  add_custom_command(OUTPUT ${out}.stamp
    COMMAND ${CMAKE_COMMAND} -E rm -rf ${out}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${out}
    COMMAND sh -c "cat \"$0\" \"$1\" | \"$2\" -" ${dir}/${doxyfile}
            ${CMAKE_BINARY_DIR}/documentation-${name}.over ${_doxygen}
    COMMAND ${CMAKE_COMMAND} -E touch ${out}.stamp
    WORKING_DIRECTORY ${dir}
    DEPENDS doxygen_host ${dir}/${doxyfile} ${CMAKE_BINARY_DIR}/documentation-${name}.over ${srcs}
    COMMENT "Documentation: ${name}" VERBATIM)
  set(_doc_stamps ${_doc_stamps} ${out}.stamp PARENT_SCOPE)
endfunction()

set(_libsyllable ${PROJECT_SOURCE_DIR}/system/sys/appserver/libsyllable)
_syllable_doc(libsyllable ${_libsyllable} libsyllable.dox
  "INPUT=. util gui storage translation ${_hdr}/util ${_hdr}/gui ${_hdr}/storage ${_hdr}/translation"
  "INCLUDE_PATH=${_hdr}")
_syllable_doc(libmedia ${PROJECT_SOURCE_DIR}/system/sys/media/libmedia libmedia.dox
  "INPUT=. ${_hdr}/media" "INCLUDE_PATH=${_hdr}")
_syllable_doc(driver-api ${PROJECT_SOURCE_DIR}/system/sys/kernel/kernel driverapi.dox
  "INCLUDE_PATH=${_hdr}")
_syllable_doc(syscalls ${PROJECT_SOURCE_DIR}/system/sys/kernel/kernel syscalls.dox
  "INCLUDE_PATH=${_hdr}")

# The page the desktop link opens.  Absolute links: Webster resolves relative
# ones against the link (~/Desktop), not the file it points to.
file(WRITE ${_docs}-index.html [=[
<html><head><title>Syllable developer documentation</title></head>
<body>
<h1>Syllable developer documentation</h1>
<ul>
<li><a href="/documentation/developer/libsyllable/html/index.html">libsyllable</a>: the high-level API (GUI, storage, utilities, translation)</li>
<li><a href="/documentation/developer/libmedia/html/index.html">libmedia</a>: the media framework</li>
<li><a href="/documentation/developer/driver-api/html/index.html">Device driver API</a>: the kernel's functions for drivers</li>
<li><a href="/documentation/developer/syscalls/html/index.html">System calls</a>: the kernel's system calls</li>
</ul>
</body></html>
]=])

add_custom_target(documentation ALL DEPENDS ${_doc_stamps})
install(DIRECTORY ${_docs}/ DESTINATION documentation/developer
        PATTERN "*.stamp" EXCLUDE PATTERN "*.log" EXCLUDE)
install(FILES ${_docs}-index.html DESTINATION documentation/developer RENAME index.html)
