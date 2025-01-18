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

#ifndef __GRAPHICS_COLOUR_H_
#define __GRAPHICS_COLOUR_H_

#include <util/string.h>

namespace graphics
{

///////////////////////////////////////////////////////////////////////////////
/*																			 */
/* C O L O U R   C L A S S		   											 */
/*																			 */
///////////////////////////////////////////////////////////////////////////////

class Colour
{
public:
	enum default_colour_t
	{
    	COL_NORMAL,
	    COL_SHINE,
    	COL_SHADOW,
	    COL_SEL_WND_BORDER,
    	COL_NORMAL_WND_BORDER,
	    COL_MENU_TEXT,
    	COL_SEL_MENU_TEXT,
	    COL_MENU_BACKGROUND,
    	COL_SEL_MENU_BACKGROUND,
	    COL_SCROLLBAR_BG,
	    COL_SCROLLBAR_KNOB,
    	COL_LISTVIEW_TAB,
	    COL_LISTVIEW_TAB_TEXT,
	    COL_ICON_TEXT,
    	COL_ICON_SELECTED,
		COL_ICON_BG,
		COL_FOCUS,
	    COL_COUNT
	};

	Colour();
	Colour(	const double nRed, const double nGreen, const double nBlue, const double nAlpha = 1.0f );
	Colour( const os::String& cName, const double nAlpha = 1.0f );
	Colour( default_colour_t nColourType );	
	~Colour();

	double m_nRed;
	double m_nGreen;
	double m_nBlue;
	double m_nAlpha;

	Colour& operator=( const Colour& cColour );

private:
	class _Private;
	_Private* m;
};

} // End of namespace

#endif
