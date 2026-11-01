# System fonts: the base set, Bitstream Vera, and LCD2 (the release's
# LCD-Fonts package; Icni's score display asks for "LCD2" "Normal")
file(GLOB BASE_FONTS ${CMAKE_CURRENT_SOURCE_DIR}/base/*)
file(GLOB VERA_FONTS ${CMAKE_CURRENT_SOURCE_DIR}/vera/*.ttf)
install(FILES ${BASE_FONTS} ${VERA_FONTS} lcd/LCD2-Normal.ttf DESTINATION system/fonts)

# DejaVu, the release's system font (system/config/appserver asks for
# "DejaVu Sans"): Builder's DejaVu-Fonts--current package, version 2.25
ExternalProject_Add(dejavu
  PREFIX ${CMAKE_BINARY_DIR}/ports/dejavu
  URL https://downloads.sourceforge.net/project/dejavu/dejavu/2.25/dejavu-fonts-ttf-2.25.tar.bz2
  URL_HASH SHA256=b5f635000e90b8fa3bc692bb2793cd275f189153f73e87b1f31d94764bff48bd
  DOWNLOAD_DIR ${CMAKE_BINARY_DIR}/downloads
  DOWNLOAD_EXTRACT_TIMESTAMP ON
  CONFIGURE_COMMAND "" BUILD_COMMAND "" INSTALL_COMMAND "")
ExternalProject_Get_Property(dejavu SOURCE_DIR)
install(DIRECTORY ${SOURCE_DIR}/ttf/ DESTINATION system/fonts FILES_MATCHING PATTERN "*.ttf")
