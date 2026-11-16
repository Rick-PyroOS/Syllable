// EFileBrowser (C)opyright 2007 Jonas Jarvoll
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
#include <gui/frameview.h>
#include <gui/treeview.h>
#include <gui/stringview.h>
#include <storage/directory.h>
#include <sys/stat.h>

#include "sidebar_tree.h"

using namespace os;

///////////////////////////////////////////////////////////////////////////////
//
// P R I V A T E
//
///////////////////////////////////////////////////////////////////////////////

class SideBarTree :: _Private
{
public:
	_Private(const os::Rect& bounds) 
	{
		_Layout = new os::LayoutView(bounds,"main_view");
		
		os::VLayoutNode* root = new os::VLayoutNode("root");
		_TreeView = new os::TreeView(os::Rect(),"tree");
		_TreeView->SetDrawTrunk(false);
		_TreeView->SetDrawExpanderBox(true);
		_TreeView->InsertColumn("Places",100);
		_TreeView->SetExpandedMessage(new os::Message(SideBarTree::TREE_EXPAND));
		_TreeView->SetSelChangeMsg(new os::Message(SideBarTree::TREE_SELECT));
		root->AddChild(_TreeView);
		_Layout->SetRoot(root);
			
		
		_Iterate("/","FileSystem");
		_Iterate(os::String(getenv("HOME")) + os::String("/"),"Home");
	};

	os::TreeView* _TreeView;
	os::LayoutView* _Layout;
	
private:
	void _Iterate(os::String cPath, os::String cCookie="",int nLevel=1)
	{
		os::TreeViewStringNode* node = new os::TreeViewStringNode();
		node->SetCookie(cPath);
		node->AppendString(cCookie);	
		node->SetIndent(nLevel);
		_TreeView->InsertNode(node);
		
		os::Directory* pcDir = new Directory(cPath);
		os::String entry;
		while (pcDir->GetNextEntry(&entry))
		{
			if (entry != "." and entry != "..")
			{
				os::String path = cPath + entry;
				struct stat status;
				stat(path.c_str(),&status);
			
				if (S_ISDIR(status.st_mode))
				{
					os::TreeViewStringNode* node = new os::TreeViewStringNode();
					node->AppendString(path.substr(cPath.size(),path.size()));
					node->SetIndent(nLevel+1);
					node->SetCookie(path);
					_TreeView->InsertNode(node);					
				}
			}
		}
		
	}
};

///////////////////////////////////////////////////////////////////////////////
//
// T H E   C L A S S
//
///////////////////////////////////////////////////////////////////////////////
SideBarTree :: SideBarTree() : SideBarBase( "Tree view" )
{
	// Create the private class
	my = new _Private(GetBounds());
	AddChild(my->_Layout);

}

SideBarTree :: ~SideBarTree()
{
	delete my;
}

void SideBarTree::AllAttached()
{
	my->_TreeView->SetTarget(this);
}

void SideBarTree::HandleMessage(os::Message* pcMessage)
{
	switch (pcMessage->GetCode())
	{
		case TREE_EXPAND:
			printf("%s\n",GetTarget->GetName().c_str());
			break;
		case TREE_SELECT:
			printf("this is another one\n\n\n");
			break;
		default:
			View::HandleMessage(pcMessage);
	}
}

void SideBarTree :: SelectFile( String path )
{
}

