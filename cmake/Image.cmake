# Disk image of the boot volume, built on demand after `cmake --install .`:
# `cmake --build . --target image` updates it incrementally (see
# cmake/scripts/make-image.sh), `--target image-full` rebuilds it.

set(SYLLABLE_AFSTOOL_SOURCE ${PROJECT_SOURCE_DIR}/../linux-afs/tools/afstool CACHE PATH
    "afstool sources (linux-afs)")
set(SYLLABLE_REFERENCE_DISK "" CACHE FILEPATH
    "Raw Syllable disk whose boot sectors and GRUB files the image reuses")
set(SYLLABLE_IMAGE_OVERLAY "" CACHE PATH "Directory copied over the boot volume in the image")
set(SYLLABLE_IMAGE_DIR ${CMAKE_BINARY_DIR}/image CACHE PATH "Where the image is written")
set(SYLLABLE_IMAGE_SIZE 2G CACHE STRING "Size of the boot partition (the ports with both native compilers need over 256M)")

# afstool: Syllable's AFS code as a Linux program (host tool)
set(AFSTOOL ${CMAKE_BINARY_DIR}/afstool/src/afstool/afstool)
# (its Makefile builds in its own directory, so it builds in a copy, refreshed
# from linux-afs every time; make then rebuilds only what changed)
ExternalProject_Add(afstool
  PREFIX ${CMAKE_BINARY_DIR}/afstool
  DOWNLOAD_COMMAND ""
  CONFIGURE_COMMAND ""
  BUILD_COMMAND rsync -a --exclude obj --exclude afstool ${SYLLABLE_AFSTOOL_SOURCE}/ <SOURCE_DIR>/
        COMMAND make -s CC=/usr/bin/gcc AFS_SRC=${PROJECT_SOURCE_DIR}/system/sys/kernel/fs/afs
  BUILD_IN_SOURCE ON
  BUILD_ALWAYS ON
  INSTALL_COMMAND ""
  BUILD_BYPRODUCTS ${AFSTOOL}
  EXCLUDE_FROM_ALL ON)

# image: sync the existing image (only changed files are written), or build
# it if there is none or its size / boot sectors changed.  image-full: always
# from scratch.  Both expect `cmake --install .` to have run.
foreach(target image image-full)
  if(target STREQUAL "image")
    set(mode auto)
  else()
    set(mode full)
  endif()
  add_custom_target(${target}
    COMMAND ${PROJECT_SOURCE_DIR}/cmake/scripts/make-image.sh ${mode} ${AFSTOOL}
            "${SYLLABLE_REFERENCE_DISK}" ${CMAKE_INSTALL_PREFIX} "${SYLLABLE_IMAGE_OVERLAY}"
            ${SYLLABLE_IMAGE_DIR} ${SYLLABLE_IMAGE_SIZE} ${CMAKE_BINARY_DIR}/image-root
    DEPENDS afstool
    USES_TERMINAL VERBATIM)
endforeach()
