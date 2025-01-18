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

#ifndef __STYLE_H__
#define __STYLE_H__

#include <graphics/graphics.h>
#include <util/string.h>
#include <gui/rect.h>

class Style
{
public:
	enum StyleState
	{
		STATE_NORMAL, 		// State during normal operation.
		STATE_ACTIVE,		// State of a currently active widget, such as a depressed button.
		STATE_PRELIGHT,		// State indicating that the mouse pointer is over the widget and the widget will respond to mouse clicks. -->
		STATE_SELECTED,		// State of a selected item, such the selected row in a list. -->
		STATE_DISABLED		// State indicating that the widget is unresponsive to user actions. -->
	};

	Style( graphics::Graphics& cGraphics, os::String cName );
	Style( graphics::Graphics& cGraphics, os::String cName, enum StyleState nState );
	~Style();

	void SetState( enum StyleState nState );
	enum StyleState GetState();

	os::Point GetCheckBoxSize();

	void FillBox( const os::Rect& cSize, int nRoundness );
	void DrawCheckBox( const os::Point& cCenter );
	void DrawFrame( os::Rect& cSize, int nRoundness );
	void DrawFilledFrame( os::Rect& cSize, int nRoundness );
	void DrawFocusFrame( const os::Rect& cSize );
	void DrawHSeparator( const os::Point& cP1, const os::Point& cP2 );
	void DrawDefaultText( os::Rect cBounds, os::String cText );
	void DrawText( os::Rect cBounds, os::String cText );
private:
	class _Private;
	_Private* m;
};

#endif



