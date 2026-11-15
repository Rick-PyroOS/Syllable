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

#include <stack>

#include <gui/statusbar.h>
#include <gui/layoutview.h>
#include <gui/menu.h>
#include <storage/fsnode.h>

#include <util/string.h>

#include "tab_browser.h"
#include "../preferences.h"
#include "../messages.h"
#include "../syll_gui/icondirview.h"
#include "../syll_gui/iconview.h"


using namespace std;

/* 
   The TabBrowser is responsible for displaying a directory content
*/

///////////////////////////////////////////////////////////////////////////////
//
// P R I V A T E
//
///////////////////////////////////////////////////////////////////////////////


class TabBrowser :: _Private
{
public:
	_Private()
	{
		_SetupMenus();
	}
	void _SetupMenus()
	{
		_OpenTabItem = new os::MenuItem("Open in New Tab",new os::Message(MSG_BROWSER_OPEN_TAB));
		_OpenWindowItem = new os::MenuItem("Open in New Window",new os::Message(MSG_BROWSER_OPEN_WINDOW));
		
		_CreateDocumentMenu = new os::Menu( os::Rect(), "Create Document", os::ITEMS_IN_COLUMN );
		_CreateDocumentMenu->AddItem("Empty File",new os::Message(MSG_CREATE_DOCUMENT_EMPTY_FILE));
		
	}
	void _SetTarget(Handler* handler)
	{
		_CreateDocumentMenu->SetTargetForItems(handler);
	}
	stack<os::String> _Backward;
	stack<os::String> _Forward;
	IconDirectoryView* _DirView;
	os::StatusBar* _Statusbar;	
	os::MenuItem *_OpenTabItem;
	os::MenuItem* _OpenWindowItem;
	os::Menu* _CreateDocumentMenu;
	
};


///////////////////////////////////////////////////////////////////////////////
//
// T H E   C L A S S
//
///////////////////////////////////////////////////////////////////////////////
TabBrowser :: TabBrowser() : TabBase()
{
	// Create the private class
	my = new _Private();

	// Create widgets
	os::LayoutNode*	root_node = new os::VLayoutNode( "pcRootNode" );
	os::LayoutView*	root_view = new os::LayoutView( GetBounds(), "pcRootView" );
	root_view->SetRoot( root_node );
	AddChild( root_view );

	// Create the widgets
	my->_DirView = new IconDirectoryView( os::Rect(), Preferences::GetInstance()->GetStartUpPath(), os::CF_FOLLOW_ALL );
	my->_DirView->SetMultiSelect( true );
	my->_DirView->SetView(Preferences::GetInstance()->GetDefaultView());
	
	my->_DirView->AddToDirectoryMenu(my->_CreateDocumentMenu,0);
	
	my->_DirView->AddToFileMenu(my->_OpenTabItem,0);
	my->_DirView->AddToFileMenu(my->_OpenWindowItem,1);
	my->_DirView->AddToFileMenu(new os::MenuSeparator(),3);
	
	


	// Attached created widgets to the window
	root_node->AddChild( my->_DirView, 1.0f );
	root_view->InvalidateLayout();	

}

TabBrowser :: ~TabBrowser()
{
	delete my;
}

void TabBrowser :: SetTarget( os::Handler* handler )
{
	my->_DirView->SetTarget( handler );
	my->_SetTarget(handler);
}



void TabBrowser::SetDirChangeMsg(os::Message* dirchg)
{
	// Setup messages
	dirchg->AddPointer( "browsertab", (void**) this );
    my->_DirView->SetDirChangeMsg( dirchg );
    
}

void TabBrowser::SetFileChangeMsg(os::Message* chg)
{
	// Setup messages
	chg->AddPointer( "browsertab", (void**) this );
    my->_DirView->SetSelChangeMsg( chg );
}

os::String TabBrowser :: GetTitle()
{
	return GetPath();
}

os::String TabBrowser :: GetName()
{
	os::String a = GetPath();

	if( a == "" || a == "/" )
		return a;

	os::Path p( GetPath() );
	return p.GetLeaf();
}

void TabBrowser :: SetPath( const os::String path, bool notify )
{
	my->_DirView->SetPath( path, notify );
	
	os::FSNode cNode( path );
	int32 nStyle;
	if( cNode.ReadAttr( "os::WindowStyle", ATTR_TYPE_INT32,&nStyle, 0, sizeof( int32 ) ) == sizeof( int32 ) )
	{
		if( nStyle < 0 || nStyle > 2 )
			nStyle = 0;
		 my->_DirView->SetView( (IconView::view_type) nStyle );
	}
	else
	{
		Preferences::GetInstance()->GetDefaultView();
	}
}


os::String TabBrowser :: GetPath()
{
	return my->_DirView->GetPath();
}

void TabBrowser :: ReRead()
{
	my->_DirView->ReRead();
}

void TabBrowser :: MakeHistory()
{
	os::String path = GetPath();

	if( my->_Backward.size() == 0 )
		my->_Backward.push( path );
	else if( my->_Backward.size() > 0 && my->_Backward.top() != path )
		my->_Backward.push( path );
}

os::String TabBrowser :: Up()
{
	os::Path current_path( GetPath() );

	current_path.Append( "/.." );

	return current_path;
}

os::String TabBrowser :: Forward()
{
	return os::String( "" );
}

os::String TabBrowser :: Back()
{
	if( my->_Backward.empty() )
		return os::String( "" );
		
	os::String new_path = my->_Backward.top();
	my->_Backward.pop();
	my->_Forward.push( new_path );
	return new_path;
}

os::String TabBrowser::GetFile()
{
	os::Path cPath = os::Path(GetPath());
	int nSelected = my->_DirView->GetLastSelectedIcon();
	
	if (nSelected >= 0)
	{
		DirectoryIconData *pcData = static_cast < DirectoryIconData * >( my->_DirView->GetIconData(nSelected ));
		cPath.Append( pcData->m_zPath.c_str() );	
		return cPath.GetPath();
	}
	else
	{
		return "";
	}
}

void TabBrowser::SetViewMode(IconView::view_type type)
{
	my->_DirView->SetView(type);
	my->_DirView->ReRead();
}

IconView::view_type TabBrowser::GetViewMode()
{
	return my->_DirView->GetView();
}

void TabBrowser::SelectAll()
{
	my->_DirView->SelectAll();
}

void TabBrowser::InvertSelection()
{
	my->_DirView->InvertSelection();
}

void TabBrowser::SetShowHiddenFiles(bool show)
{
	my->_DirView->SetShowHiddenFiles(show);
}

bool TabBrowser::GetShowHiddenFiles()
{
	return my->_DirView->GetShowHiddenFiles();
}

void TabBrowser::ClearLabel()
{
	return my->_DirView->ClearAllSelectedLabels();
}

void TabBrowser::AddLabel(os::Color32_s s)
{
	my->_DirView->AddLabelToSelected(s);
}

int32_t TabBrowser::GetSelectedCount()
{
	return my->_DirView->GetSelectedCount();
}


void TabBrowser::SetZoom(float vZoom)
{
	my->_DirView->SetZoom(vZoom);
}

float TabBrowser::GetZoom()
{
	return my->_DirView->GetZoom();
}




