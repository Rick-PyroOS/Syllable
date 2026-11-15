# Mesa 3D for Syllable (Pyro): OpenGL 1.5 in software, with GLUT and the Mesa
# demos.  See README.md.
#
#   libGL.so.6     Mesa 6.5.2's core and software renderer, with the OSMesa
#                  interface (Mesa's own make, configs-syllable)
#   libGLU.so.1    the SGI GLU, from the same tree
#   libGLView.so.1 os::GLView: an os::View that renders with OSMesa into an
#                  os::Bitmap (glview/)
#   libglut.so.3   GLUT 3.7 on os::GLView (glut/, with GLUT's own fonts and
#                  shapes from the MesaGLUT sources)
#   applications/MesaDemos   Mesa's demos (progs/demos), and GLTest, the
#                  example of the Mesa 6.0.1 Syllable port
#
# The three Mesa archives are downloaded at build time, like the ports'.
# In the tree or with `sb compile system/libs/mesa`.

include(ExternalProject)

set(_mesa_url https://downloads.sourceforge.net/project/mesa3d)
set(_mesa ${CMAKE_CURRENT_BINARY_DIR}/MesaLib)
set(_mesaglut ${CMAKE_CURRENT_BINARY_DIR}/MesaGLUT)
set(_mesademos ${CMAKE_CURRENT_BINARY_DIR}/MesaDemos)
if(NOT SYLLABLE_JOBS)
  set(SYLLABLE_JOBS 4)
endif()
get_filename_component(_mesa_cross_bin ${CMAKE_C_COMPILER} DIRECTORY)

# ---- libGL and libGLU: Mesa's own make ---------------------------------------
ExternalProject_Add(mesa_gl
  URL ${_mesa_url}/MesaLib/6.5.2/MesaLib-6.5.2.tar.bz2
  URL_HASH SHA256=137f50a30461d51eb9af5aac737bc788d536354cf47b26129b97bde6e41fb85f
  DOWNLOAD_DIR ${CMAKE_BINARY_DIR}/downloads
  DOWNLOAD_EXTRACT_TIMESTAMP ON
  SOURCE_DIR ${_mesa}
  BUILD_IN_SOURCE ON
  CONFIGURE_COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/configs-syllable
                    <SOURCE_DIR>/configs/current
            COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/mesa-x86-Makefile
                    <SOURCE_DIR>/src/mesa/x86/Makefile
            COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/mesa-x86-Makefile
                    <SOURCE_DIR>/src/mesa/x86-64/Makefile
  BUILD_COMMAND ${CMAKE_COMMAND} -E env --unset=TARGET PATH=${_mesa_cross_bin}:$ENV{PATH}
                make -j${SYLLABLE_JOBS} CC=${CMAKE_C_COMPILER} CXX=${CMAKE_CXX_COMPILER}
  INSTALL_COMMAND ""
  BUILD_BYPRODUCTS ${_mesa}/lib/libGL.so.6 ${_mesa}/lib/libGLU.so.1
                   ${_mesa}/lib/libGL.so.6.5.2 ${_mesa}/lib/libGLU.so.1.3.060502
  LOG_BUILD ON LOG_OUTPUT_ON_FAILURE ON)
# the configure step copies these: run it again (and the build) when they change
ExternalProject_Add_StepDependencies(mesa_gl configure
  ${CMAKE_CURRENT_SOURCE_DIR}/configs-syllable ${CMAKE_CURRENT_SOURCE_DIR}/mesa-x86-Makefile)

add_library(mesa_GL SHARED IMPORTED GLOBAL)
set_target_properties(mesa_GL PROPERTIES IMPORTED_LOCATION ${_mesa}/lib/libGL.so.6
  IMPORTED_SONAME libGL.so.6)
add_dependencies(mesa_GL mesa_gl)
add_library(mesa_GLU SHARED IMPORTED GLOBAL)
set_target_properties(mesa_GLU PROPERTIES IMPORTED_LOCATION ${_mesa}/lib/libGLU.so.1
  IMPORTED_SONAME libGLU.so.1)
add_dependencies(mesa_GLU mesa_gl)

# GL/ (Mesa; GL/glut.h is in the MesaGLUT archive) and gui/GLView.h for
# everything below
set(_mesa_view ${CMAKE_CURRENT_BINARY_DIR}/include)
configure_file(glview/GLView.h ${_mesa_view}/gui/GLView.h COPYONLY)
set(_mesa_includes ${_mesa_view} ${_mesa}/include ${_mesaglut}/include)

# the soname as the file name: the loader looks for it, and sb execute copies
# no symbolic links
set(_lib_perms PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE
               WORLD_READ WORLD_EXECUTE)
install(FILES ${_mesa}/lib/libGL.so.6.5.2 DESTINATION system/libraries RENAME libGL.so.6 ${_lib_perms})
install(FILES ${_mesa}/lib/libGLU.so.1.3.060502 DESTINATION system/libraries RENAME libGLU.so.1
  ${_lib_perms})
foreach(_h gl.h glext.h glu.h osmesa.h)
  install(FILES ${_mesa}/include/GL/${_h} DESTINATION system/development/headers/GL)
endforeach()
install(FILES ${_mesaglut}/include/GL/glut.h DESTINATION system/development/headers/GL)
install(FILES glview/GLView.h DESTINATION system/development/headers/gui)

# ---- os::GLView ---------------------------------------------------------------
syllable_library(glview SONAME libGLView.so.1
  SOURCES glview/glview.cpp
  INCLUDES ${_mesa_includes}
  LIBS libsyllable mesa_GL
  STRIP INSTALL system/libraries)
add_dependencies(glview mesa_gl)

# ---- GLUT -----------------------------------------------------------------------
# GLUT 3.7's fonts, shapes and teapot are portable: they come from the MesaGLUT
# archive, with our glutint.h over its X11 one
set(_glut_portable 8x13 9x15 bitmap bwidth hel10 hel12 hel18 mroman roman shapes stroke
                   swidth teapot tr10 tr24)
set(_glut_sources)
foreach(_f ${_glut_portable})
  list(APPEND _glut_sources ${_mesaglut}/src/glut/glx/glut_${_f}.c)
endforeach()
ExternalProject_Add(mesa_glut_src
  URL ${_mesa_url}/MesaGLUT/6.5.2/MesaGLUT-6.5.2.tar.bz2
  URL_HASH SHA256=32bc1bfbdeb381de058c007a91a744017a859a692979277ac01505f17d23cf62
  DOWNLOAD_DIR ${CMAKE_BINARY_DIR}/downloads
  DOWNLOAD_EXTRACT_TIMESTAMP ON
  SOURCE_DIR ${_mesaglut}
  CONFIGURE_COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/glut/glutint.h
                    <SOURCE_DIR>/src/glut/glx/glutint.h
  BUILD_COMMAND "" INSTALL_COMMAND ""
  BUILD_BYPRODUCTS ${_glut_sources})
ExternalProject_Add_StepDependencies(mesa_glut_src configure ${CMAKE_CURRENT_SOURCE_DIR}/glut/glutint.h)
set_source_files_properties(${_glut_sources} PROPERTIES GENERATED ON)

syllable_library(glut SONAME libglut.so.3
  SOURCES glut/glut_syllable.cpp ${_glut_sources}
  INCLUDES ${_mesa_includes} ${CMAKE_CURRENT_SOURCE_DIR}/glut
  LIBS glview libsyllable mesa_GLU mesa_GL m
  STRIP INSTALL system/libraries)
target_compile_options(glut PRIVATE $<$<COMPILE_LANGUAGE:C>:-w>)   # Kilgard's 1990s C
add_dependencies(glut mesa_gl mesa_glut_src)

# ---- The demos --------------------------------------------------------------------
# Mesa's progs/demos, as its Makefile lists them.  They open their data files
# relative to the working directory ("isosurf.dat", "../images/reflect.rgb"):
# demo_main.c runs them in their own folder, applications/MesaDemos/bin, next
# to applications/MesaDemos/images.
set(_mesa_demos
  arbfplight arbfslight arbocclude bounce clearspd cubemap drawpix engine fire
  fogcoord fplight fslight gamma gearbox gears geartrain glinfo gloss glslnoise
  gltestperf glutfx isosurf ipers lodbias morph3d multiarb paltex pointblast ray
  readpix reflect renormal shadowtex singlebuffer streaming_rect spectex
  spriteblast stex3d teapot terrain tessdemo texcyl texdown texenv texobj trispd
  tunnel tunnel2 vao_demo winpos)
set(MESA_DEMOS ${_mesa_demos} CACHE STRING "The Mesa demos to build (default: all)")

set(_demo_dir ${_mesademos}/progs/demos)
# readtex, trackball and showbuffer: MesaDemos has them in progs/demos (progs/util
# is only in the MesaLib archive)
set(_util_dir ${_demo_dir})
set(_demo_util ${_util_dir}/readtex.c ${_util_dir}/trackball.c ${_util_dir}/showbuffer.c)
set(_demo_byproducts ${_demo_util})
foreach(_d ${MESA_DEMOS})
  list(APPEND _demo_byproducts ${_demo_dir}/${_d}.c)
endforeach()
ExternalProject_Add(mesa_demos_src
  URL ${_mesa_url}/MesaDemos/6.5.2/MesaDemos-6.5.2.tar.bz2
  URL_HASH SHA256=804f74959bf5f0bb58e159f1a62c7ed8290ab4d92fdce9ed986d0eaf81e70eea
  DOWNLOAD_DIR ${CMAKE_BINARY_DIR}/downloads
  DOWNLOAD_EXTRACT_TIMESTAMP ON
  SOURCE_DIR ${_mesademos}
  CONFIGURE_COMMAND "" BUILD_COMMAND "" INSTALL_COMMAND ""
  BUILD_BYPRODUCTS ${_demo_byproducts})
set_source_files_properties(${_demo_byproducts} PROPERTIES GENERATED ON)

syllable_static_library(mesademo_util SOURCES ${_demo_util}
  INCLUDES ${_mesa_includes} ${_util_dir} OPTIONS -w)
add_dependencies(mesademo_util mesa_gl mesa_glut_src mesa_demos_src)

foreach(_d ${MESA_DEMOS})
  # main() is the demo's mesademo_main(), called by demo_main.c's
  set_source_files_properties(${_demo_dir}/${_d}.c PROPERTIES COMPILE_DEFINITIONS main=mesademo_main)
  syllable_program(mesademo_${_d} OUTPUT_NAME ${_d}
    SOURCES ${_demo_dir}/${_d}.c demos/demo_main.c
    INCLUDES ${_mesa_includes} ${_util_dir}
    OPTIONS -w
    LIBS mesademo_util glut glview mesa_GLU mesa_GL libsyllable m
    STRIP INSTALL applications/MesaDemos/bin)
  add_dependencies(mesademo_${_d} mesa_gl mesa_glut_src mesa_demos_src)
endforeach()
# In the Dock's menu: Other > Mesa, in folders (the Dock reads "Other/Mesa/
# Showcase" as the Showcase folder in the Mesa folder in Other).  A menu
# doesn't scroll, so one folder of all of them would run off the screen.
# glinfo only prints, so it stays out of the menu; a demo in no list goes in
# Mesa itself.
set(_mesa_menu_Showcase bounce engine fire gearbox gears geartrain gloss ipers isosurf
  morph3d ray reflect teapot terrain tunnel tunnel2)
set(_mesa_menu_Shaders arbfplight arbfslight fplight fslight glslnoise)
set(_mesa_menu_Textures cubemap lodbias multiarb paltex shadowtex spectex stex3d texcyl
  texdown texenv texobj)
set(_mesa_menu_Tests arbocclude clearspd drawpix fogcoord gamma gltestperf glutfx
  pointblast readpix renormal singlebuffer spriteblast streaming_rect tessdemo trispd
  vao_demo winpos)
set(_mesa_menu_folders Showcase Shaders Textures Tests)
set(_mesa_menu_name_Showcase "Showcase")
set(_mesa_menu_name_Shaders "Shaders")
set(_mesa_menu_name_Textures "Textures")
set(_mesa_menu_name_Tests "Tests and Benchmarks")
foreach(_d ${MESA_DEMOS})
  set(_category Other/Mesa)
  foreach(_f ${_mesa_menu_folders})
    list(FIND _mesa_menu_${_f} ${_d} _i)
    if(NOT _i EQUAL -1)
      set(_category "Other/Mesa/${_mesa_menu_name_${_f}}")
    endif()
  endforeach()
  if(_d STREQUAL "glinfo")
    set(_category Ignore)
  endif()
  syllable_attribute(applications/MesaDemos/bin/${_d} os::Category "${_category}")
endforeach()
install(FILES ${_demo_dir}/geartrain.dat ${_demo_dir}/isosurf.dat ${_demo_dir}/terrain.dat
  DESTINATION applications/MesaDemos/bin)
install(DIRECTORY ${_mesademos}/progs/images/ DESTINATION applications/MesaDemos/images
  FILES_MATCHING PATTERN "*.rgb" PATTERN "*.rgba")

# GLTest: the Mesa 6.0.1 port's GLView example (Builder's mesa-6.0.1), as it
# was; it opens texture.raw in the working directory, so demo_main runs it in
# its folder too
set(_gltest ${SYLLABLE_BUILDER_PORTS}/mesa-6.0.1/patches/progs/syllable/test1)
set_source_files_properties(${_gltest}/GLTest.cpp PROPERTIES COMPILE_DEFINITIONS main=mesademo_main)
syllable_program(gltest OUTPUT_NAME GLTest SOURCES ${_gltest}/GLTest.cpp demos/demo_main.cpp
  INCLUDES ${_mesa_includes}
  OPTIONS -include stdlib.h   # it calls malloc() without the header
  LIBS glview mesa_GLU mesa_GL libsyllable
  STRIP INSTALL applications/MesaDemos/GLTest)
add_dependencies(gltest mesa_gl)
install(FILES ${_gltest}/texture.raw DESTINATION applications/MesaDemos/GLTest)
syllable_attribute(applications/MesaDemos/GLTest/GLTest os::Category Other/Mesa)
