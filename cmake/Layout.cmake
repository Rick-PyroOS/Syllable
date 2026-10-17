# The boot volume's layout: the directory skeleton from the Builder profile
# that defines the core system (its "tree" section and the links its
# "prepare" section makes), plus the configuration files the profile's
# "resources" section copies from Builder/resources.

set(BUILDER ${PROJECT_SOURCE_DIR}/system/apps/utils/Builder)

# Directories from the profile's tree section, plus those the Makefiles
# install into that the profile leaves to mkdisttree
file(STRINGS ${BUILDER}/profiles/syllable _profile)
set(_section "")
set(SYLLABLE_SKELETON
  system/libraries system/programs system/config system/resources
  system/development/headers system/resources/catalogs
  system/drivers/dev/bus system/drivers/dev/hcd system/drivers/dev/disk
  system/drivers/fs system/drivers/net/if
  system/drivers/appserver/input system/drivers/appserver/decorators
  system/drivers/appserver/video system/extensions/translators
  system/fonts system/keymaps system/icons applications/system home/root)
foreach(line IN LISTS _profile)
  if(line MATCHES "^([a-z]+)")
    set(_section ${CMAKE_MATCH_1})
  elseif(_section STREQUAL "tree" AND line MATCHES "^\t+([^;\t ][^\t ]*)")
    list(APPEND SYLLABLE_SKELETON ${CMAKE_MATCH_1})
  endif()
endforeach()
list(REMOVE_DUPLICATES SYLLABLE_SKELETON)
foreach(dir ${SYLLABLE_SKELETON})
  install(DIRECTORY DESTINATION ${dir})
endforeach()
# World-writable temporary directories, and the profile's compatibility links
install(CODE "
  set(root \$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX})
  file(MAKE_DIRECTORY \${root}/tmp \${root}/var/tmp)
  execute_process(COMMAND chmod 1777 \${root}/tmp \${root}/var/tmp)
  file(CREATE_LINK usr/ \${root}/resources SYMBOLIC)
  file(CREATE_LINK index/ \${root}/usr/indexes SYMBOLIC)")

# Configuration files from the profile's resources section
install(FILES
  ${BUILDER}/resources/configuration/passwd ${BUILDER}/resources/configuration/group
  ${BUILDER}/resources/configuration/profile ${BUILDER}/resources/configuration/hosts
  ${BUILDER}/resources/configuration/hostname ${BUILDER}/resources/configuration/resolv.conf
  DESTINATION etc)
install(FILES ${BUILDER}/resources/kernel.cfg DESTINATION system/config)
