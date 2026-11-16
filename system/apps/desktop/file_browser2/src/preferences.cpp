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

#include "preferences.h"

using namespace os;

/*
  The Preferences is a Singleton and contains all possible settings for 
  EFileBrowser. It is loaded at start up and saved when EFileBrowser quits.
*/

///////////////////////////////////////////////////////////////////////////////
//
// S I N G L E T O N
//
///////////////////////////////////////////////////////////////////////////////

Preferences* Preferences::m_theInstance = NULL;

Preferences* Preferences :: GetInstance()
{
	if( m_theInstance == NULL )
	{
		m_theInstance = new Preferences();
		m_theInstance->Load();
	}

	return m_theInstance;
}

///////////////////////////////////////////////////////////////////////////////
//
// T H E   C L A S S
//
///////////////////////////////////////////////////////////////////////////////

Preferences :: Preferences() : Settings()
{
	SetFile("layout");
}

Preferences :: ~Preferences()
{
}

Rect Preferences :: GetWindowFrame()
{
	return GetRect( "window_position", Rect( 100,100,400,275 ) );
}

void Preferences :: SetWindowFrame( const Rect frame )
{
	SetRect( "window_position", frame );
}

void Preferences :: SetSplitRatio( float value )
{
	SetFloat("split_ratio",value);
}

float Preferences :: GetSplitRatio()
{
	return GetFloat("split_ratio",0.2f);
}

String Preferences :: GetStartUpPath()
{
	return GetString( "startup_path", getenv("HOME") );
}

void Preferences :: SetStartUpPath( const String path )
{
	SetString( "startup_path", path );
}

void Preferences :: SetDefaultView( IconView::view_type style )
{
	SetInt32("window_style",style);
}

IconView::view_type Preferences :: GetDefaultView()
{
	return (IconView::view_type)GetInt32("window_style",IconView::VIEW_ICONS);
}

void Preferences :: SetUseEmblems( const bool active )
{
	SetBool( "use_emblems", active );
}

bool Preferences :: GetUseEmblems()
{
	return GetBool( "use_emblems", true );
}

Point Preferences :: GetEmblemPosition()
{
	return GetPoint( "emblem_position", Point( 1.0f, 1.0f ) );
}

void Preferences :: SetEmblemPosition( const Point pos )
{
	SetPoint( "emblem_position", pos );
}

void Preferences::SetShowToolBar(bool toolbar)
{
	SetBool("show_toolbar",toolbar);
}

bool Preferences::GetShowToolBar()
{
	return GetBool("show_toolbar",true);
}

void Preferences::SetToolBarType(os::ImageButton::label_position e)
{
	SetInt32("toolbar_type",e);
}

ImageButton::label_position Preferences::GetToolBarType()
{
	return (os::ImageButton::label_position)GetInt32("toolbar_type",os::ImageButton::IB_TEXT_BOTTOM);
}


void Preferences::SetShowSideBar(bool sidebar)
{
	SetBool("show_sidebar",sidebar);
}

bool Preferences::GetShowSideBar()
{
	return GetBool("show_sidebar",false);
}

void Preferences::SetShowStatusBar(bool sidebar)
{
	SetBool("show_statusbar",sidebar);
}

bool Preferences::GetShowStatusBar()
{
	return GetBool("show_statusbar",false);
}

void Preferences::SetShowHiddenFiles(bool show)
{
	SetBool("show_hidden",show);
}


bool Preferences::GetShowHiddenFiles()
{
	return GetBool("show_hidden",false);
}


void Preferences::SetShowPane(bool pane)
{
	SetBool("show_pane",pane);
}

bool Preferences::GetShowPane()
{
	return GetBool("show_pane",false);
}

float Preferences::GetZoom()
{
	return GetFloat("zoom",1.00);
}

void Preferences::SetZoom(float vZoom)
{
	SetFloat("zoom",vZoom);
}




