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


#ifndef __SIDEBAR_TREE_H_
#define __SIDEBAR_TREE_H_

#include "../my_gui/sidebar_base.h"

class SideBarTree : public SideBarBase
{
public:
	SideBarTree();
	~SideBarTree();

	virtual void SelectFile( os::String path );
	void AllAttached();
	void HandleMessage(os::Message*);
public:
	enum
	{
		TREE_SELECT,
		TREE_EXPAND,
	};
private:	
	class _Private;
	_Private* my;
};

#endif







