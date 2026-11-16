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

#ifndef __APPLICATION_H_
#define __APPLICATION_H_

#include <util/application.h>
#include <util/string.h>
#include <util/message.h>

#include "mainwindow.h"

using namespace os;

class EFileBrowser : public os::Application
{
public:
	EFileBrowser(int argc, char* argv[]);   
	
	static AppWindow* GetAppWindow() { return m_AppWindow; };
	static void SetAppWindow( AppWindow* app ) { m_AppWindow = app; };
	void HandleMessage(os::Message*);
private:
	// The main window
	static AppWindow* m_AppWindow;
};

#endif





