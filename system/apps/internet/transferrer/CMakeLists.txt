# Transferrer: FTP client (Applications/Transferrer)
syllable_program(transferrer OUTPUT_NAME Transferrer
  SOURCES main.cpp application.cpp containerview.cpp localview.cpp remoteview.cpp connectbar.cpp
          ftpparse.c address_field.cpp address_field_button.cpp settingsmenu.cpp progresswindow.cpp
          requesters.cpp transferthread.cpp server.cpp mainwindow.cpp job.cpp
  OPTIONS -O2 -Wall LIBS libsyllable syllable::curl INSTALL applications/Transferrer)
file(GLOB _res RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/images/*.png)
syllable_resources(transferrer FILES ${_res})
syllable_attribute(applications/Transferrer/Transferrer os::Category Internet)
