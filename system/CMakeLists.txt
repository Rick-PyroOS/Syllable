# The Syllable tree, by area.  Ordering that matters for installation
# (the layout, configuration and boot scripts) stays in the top-level
# CMakeLists.txt, after this.
add_subdirectory(sys)       # kernel, drivers, file systems, appserver, media, translators
add_subdirectory(libs)      # libpthread, libdl, codeview
add_subdirectory(apps)      # desktop, development tools, utilities, preferences
add_subdirectory(other)     # fonts, keymaps, icons
