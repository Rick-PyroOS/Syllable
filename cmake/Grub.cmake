# GRUB 2, the boot loader: built for the build host from upstream source with
# our patches (system/sys/boot/grub2/patches), platform i386-pc (BIOS).
#
#   ${SYLLABLE_GRUB_DIR}/core.img   boot code written after the MBR by
#                                   make-image.sh (finds /boot/grub by search)
#   ${SYLLABLE_GRUB_DIR}/boot.img   the MBR's boot code
#   ${SYLLABLE_GRUB_DIR}/core-part1.img
#                                   a small core.img (about 28 KiB) for disks
#                                   whose first partition starts at sector 63:
#                                   reads /boot/grub from the first partition
#                                   of the disk it starts from (the release
#                                   installer writes it)
#   ${SYLLABLE_GRUB_DIR}/eltorito.img
#                                   El Torito boot image for CDs (/boot/grub
#                                   on the CD)
#   /boot/grub in the stage         grub.cfg, i386-pc/*.mod, fonts, theme, background,
#                                   splash.img (the kernel's boot splash, a boot
#                                   module: system/sys/boot/splash)
#
# Needs on the build host: gcc able to build -m32 code, make, bison, flex,
# gawk (mawk cannot run genmoddep.awk), python3.

set(SYLLABLE_GRUB_VERSION 2.12)
set(_grub_src ${PROJECT_SOURCE_DIR}/system/sys/boot/grub2)
set(SYLLABLE_GRUB_PREFIX ${CMAKE_BINARY_DIR}/host-grub)
set(SYLLABLE_GRUB_DIR ${CMAKE_BINARY_DIR}/grub)
set(_grub_lib ${SYLLABLE_GRUB_PREFIX}/lib/grub/i386-pc)
file(GLOB _grub_patches ${_grub_src}/patches/*.patch)
list(SORT _grub_patches)

ExternalProject_Add(grub_host
  PREFIX ${CMAKE_BINARY_DIR}/ports/grub_host
  URL https://ftp.gnu.org/gnu/grub/grub-${SYLLABLE_GRUB_VERSION}.tar.xz
      https://ftpmirror.gnu.org/grub/grub-${SYLLABLE_GRUB_VERSION}.tar.xz
  URL_HASH SHA256=f3c97391f7c4eaa677a78e090c7e97e6dc47b16f655f04683ebd37bef7fe0faa
  DOWNLOAD_DIR ${CMAKE_BINARY_DIR}/downloads
  DOWNLOAD_EXTRACT_TIMESTAMP ON
  INSTALL_DIR ${SYLLABLE_GRUB_PREFIX}
  PATCH_COMMAND ${PROJECT_SOURCE_DIR}/cmake/scripts/patch-grub.sh ${_grub_patches}
  CONFIGURE_COMMAND env -u TARGET CC=/usr/bin/gcc <SOURCE_DIR>/configure
                    --prefix=<INSTALL_DIR> --with-platform=pc --target=i386
                    --disable-werror --disable-nls --disable-grub-mount
                    --disable-device-mapper --disable-liblzma --disable-libzfs
                    --enable-grub-mkfont=no --enable-grub-themes=no
  BUILD_COMMAND env -u TARGET make -j${SYLLABLE_JOBS}
  INSTALL_COMMAND env -u TARGET make install
  BUILD_BYPRODUCTS ${SYLLABLE_GRUB_PREFIX}/bin/grub-mkimage ${_grub_lib}/boot.img
  LOG_CONFIGURE ON LOG_BUILD ON LOG_INSTALL ON LOG_OUTPUT_ON_FAILURE ON)
ExternalProject_Add_StepDependencies(grub_host patch ${_grub_patches})

# core.img: the modules needed to find the volume and read grub.cfg; the rest
# are loaded from /boot/grub/i386-pc.  It goes in the 1 MiB before the
# partition, so its size (about 155 KiB) is no concern.
set(_grub_core_modules biosdisk part_msdos afs search search_fs_file normal
    multiboot configfile serial terminal echo test ls cat)
set(_grub_mkimage ${SYLLABLE_GRUB_PREFIX}/bin/grub-mkimage -d ${_grub_lib})
set(_grub_images ${SYLLABLE_GRUB_DIR}/core.img ${SYLLABLE_GRUB_DIR}/boot.img
    ${SYLLABLE_GRUB_DIR}/core-part1.img ${SYLLABLE_GRUB_DIR}/eltorito.img)
# The kernel's boot splash, from the same picture as GRUB's background
set(_splash_src ${PROJECT_SOURCE_DIR}/system/sys/boot/splash)
add_custom_command(
  OUTPUT ${SYLLABLE_GRUB_DIR}/splash.img
  COMMAND ${CMAKE_COMMAND} -E make_directory ${SYLLABLE_GRUB_DIR}
  COMMAND python3 ${_splash_src}/make-splash.py ${_grub_src}/pyro-boot.png ${SYLLABLE_GRUB_DIR}/splash.img
  DEPENDS ${_splash_src}/make-splash.py ${_grub_src}/pyro-boot.png
  VERBATIM)
add_custom_command(
  OUTPUT ${_grub_images}
  COMMAND ${CMAKE_COMMAND} -E make_directory ${SYLLABLE_GRUB_DIR}
  COMMAND ${_grub_mkimage} -O i386-pc -o ${SYLLABLE_GRUB_DIR}/core.img
          -c ${_grub_src}/early.cfg -p /boot/grub ${_grub_core_modules}
  # the empty device: the disk GRUB was started from
  COMMAND ${_grub_mkimage} -O i386-pc -o ${SYLLABLE_GRUB_DIR}/core-part1.img
          -p "(,msdos1)/boot/grub" biosdisk part_msdos afs
  COMMAND ${_grub_mkimage} -O i386-pc-eltorito -o ${SYLLABLE_GRUB_DIR}/eltorito.img
          -p /boot/grub biosdisk iso9660
  COMMAND ${CMAKE_COMMAND} -E copy ${_grub_lib}/boot.img ${SYLLABLE_GRUB_DIR}/boot.img
  DEPENDS grub_host ${_grub_src}/early.cfg
  VERBATIM)
add_custom_target(grub ALL DEPENDS ${_grub_images} ${SYLLABLE_GRUB_DIR}/splash.img)

# /boot/grub on the boot volume
install(DIRECTORY ${_grub_lib}/ DESTINATION boot/grub/i386-pc
        FILES_MATCHING PATTERN "*.mod" PATTERN "*.lst")
install(FILES ${_grub_src}/grub.cfg ${_grub_src}/pyro-boot.png ${SYLLABLE_GRUB_DIR}/splash.img DESTINATION boot/grub)
install(FILES ${_grub_src}/fonts/dejavu-sans-mono-16.pf2 ${_grub_src}/fonts/dejavu-sans-14.pf2 ${_grub_src}/fonts/dejavu-sans-bold-16.pf2
        DESTINATION boot/grub/fonts)
file(GLOB _grub_theme_pngs ${_grub_src}/theme/*.png)
install(FILES ${_grub_src}/theme/theme.txt ${_grub_theme_pngs} DESTINATION boot/grub/theme)
