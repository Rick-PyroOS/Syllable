# Sourcery: the code editor
syllable_program(sourcery OUTPUT_NAME Sourcery
  SOURCES main.cpp file.cpp settings.cpp splash.cpp application.cpp editor.cpp mainwindow.cpp
          aboutbox.cpp statusbar.cpp formatset.cpp commonfuncs.cpp link.cpp convertdialog.cpp
          fileproperties.cpp finddialog.cpp replacedialog.cpp userinserts.cpp gotodialog.cpp
          browser.cpp settingsdialog.cpp execute.cpp toolsdialog.cpp colorbutton.cpp
  OPTIONS -O2 -Wall LIBS libsyllable codeview m INSTALL applications/sIDE)
file(GLOB _res RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/images/*.png ${CMAKE_CURRENT_SOURCE_DIR}/images/mimetypes/*.png)
syllable_resources(sourcery FILES ${_res})
syllable_attribute(applications/sIDE/Sourcery os::Category Development)
