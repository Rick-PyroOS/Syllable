#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <gui/window.h>
#include <gui/textview.h>
#include <gui/button.h>
#include <gui/image.h>
#include <util/application.h>
#include <util/message.h>
#include <util/resources.h>
#include <common/command.h>


using namespace os;
using namespace common;

class MainWindow : public Window
{
public:
	MainWindow();
	void HandleMessage( Message* );
	void Insert( char* szData );
	
private:
	bool OkToQuit(); // Obsolete?
	
	void _UpdateLayout();
	
	/* App */
	Command		*m_pcCommand;
	
	/* GUI */
	TextView	*m_pcDisplayView;
	TextView	*m_pcEntryView;
	Button		*m_pcEntryButton;
	Button		*m_pcClearButton;
};

#endif


