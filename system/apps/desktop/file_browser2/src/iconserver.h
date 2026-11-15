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

#ifndef _ICONSERVER_H_
#define _ICONSERVER_H_

#include <gui/image.h>

class IconServer
{
public:
	enum eSize { LARGE, SMALL };

	static IconServer* GetInstance();

	IconServer();
	~IconServer();

	os::Image* GetIcon( const os::String path, enum eSize size );
	os::Image* GetRootIcon( const os::String path, enum eSize size );

private:
	static IconServer* m_theInstance;

	class _Private;
	_Private* _m;
};

#endif
