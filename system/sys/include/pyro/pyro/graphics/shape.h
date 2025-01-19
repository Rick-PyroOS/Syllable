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

#ifndef __GRAPHICS_SHAPE_H_
#define __GRAPHICS_SHAPE_H_

#include <cairo/cairo.h>
#include <gui/point.h>
#include <gui/rect.h>

namespace graphics
{

///////////////////////////////////////////////////////////////////////////////
/*																			 */
/* S H A P E   C L A S S					  								 */
/*																			 */
///////////////////////////////////////////////////////////////////////////////

class Shape
{
public:
	Shape();
	Shape( const os::Point& cTo );
	~Shape();

	void Clear();

	void NewSubShape();
	void CloseShape();

	void MoveTo( const os::Point& cTo );
	void LineTo( const os::Point& cTo );
	void CurveTo( const os::Point& cCtrl1, const os::Point& cCtrl2, const os::Point& cTo );
	void Arc( const os::Point& cCenter, const double nRadius, const double nStartAngle, const double nEndAngle );
	void NegativeArc( const os::Point& cCenter, const double nRadius, const double nStartAngle, const double nEndAngle );
	void Rectangle( const os::Rect& cRect );
	void Rectangle( const os::Point& cFrom, const os::Point& cTo );
	void RoundRectangle( const os::Point cP1, const os::Point cP2, const int nRadius = 5 );
	void RoundRectangle( const os::Rect cRect, const int nRadius = 5  );

	void MoveRelTo( const os::Point& cTo );
	void LineRelTo( const os::Point& cTo );
	void CurveRelTo( const os::Point& cCtrl1, const os::Point& cCtrl2, const os::Point& cTo );
		
	cairo_path_t* GetCairoPath() const;

	Shape& operator=( const Shape& cShape );
	Shape& operator+( const Shape& cShape );
private:
	class _Private;
	_Private* m;
};

} // End of namespace

#endif

