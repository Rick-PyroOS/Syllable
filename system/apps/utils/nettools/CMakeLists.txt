# Network tools
foreach(tool ifconfig route ping)
  syllable_program(${tool} SOURCES ${tool}.c INSTALL system/programs)
endforeach()
