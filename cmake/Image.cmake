# Disk image of the boot volume (built on demand: `cmake --build . --target
# image` after `cmake --install .`).  Full rebuild for now; phase 3 of the
# plan adds incremental sync.

set(SYLLABLE_AFSTOOL_SOURCE ${PROJECT_SOURCE_DIR}/../linux-afs/tools/afstool CACHE PATH
    "afstool sources (linux-afs)")
set(SYLLABLE_REFERENCE_DISK "" CACHE FILEPATH
    "Raw Syllable disk whose boot sectors and GRUB files the image reuses")
set(SYLLABLE_IMAGE_OVERLAY "" CACHE PATH "Directory copied over the boot volume in the image")
set(SYLLABLE_IMAGE_DIR ${CMAKE_BINARY_DIR}/image CACHE PATH "Where the image is written")
set(SYLLABLE_IMAGE_SIZE 256M CACHE STRING "Size of the boot partition")

# afstool: Syllable's AFS code as a Linux program (host tool)
set(AFSTOOL ${CMAKE_BINARY_DIR}/afstool/src/afstool/afstool)
ExternalProject_Add(afstool
  PREFIX ${CMAKE_BINARY_DIR}/afstool
  DOWNLOAD_COMMAND ${CMAKE_COMMAND} -E copy_directory ${SYLLABLE_AFSTOOL_SOURCE} <SOURCE_DIR>
  CONFIGURE_COMMAND ""
  BUILD_COMMAND make -s CC=/usr/bin/gcc AFS_SRC=${PROJECT_SOURCE_DIR}/system/sys/kernel/fs/afs
  BUILD_IN_SOURCE ON
  INSTALL_COMMAND ""
  BUILD_BYPRODUCTS ${AFSTOOL}
  EXCLUDE_FROM_ALL ON)

add_custom_target(image
  COMMAND ${CMAKE_COMMAND} -E echo "Building the disk image from ${CMAKE_INSTALL_PREFIX}"
  COMMAND ${PROJECT_SOURCE_DIR}/cmake/scripts/make-image.sh ${AFSTOOL} "${SYLLABLE_REFERENCE_DISK}"
          ${CMAKE_INSTALL_PREFIX} "${SYLLABLE_IMAGE_OVERLAY}" ${SYLLABLE_IMAGE_DIR} ${SYLLABLE_IMAGE_SIZE}
  DEPENDS afstool
  USES_TERMINAL VERBATIM)
