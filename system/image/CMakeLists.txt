# Files installed onto the boot volume as they are (see README.md)
install(DIRECTORY ./ DESTINATION . USE_SOURCE_PERMISSIONS
  PATTERN CMakeLists.txt EXCLUDE PATTERN README.md EXCLUDE PATTERN registrar-types.json EXCLUDE)

# The registrar's file-type database (~/Settings/registrar): the release's,
# with handlers pointing at this build's applications.  Kept as JSON
# (registrar-types.json; edit that) and written in the registrar's binary
# format by cmake/scripts/registrar-db.py.
set(_db ${CMAKE_CURRENT_BINARY_DIR}/registrar)
add_custom_command(OUTPUT ${_db}
  COMMAND python3 ${PROJECT_SOURCE_DIR}/cmake/scripts/registrar-db.py build
          ${CMAKE_CURRENT_SOURCE_DIR}/registrar-types.json ${_db}
  DEPENDS registrar-types.json ${PROJECT_SOURCE_DIR}/cmake/scripts/registrar-db.py)
add_custom_target(registrar_db ALL DEPENDS ${_db})
foreach(home root .template)
  install(FILES ${_db} DESTINATION home/${home}/Settings)
endforeach()
# Home directories are private (Builder's finish profile)
install(CODE "execute_process(COMMAND chmod -R o-rwx
  \$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/home/.template \$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/home/root)")

# Folder and program icons, as in the release (os::Icon attributes naming a
# file in /system/icons)
syllable_attribute(applications os::Icon /system/icons/applications.png)
syllable_attribute(applications/Preferences os::Icon /system/icons/settings.png)
syllable_attribute(home os::Icon /system/icons/home.png)
syllable_attribute(home/.template os::Icon /system/icons/home.png)
syllable_attribute(home/root os::Icon /system/icons/home.png)
syllable_attribute(home/root/Trash os::Icon /system/icons/trash.png)
# the release's /system/bin/aterm; here system/programs/aterm links to Terminal
syllable_attribute(applications/system/Terminal os::Icon /system/icons/aterm.png)

# The release's /system/bin names that the file-type database
# (registrar-types.json, from the release) uses as default handlers
install(CODE "
  set(bin \$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/system/bin)
  file(MAKE_DIRECTORY \${bin})
  file(CREATE_LINK /applications/AEdit/AEdit \${bin}/aedit SYMBOLIC)
  file(CREATE_LINK /applications/AView/AView \${bin}/aview SYMBOLIC)
  file(CREATE_LINK /applications/Webster/Webster \${bin}/browse SYMBOLIC)
  file(CREATE_LINK /applications/sIDE/Sourcery \${bin}/code-editor SYMBOLIC)")
