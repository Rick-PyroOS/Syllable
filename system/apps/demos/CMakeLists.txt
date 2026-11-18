add_subdirectory(rounded)
add_subdirectory(DoubleBuffering)

# Widget Gallery, one per GUI library, side by side for comparison:
#   Demos/Libraries/Syllable/Gallery  (gallery_libsyllable)  libsyllable
#   Demos/Libraries/Pyro/Gallery      (gallery_libos)        libos, cairo port
add_subdirectory(${SYLLABLE_SOURCE_DIR}/system/sys/appserver/libsyllable/gallery
                 gallery-libsyllable)
if(EXISTS ${SYLLABLE_SOURCE_DIR}/system/sys/appserver/libos/gallery/CMakeLists.txt)
  add_subdirectory(${SYLLABLE_SOURCE_DIR}/system/sys/appserver/libos/gallery
                   gallery-libos)
endif()
