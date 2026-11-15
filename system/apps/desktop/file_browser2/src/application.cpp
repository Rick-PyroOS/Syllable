#include "application.h"
#include "../include/plugin.h"
using namespace os;

AppWindow* EFileBrowser :: m_AppWindow = NULL;

EFileBrowser :: EFileBrowser( int argc, char* argv[] ) : Application( "application/x-vnd-EFileBrowser" )
{
	try 
	{
		SetCatalog( "lang.catalog" );
	}
	catch( ... )
	{
	}

	
	try
	{
		//printf("%s\n",argv[1]);
		m_AppWindow = new AppWindow( argc, argv );
		m_AppWindow->Show();
		m_AppWindow->MakeFocus();
		//m_AppWindow->LoadPlugins();
		
	}
	catch( String& s )
	{
		Application::GetInstance()->PostMessage( M_QUIT );
	}
}

void EFileBrowser::HandleMessage(os::Message* pcMessage)
{
	switch (pcMessage->GetCode())
	{
		case TOOLBAR:
		case SIDEBAR:
		case PANE:
		{
			m_AppWindow->PostMessage(pcMessage,m_AppWindow);
			break;
		}		
	}	
}






