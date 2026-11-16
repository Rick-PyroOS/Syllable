#include <common/debug.h>

#include "application.h"
#include "mainwindow.h"


App::App()
 : Application( "application/x-vnd.pyro-libads::system_test" )
{
	FN_START();
	
	m_pcMainWindow = new MainWindow();
	m_pcMainWindow->CenterInScreen();
	m_pcMainWindow->Show();
	m_pcMainWindow->MakeFocus();
	
	FN_END();
}



