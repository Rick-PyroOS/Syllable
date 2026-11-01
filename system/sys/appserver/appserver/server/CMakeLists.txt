# The appserver: windows, drawing, fonts, input.  VESA graphics are built in;
# hardware video drivers are add-ons (drivers/video).
syllable_program(appserver
  SOURCES ddriver.cpp ddline.cpp ddmemory.cpp event.cpp vesadrv.cpp server.cpp input.cpp
          config.cpp keyboard.cpp keymap.cpp defaultkeymap.cpp sapplication.cpp swindow.cpp
          layer.cpp toplayer.cpp render.cpp bitmap.cpp crdtrans.cpp sfont.cpp fontnode.cpp
          wndborder.cpp desktop.cpp winselect.cpp sprite.cpp clipboard.cpp
          windowdecorator.cpp defaultdecorator.cpp
  OPTIONS -fexceptions -ffast-math
  DEFINES __NO_SCREENSHOT_SUPPORT__=1
  LIBS syllable::freetype libsyllable m
  INSTALL system STRIP)
