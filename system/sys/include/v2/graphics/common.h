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

#ifndef __GRAPHICS_COMMON_H_
#define __GRAPHICS_COMMON_H_

namespace graphics
{

enum eAntiAlias
{
	ANTIALIAS_DEFAULT,	 // Use the default antialiasing for the subsystem and target device
	ANTIALIAS_NONE,	 	 // Use a bilevel alpha mask
	ANTIALIAS_GRAY,	 	 // Perform single-color antialiasing (using shades of gray for black text on a white background, for example).
	ANTIALIAS_SUBPIXEL	 // Perform antialiasing by taking advantage of the order of subpixel elements on devices such as LCD panels
};

enum eOperator
{
	OPERATOR_CLEAR,

    OPERATOR_SOURCE,
    OPERATOR_OVER,
    OPERATOR_IN,
    OPERATOR_OUT,
    OPERATOR_ATOP,

    OPERATOR_DEST,
    OPERATOR_DEST_OVER,
    OPERATOR_DEST_IN,
    OPERATOR_DEST_OUT,
    OPERATOR_DEST_ATOP,

    OPERATOR_XOR,
    OPERATOR_ADD,
    OPERATOR_SATURATE
};

} // End of namespace

#endif
