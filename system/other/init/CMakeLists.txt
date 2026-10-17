# Boot scripts init runs (start.sh) and the ones it sources
install(DIRECTORY ./ DESTINATION system USE_SOURCE_PERMISSIONS
  FILES_MATCHING PATTERN "*.sh")
