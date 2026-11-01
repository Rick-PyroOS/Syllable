# Media plugins that need no ports (aac, ffmpeg and vorbis do)
foreach(drv serveraudio oss raw ac97_audio hd_audio screen cdda cdrom)
  add_subdirectory(${drv})
endforeach()
