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

#ifndef _LAYOUTER_H_
#define _LAYOUTER_H_

#include <gui/view.h>
#include "../include/plugin.h"

class Layouter : public os::View
{
public:
	static Layouter* GetInstance();

	Layouter();
	~Layouter();
	
	void SetFrame( const os::Rect& cFrame, bool bNotifyServer = true );

	void AllAttached();
	void AttachedToWindow();
	
	void HandleMessage( os::Message* pcMessage );
	void AddPlugin(Plugin*);
private:
	static Layouter* m_theInstance;

	class _Private;
	_Private* my;
};

#endif


