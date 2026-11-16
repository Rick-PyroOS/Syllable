# Makefile for EFileBrowser
# Converted from its Makefile (Neovim: Convert to CMakeLists.txt). Check it, then
# add_subdirectory() it from the CMakeLists.txt above if that doesn't yet.
# TODO(make): resources/ also has translations the Makefile didn't build: sv (add them to LANGS to build them)

syllable_program(efilebrowser OUTPUT_NAME EFileBrowser SOURCES main.cpp application.cpp
  mainwindow.cpp common.cpp iconserver.cpp labels.cpp preferences.cpp shortcuts.cpp
  my_gui/address_field.cpp my_gui/address_field_button.cpp my_gui/sidebar_base.cpp
  my_gui/sidebar_title.cpp layouter.cpp statusbar.cpp syll_gui/icondirview.cpp
  syll_gui/iconview.cpp syll_gui/splitter.cpp syll_gui/tabview.cpp syll_gui/toolbar.cpp
  syll_gui/imagebutton.cpp syll_gui/radiomenu.cpp syll_gui/popupmenu.cpp tabs/tab_base.cpp
  tabs/tab_browser.cpp settings/keyboardsettings.cpp settings/settingsdialog.cpp
  settings/settingsnode.cpp OPTIONS -Wall -O2 LIBS libsyllable INSTALL system/programs/file_browser2 )
file(GLOB _files RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/images/*.png)
syllable_resources(efilebrowser FILES ${_files})
syllable_catalogs(efilebrowser NAME lang DIR resources EMBED)
#syllable_attribute(applications/EFileBrowser/EFileBrowser os::Category)


