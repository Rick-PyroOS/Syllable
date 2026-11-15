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

#include "tab_base.h"

using namespace os;

/* 
   The TabBase is the base class for all type of views that want to show
   something in the tabview. Typical classes that displays content of a 
   path or search views shall inherit this class
*/

///////////////////////////////////////////////////////////////////////////////
//
// P R I V A T E
//
///////////////////////////////////////////////////////////////////////////////


class TabBase :: _Private
{
public:
	_Private()
	{		
	}
};

///////////////////////////////////////////////////////////////////////////////
//
// T H E   C L A S S
//
///////////////////////////////////////////////////////////////////////////////
TabBase :: TabBase() : View( Rect(), "", CF_FOLLOW_ALL, WID_WILL_DRAW )
{
	// Create the private class
	my = new _Private();
}

TabBase :: ~TabBase()
{
	delete my;
}

void TabBase :: SetTarget( Handler* handler )
{
}
	
String TabBase :: GetName()
{
	return String( "No name" );
}

String TabBase :: GetTitle()
{
	return String("");
}

bool TabBase :: Close()
{
	return true;
}

void TabBase :: SetPath( const String path, bool notify )
{
}

String TabBase :: GetPath()
{
	return String( "" );
}

void TabBase :: ReRead()
{
}

void TabBase :: MakeHistory()
{
}

String TabBase :: Up()
{
	return String( "" );
}

String TabBase :: Forward()
{
	return String( "" );
}

String TabBase :: Back()
{
	return String( "" );
}

void TabBase::SetDirChangeMsg(os::Message*)
{
}

void TabBase::SetFileChangeMsg(os::Message*)
{
}

os::String TabBase::GetFile()
{
	return String("");
}

void TabBase::SetViewMode(IconView::view_type type)
{
}

IconView::view_type TabBase::GetViewMode()
{
	return IconView::VIEW_ICONS;
}

void TabBase::SelectAll()
{
}

void TabBase::InvertSelection()
{
}

void TabBase::SetShowHiddenFiles(bool)
{
}

bool TabBase::GetShowHiddenFiles()
{
	return false;
}

void TabBase::ClearLabel()
{
}

void TabBase::AddLabel(os::Color32_s s)
{
}

int32_t TabBase::GetSelectedCount()
{
	return 0;
}

int32_t TabBase::GetAllCount()
{
	return 0;
}

int32_t TabBase::GetSize()
{
	return 0;
}

void TabBase::SetZoom(float vZoom)
{
}

float TabBase::GetZoom()
{
	return 1.0f;
}





