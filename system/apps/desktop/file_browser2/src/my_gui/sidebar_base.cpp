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

#include <gui/layoutview.h>
#include <gui/view.h>
#include "sidebar_base.h"
#include "sidebar_title.h"
#include "../common.h"
#include "../messages.h"
using namespace os;


class SideBar :: _Private
{
public:
	_Private() {};

	SideBarTitle* _Title;
	os::View* _SideBarView;
	LayoutNode* _RootNode;
	os::Handler* _Parent;
	
};


class SideBarView : public os::View
{
public:
	SideBarView() : View(os::Rect(),"sidebar_view"){}
	
	void Paint(const os::Rect& cRect)
	{
		FillRect(GetBounds(),os::Color32_s(255,255,255));
		
	}
};

///////////////////////////////////////////////////////////////////////////////
//
// T H E   C L A S S
//
///////////////////////////////////////////////////////////////////////////////
SideBar :: SideBar(os::Handler* parent) : View( Rect(0,0,1,1), "sidebar" )
{
	
	// Create the private class
	_my = new _Private();

	// Create widgets
	_my->_RootNode = new VLayoutNode( "pcSidebarRootNode" );
	_my->_RootNode->SetVAlignment(os::ALIGN_TOP);
	
	LayoutView*	root_view = new LayoutView( GetBounds(), "pcRootView" );
	root_view->SetRoot( _my->_RootNode );
	AddChild( root_view );

	
	// Create title
	_my->_Title = new SideBarTitle("sidebar_title");
	_my->_SideBarView = new SideBarView();
	
	_my->_RootNode->AddChild( _my->_Title, 0.02f );
	_my->_RootNode->AddChild( _my->_SideBarView,1.0f);

	_my->_Parent = parent;
}

SideBar :: ~SideBar()
{
	delete _my;
}


LayoutNode* SideBar :: GetRoot()
{
	return _my->_RootNode;
}

void SideBar::AllAttached()
{
	_my->_Title->SetTarget(this);
}

void SideBar::AddSideBar(SideBarPlugin* plugin)
{
	m_cSidebars.push_back(plugin);
}

void SideBar::HandleMessage(os::Message* pcMessage)
{
	switch (pcMessage->GetCode())
	{
		case MSG_CLOSE_SIDEBAR:
		{
			_my->_Parent->HandleMessage(pcMessage);
			break;
		}
		
		case MSG_SIDEBAR_SWITCH:
		{
			printf("switch sidebar\n");
			break;
		}
		default:
			View::HandleMessage(pcMessage);
	}
	//HandleMessage(pcMessage);
}



