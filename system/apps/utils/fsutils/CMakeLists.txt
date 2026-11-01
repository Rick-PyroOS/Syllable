# File system tools (mount, format, attributes, df...)
foreach(tool fsprobe mount unmount format addattrib lsattribs mkindex devstat rmattrib dumpattrib df)
  syllable_program(fs_${tool} OUTPUT_NAME ${tool} SOURCES ${tool}.c INSTALL system/programs)
endforeach()
