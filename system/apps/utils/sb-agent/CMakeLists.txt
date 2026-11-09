# sb-agent: lets Syllable-Build-System's ./sb execute list, copy and run
# programs here from the Mac (started on ./sb image's images, not on a
# release's: system/index/tasks/start/sb-agent)
syllable_program(sb-agent SOURCES sb-agent.c INSTALL system/development/bin)
# sb-dock: has the running Dock load a plugin (./sb compile --run)
syllable_program(sb-dock SOURCES sb-dock.cpp LIBS libsyllable INSTALL system/development/bin)
