#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <util/application.h>
#include <gui/window.h>
#include <gui/image.h>
#include <util/message.h>
#include <util/resources.h>
#include <util/shortcutkey.h>


#include "top_view.h"

class MainWindow : public os::Window
{
public:
	MainWindow();
	~MainWindow();
	
	void HandleMessage( os::Message* );
	void SetFocus();
private:
	bool OkToQuit();
	TopView* m_pcView;
	os::Message* m_pcEscapeMessage;
	os::ShortcutKey m_cEscapeShortcutKey;
};

#endif







