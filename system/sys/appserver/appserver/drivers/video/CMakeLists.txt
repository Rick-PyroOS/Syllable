# nvidia: not in the release (riva and geforcefx cover NVIDIA cards) and its
# source predates the current appserver API (IRect, CS_RGB*), so it is left out
foreach(video geforcefx i855 mach64 mga radeon riva s3 s3_chrome savage sis3xx tdfx trident vmware)
  add_subdirectory(${video})
endforeach()
