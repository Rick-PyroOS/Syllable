# Development headers (/system/development/headers), as the Makefile's dist did
install(DIRECTORY ./ DESTINATION system/development/headers USE_SOURCE_PERMISSIONS
  PATTERN Makefile EXCLUDE PATTERN CMakeLists.txt EXCLUDE PATTERN CVS EXCLUDE)
