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

#ifndef __SIDEBAR_TITLE_H_
#define __SIDEBAR_TITLE_H_

#include <gui/view.h>
#include <util/string.h>
#include <gui/rect.h>
#include "../syll_gui/radiomenu.h"

class SideBarTitle : public os::View
{
public:
	SideBarTitle( os::String title);
	~SideBarTitle();

	os::Point GetPreferredSize( bool bLargest ) const;
	void SetTarget(os::Handler* handler);
	void SetMenu(os::RadioMenu* menu);	
private:
	class _Private;
	_Private* my;
};

#endif








