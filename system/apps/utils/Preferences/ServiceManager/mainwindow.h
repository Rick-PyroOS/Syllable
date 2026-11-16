#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <gui/window.h>
#include <gui/layoutview.h>
#include <gui/listview.h>
#include <gui/button.h>
#include <gui/image.h>
#include <util/resources.h>
#include <util/application.h>
#include <util/message.h>
#include <util/event.h>
#include <util/settings.h>

class MainWindow : public os::Window
{
public:
	MainWindow();
	void LoadServices();
	void SaveServices();
	void StartService();
	void StopService();
	void HandleMessage( os::Message* );
private:
	bool OkToQuit();
	#include "mainwindowLayout.h"
	dirent *psEntry;
	DIR *pDir;
};

#endif

