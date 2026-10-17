# Keymaps, compiled from their text form with the host keymapconv
file(GLOB MAPS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/*.map)
set(KEYMAPS)
foreach(map ${MAPS})
  get_filename_component(name ${map} NAME_WE)
  set(out ${CMAKE_CURRENT_BINARY_DIR}/keymaps/${name})
  add_custom_command(OUTPUT ${out}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/keymaps
    COMMAND ${SYLLABLE_KEYMAPCONV} ${CMAKE_CURRENT_SOURCE_DIR}/${map} ${out} > /dev/null
    DEPENDS ${map} hosttools)
  list(APPEND KEYMAPS ${out})
endforeach()
add_custom_target(keymaps ALL DEPENDS ${KEYMAPS})
install(FILES ${KEYMAPS} DESTINATION system/keymaps)
