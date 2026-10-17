# System fonts: the base set and Bitstream Vera
file(GLOB BASE_FONTS ${CMAKE_CURRENT_SOURCE_DIR}/base/*)
file(GLOB VERA_FONTS ${CMAKE_CURRENT_SOURCE_DIR}/vera/*.ttf)
install(FILES ${BASE_FONTS} ${VERA_FONTS} DESTINATION system/fonts)
