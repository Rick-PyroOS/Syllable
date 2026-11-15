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


#include <gui/menu.h>
#include <gui/layoutview.h>

#include "../syll_gui/radiomenu.h"
#include "../syll_gui/popupmenu.h"
#include "../common.h"
#include "../messages.h"

#include "sidebar_title.h"
#include "../syll_gui/imagebutton.h"

using namespace os;

///////////////////////////////////////////////////////////////////////////////
//
// 
//
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//
// P R I V A T E
//
///////////////////////////////////////////////////////////////////////////////

class SideBarTitle :: _Private
{
public:
	_Private() {};

	ImageButton* _CloseButton;
	os::PopupMenu* _SidebarMenu;
	os::RadioMenu* _SidebarSubMenu;
	LayoutView*	root_view;
};

///////////////////////////////////////////////////////////////////////////////
//
// T H E   C L A S S
//
///////////////////////////////////////////////////////////////////////////////

SideBarTitle :: SideBarTitle( const String title) : View( Rect(0,0,100,35), os::String().Format("title_%s",title.c_str()) )
{
	// Create the private class
	my = new _Private();

	// Create widgets


	//Sidebars submenu
	my->_SidebarSubMenu = new os::RadioMenu(os::Rect(), "Sidebars", os::ITEMS_IN_COLUMN,os::M_CHECK_MODE);
//	my->_SidebarSubMenu->AddItem(new os::RadioMenuItem("Information",new os::Message(MSG_SIDEBAR_SWITCH)));
//	my->_SidebarSubMenu->AddItem(new os::RadioMenuItem( "Tree view", new os::Message(MSG_SIDEBAR_SWITCH)));


	my->_SidebarMenu = new os::PopupMenu(os::Rect(0,0,70,25),"sidebars_popup","SideBars",my->_SidebarSubMenu);

	my->root_view = new LayoutView( os::Rect(0,0,GetBounds().Width()-2,35), "pcRootView",NULL,os::CF_FOLLOW_ALL);
	BitmapImage* close_icon = LoadImage( "tab_close.png" );
	my->_CloseButton = new ImageButton( Rect(0,0,16,16), "title_close_button", "", new os::Message(MSG_CLOSE_SIDEBAR), close_icon);
		
	LayoutNode*	node = new HLayoutNode( "pcRootNode" );
	node->SetVAlignment(os::ALIGN_BOTTOM);
	node->SetHAlignment(os::ALIGN_LEFT);
	
	os::LayoutNode* node2 = new HLayoutNode("node2");
	node2->AddChild(my->_SidebarMenu);

	
	os::LayoutNode* node3 = new HLayoutNode("node3");
	node3->AddChild(my->_CloseButton);

	node->AddChild(node2);
	node->AddChild(node3);
	node->SameHeight("node2","node3",NULL);

	my->root_view->SetRoot( node );
	AddChild( my->root_view );	

}

SideBarTitle :: ~SideBarTitle()
{
	delete my;
}



void SideBarTitle::SetTarget(os::Handler* handler)
{
	my->_SidebarSubMenu->SetTargetForItems(handler);
	my->_CloseButton->SetTarget(handler);
}

void SideBarTitle::SetMenu(os::RadioMenu* menu)
{
	my->_SidebarSubMenu = menu;
	my->_SidebarMenu->SetMenu(my->_SidebarSubMenu);
}

Point SideBarTitle :: GetPreferredSize( bool bLargest ) const
{
	if( bLargest )
		return Point( 1000, 1000 );
	return os::Point(100,35);
}


























