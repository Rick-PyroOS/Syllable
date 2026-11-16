// EFileBrowser (C)opyright 2008 Jonas Jarvoll
//
// This is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
// See the GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

#include <vector>

#include <util/application.h>
#include <util/settings.h>
#include <util/resources.h>
#include <gui/desktop.h>

#include "mainwindow.h"
#include "layouter.h"
#include "messages.h"
#include "resources/lang.h"
#include "application.h"
#include "common.h"
#include "preferences.h"
#include "labels.h"
#include <storage/directory.h>

using namespace os;

///////////////////////////////////////////////////////////////////////////////
//
// P R I V A T E
//
///////////////////////////////////////////////////////////////////////////////

class AppWindow :: _Private
{
public:
	_Private()
	{		
	}

	bool Fullscreen;	// If true we are in full screen mode	
	Rect WindowSize;	// Save window size if we go into fullscreen
};

///////////////////////////////////////////////////////////////////////////////
//
// T H E   C L A S S
//
///////////////////////////////////////////////////////////////////////////////
AppWindow :: AppWindow( int argc, char* argv[] ) : Window( Rect(), "main_window", "EFileBrowser" )
{
	
	EFileBrowser::SetAppWindow( this );

	// Set size of window
	SetFrame( Preferences::GetInstance()->GetWindowFrame() );
	
	// Create the private class
	m = new _Private();
	m->Fullscreen = false;


	Layouter::GetInstance()->SetFrame(os::Rect(0,0,Preferences::GetInstance()->GetWindowFrame().Width(),Preferences::GetInstance()->GetWindowFrame().Height()));
	AddChild( Layouter::GetInstance() );
}

AppWindow :: ~AppWindow()
{
	delete m;
}




bool AppWindow :: OkToQuit( void )
{
	// Save application settings
	Preferences::GetInstance()->SetWindowFrame( GetFrame() );
	Preferences::GetInstance()->Save();
	Labels::GetInstance()->SaveSettings();
	
	Application::GetInstance()->PostMessage( M_QUIT );
	return true;	
}

bool AppWindow :: Fullscreen()
{
	m->Fullscreen = !m->Fullscreen;

	if( m->Fullscreen )
	{
		m->WindowSize = GetFrame();

		SetFlags( WND_NO_BORDER | WND_FRONTMOST );
		Desktop cDesktop;
		SetFrame( Rect( Point( 0, 0 ), Point( cDesktop.GetResolution() ) ) );
	}
	else
	{
		SetFlags( 0 );	
		SetFrame( m->WindowSize );
		
		SetTitle( GetTitle() );		// This will force a window border redraw
		FrameSized( Point( 0, 0 ) );  // and this will force a window content redraw :)
	}

	return m->Fullscreen;
}


typedef Plugin* init_filebrowser_plugin( os::Path cPluginFile);
void AppWindow::LoadPlugins()
{	
	os::Directory*  pcDir = new os::Directory(GetPluginPath());
	String zPath;
	String cFile;

	while (pcDir->GetNextEntry(&zPath))
	{
		printf ("%s\n",zPath.c_str());
		
		if (zPath == "." || zPath == "..")
			continue;

		String cFile;
		cFile.Format("%s/%s",GetPluginPath().c_str(),zPath.c_str());

		//ToDo:  using the registar
		if (cFile.substr(cFile.Length()-3,cFile.Length()) == ".so")
		{
			int nID;
		
			//Try to load the plugin
			if( ( nID = load_library( cFile.c_str(), IM_ADDON_SPACE ) ) >= 0 )
			{
				init_filebrowser_plugin* pInit;
				
				if( get_symbol_address( nID, "init_filebrowser_plugin", -1, (void**)&pInit ) == 0 )
				{
					os::Path cPath(cFile);
					Plugin* pcPlugin = pInit(cPath);
					Layouter::GetInstance()->AddPlugin(pcPlugin);
				}
			}
		}
		else
			printf("typenames don't match: FileBrowserPlugin vs %s\n",cFile.c_str());
	}
}



