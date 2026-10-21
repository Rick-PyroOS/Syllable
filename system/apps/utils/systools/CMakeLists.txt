# System tools.  init is the first userland process (the kernel runs
# /boot/system/programs/init); the others need libsyllable and follow it.
syllable_program(init SOURCES init.c INSTALL system/programs)
foreach(tool ps top nice semctl strace sysinfo)
  syllable_program(${tool} SOURCES ${tool}.c INSTALL system/programs)
endforeach()
syllable_program(unbuffer SOURCES unbuffer.c LIBS util INSTALL system/programs)
foreach(tool ls_ports ls_sema shutdown open cbcopy cbpaste alert)
  syllable_program(${tool} SOURCES ${tool}.cpp LIBS libsyllable INSTALL system/programs)
endforeach()
install(PROGRAMS mkhome kill_all DESTINATION system/programs)
install(CODE "file(CREATE_LINK /system/programs/shutdown
  \$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/system/programs/reboot SYMBOLIC)")
