# SimpleBurn: CD/DVD burning
syllable_program(simpleburn OUTPUT_NAME SimpleBurn
  SOURCES main.cpp application.cpp mainwindow.cpp splash.cpp erasewindow.cpp
  OPTIONS -Wall LIBS libsyllable libmedia INSTALL applications/SimpleBurn)
syllable_resources(simpleburn FILES
  images/add-audio.png images/add-video.png images/burn.png images/icon24x24.png images/icon48x48.png
  images/menu-audio.png images/menu-backup.png images/menu-data.png images/menu-video.png
  images/move-down.png images/move-up.png images/remove.png images/menu-burn.png images/logo.png)
syllable_attribute(applications/SimpleBurn/SimpleBurn os::Category Other)
