# Files installed onto the boot volume as they are (see README.md)
install(DIRECTORY ./ DESTINATION . USE_SOURCE_PERMISSIONS
  PATTERN CMakeLists.txt EXCLUDE PATTERN README.md EXCLUDE)
# Home directories are private (Builder's finish profile)
install(CODE "execute_process(COMMAND chmod -R o-rwx
  \$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/home/.template \$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/home/root)")
