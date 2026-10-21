# Layout editor
syllable_program(layout_editor OUTPUT_NAME LayoutEditor
  SOURCES AppWindow.cpp EditList.cpp EditString.cpp PropertyRow.cpp WidgetsControls.cpp
          WidgetsImage.cpp WidgetsList.cpp WidgetsMenu.cpp WidgetsNodes.cpp WidgetsUtils.cpp
          application.cpp main.cpp mainwindow.cpp ccatalog.cpp
  OPTIONS -O2 -Wall LIBS libsyllable INSTALL applications/sIDE)
syllable_resources(layout_editor FILES images/breaker.png images/open.png images/save.png
  icon24x24.png icon48x48.png application_interface.png)
syllable_attribute(applications/sIDE/LayoutEditor os::Category Development)
