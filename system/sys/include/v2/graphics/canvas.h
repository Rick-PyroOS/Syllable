// Graphics (C)opyright 2008 Jonas Jarvoll
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

#ifndef __CANVAS_H_
#define __CANVAS_H_

#include <gui/view.h>
#include "graphics.h"

namespace graphics
{

///////////////////////////////////////////////////////////////////////////////
/*																			 */
/* C A N V A S   C L A S S		   											 */
/*																			 */
///////////////////////////////////////////////////////////////////////////////

class Canvas : public os::View
{
public:
	Canvas( const os::Rect& cFrame, const os::String& cTitle,
	  		uint32 nResizeMask = os::CF_FOLLOW_LEFT | os::CF_FOLLOW_TOP,
	  		uint32 nFlags = os::WID_WILL_DRAW | os::WID_CLEAR_BACKGROUND );
	~Canvas();

	void SetFrame( const os::Rect& cRect, bool bNotifyServer = true );
	virtual void Paint( Graphics* pcGraphics, const os::Rect& cUpdateRect );

private:
	void Paint( const os::Rect& cUpdateRect );

	class _Private;
	_Private* _m;
};

} // End of namespace

#endif
