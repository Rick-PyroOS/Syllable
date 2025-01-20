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

///////////////////////////////////////////////////////////////////////////////
/*																			 */
/* S H A P E    C L A S S  													 */
/*																			 */
///////////////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include <cairo/cairo.h>
#include <graphics/shape.h>

using namespace os;
using namespace graphics;

///////////////////////////////////////////////////////////////////////////////
//
// P R I V A T E
//
///////////////////////////////////////////////////////////////////////////////

class Shape :: _Private
{
public:
	_Private()
	{		
		pcDummySurface = cairo_image_surface_create( CAIRO_FORMAT_RGB24, 1, 1);  // We create a dummy surface so that we can use 


		pcCairo = cairo_create( pcDummySurface );  	// Create an empty Cairo context so that we can store the 
												 	// path somewhere
	}

	cairo_path_t* _GetCairoPath()
	{
		return cairo_copy_path( pcCairo );
	}

	void _CleanUp()
	{
		cairo_surface_destroy( pcDummySurface );
		cairo_destroy( pcCairo );
	}

	cairo_surface_t* pcDummySurface;
	cairo_t* pcCairo;
};

///////////////////////////////////////////////////////////////////////////////
//
// T H E   C L A S S
//
///////////////////////////////////////////////////////////////////////////////

Shape :: Shape()
{
	m = new _Private();
}

Shape:: Shape( const Point& cTo )
{
	m = new _Private();
	MoveTo( cTo );
}


Shape :: ~Shape()
{ 
	m->_CleanUp();
	delete m;
}

void Shape :: Clear()
{
	cairo_new_path( m->pcCairo ); 
}

void Shape :: NewSubShape()
{
	cairo_new_sub_path( m->pcCairo );
}

void Shape :: CloseShape()
{
	cairo_close_path( m->pcCairo );
}

void Shape :: MoveTo( const Point& cTo )
{
	cairo_move_to( m->pcCairo, cTo.x, cTo.y );
}

void Shape :: LineTo( const Point& cTo )
{
	cairo_line_to( m->pcCairo, cTo.x, cTo.y );
}

void Shape :: CurveTo( const Point& cCtrl1, const Point& cCtrl2, const Point& cTo )
{
	cairo_curve_to( m->pcCairo, cCtrl1.x, cCtrl1.y, cCtrl2.x, cCtrl2.y, cTo.x, cTo.y );
}

void Shape :: Arc( const Point& cCenter, const double nRadius, const double nStartAngle, const double nEndAngle )
{
	cairo_arc( m->pcCairo, cCenter.x, cCenter.y, nRadius, nStartAngle, nEndAngle );
}

void Shape :: Rectangle( const Point& cFrom, const Point& cTo )
{
	Rectangle( Rect( cFrom, cTo ) );
}

void Shape :: Rectangle( const os::Rect& cRect )
{
	cairo_rectangle( m->pcCairo, cRect.left, cRect.top, cRect.Width(), cRect.Height() );	
}

void Shape :: RoundRectangle( const Point cP1, const Point cP2, const int nRadius )
{
	RoundRectangle( Rect( cP1, cP2 ), nRadius );
}

void Shape :: RoundRectangle( const Rect cRect, const int nRadius )
{
	if( nRadius == 0 )
		Rectangle( cRect );
	else
	{
		double x = cRect.left;
		double y = cRect.top;
		double w = cRect.Width() + 1;
		double h = cRect.Height() + 1;		

		cairo_new_path( m->pcCairo );
		cairo_move_to( m->pcCairo, x + nRadius, y );
    	cairo_line_to( m->pcCairo, x + w - nRadius, y );
		cairo_curve_to( m->pcCairo, x + w, y, x + w, y, x + w, y + nRadius );
		cairo_line_to( m->pcCairo, x + w, y + h - nRadius );
		cairo_curve_to( m->pcCairo, x + w, y + h, x + w, y + h, x + w - nRadius, y + h );
		cairo_line_to( m->pcCairo, x + nRadius, y + h );
		cairo_curve_to( m->pcCairo, x, y + h, x, y + h, x, y + h - nRadius );
		cairo_line_to( m->pcCairo, x, y + nRadius );
		cairo_curve_to( m->pcCairo, x, y, x, y, x + nRadius, y );
	}
}

void Shape :: NegativeArc( const Point& cCenter, const double nRadius, const double nStartAngle, const double nEndAngle )
{
	cairo_arc_negative( m->pcCairo, cCenter.x, cCenter.y, nRadius, nStartAngle, nEndAngle );
}

void Shape :: MoveRelTo( const Point& cTo )
{
	cairo_rel_move_to( m->pcCairo, cTo.x, cTo.y );
}

void Shape :: LineRelTo( const Point& cTo )
{
	cairo_rel_line_to( m->pcCairo, cTo.x, cTo.y );
}

void Shape :: CurveRelTo( const Point& cCtrl1, const Point& cCtrl2, const Point& cTo )
{
	cairo_rel_curve_to( m->pcCairo, cCtrl1.x, cCtrl1.y, cCtrl2.x, cCtrl2.y, cTo.x, cTo.y );
}

cairo_path_t* Shape :: GetCairoPath() const
{
	return m->_GetCairoPath();
}

Shape& Shape :: operator=( const Shape& cShape )
{
	if( this == &cShape )  // Selfassigment
		return *this;

	if( m != NULL )
	{
		m->_CleanUp();
		delete m;
	}

	m = new _Private();

	cairo_append_path( m->pcCairo, cShape.GetCairoPath() );

	return *this;
}

Shape& Shape :: operator+( const Shape& cShape )
{
	if( this == &cShape )  // Selfassigment, not allowed
		return *this;

	cairo_append_path( m->pcCairo, cShape.GetCairoPath() );

	return *this;
}
