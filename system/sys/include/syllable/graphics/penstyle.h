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

#ifndef __GRAPHICS_PENSTYLE_H_
#define __GRAPHICS_PENSTYLE_H_

#include <vector>
#include <util/string.h>

#include <graphics/colour.h>
#include <graphics/fillstyle.h>

namespace graphics
{

enum eLineCap
{
	LINE_CAP_BUTT,		// start(stop) the line exactly at the start(end) point
    LINE_CAP_ROUND,		// use a round ending, the center of the circle is the end point
    LINE_CAP_SQUARE		// use squared ending, the center of the square is the end point
};

enum eLineJoin
{
	LINE_JOIN_MITER,	// use a sharp (angled) corner, see cairo_set_miter_limit()
	LINE_JOIN_ROUND, 	// use a rounded join, the center of the circle is the joint point
	LINE_JOIN_BEVEL	 	// use a cut-off join, the join is cut off at half the line width from the joint point
};

///////////////////////////////////////////////////////////////////////////////
/*																			 */
/* P E N S T Y L E   C L A S S	   											 */
/*																			 */
///////////////////////////////////////////////////////////////////////////////

class Penstyle
{
public:
	Penstyle();
	Penstyle( const Colour& cPenColour );
	Penstyle( const os::String cColourName );
	Penstyle( const Colour& cPenColour, const double nLinewidth );
	Penstyle( const os::String cColourName, const double nLinewidth  );
	Penstyle( const Fillstyle& cFillstyle );
	Penstyle( const Fillstyle& cFillstyle, const double nLinewidth  );

	~Penstyle();

	bool IsFillstyleType();
	void GetFillstyle( Fillstyle& cFillstyle ); 

	void SetColour( const Colour& nPenColour );
	void SetColour( const os::String cColourName );
	void GetColour( Colour& cColour );

	void SetLinewidth( const double nWidth );
	double GetLinewidth();

	void SetMiterLimit( const double nMiterLimit );
	double GetMiterLimit();

	void SetDashes( const os::String& cStyle );
	void SetDashes( std::vector< double > cDashList, const double nOffset = 0.0f );
	void ClearDashes();
	void GetDashes( std::vector< double >& cDashList, double& nOffset );

	void SetLineJoin( const enum eLineJoin eLineJoin );
	enum eLineJoin GetLineJoin();

	void SetLineCap( const enum eLineCap eLineCap );
	enum eLineCap GetLineCap();

	Penstyle& operator=( const Penstyle& cPenstyle );
private:
	class _Private;
	_Private* m;
};

} // End of namespace

#endif
