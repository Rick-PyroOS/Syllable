foreach(drv emu10k1 es1370 es1371 maestro sbpro trident via686a ymfpci)
  add_subdirectory(${drv})
endforeach()
