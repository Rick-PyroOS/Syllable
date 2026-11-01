# nvidia: not in the release; it would hand NVIDIA cards to the nvidia video
# driver, which is left out (see appserver/drivers/video)
foreach(drv geforcefx i855 mach64 mga radeon riva s3_chrome s3_virge savage sis3xx
            tdfx trident vmware)
  add_subdirectory(${drv})
endforeach()
