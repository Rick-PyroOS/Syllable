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

#include <cairo/cairo-ps.h>
#include <graphics/graphics_ps.h>

using namespace os;
using namespace graphics;

///////////////////////////////////////////////////////////////////////////////
/*																			 */
/* G R A P H I C S I M A G E  C L A S S										 */
/*																			 */
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//
// P R I V A T E
//
///////////////////////////////////////////////////////////////////////////////

class GraphicsPS :: _Private
{
public:
	_Private()
	{		
	}	

	cairo_surface_t* _Surface;
};


///////////////////////////////////////////////////////////////////////////////
//
// T H E   C L A S S
//
///////////////////////////////////////////////////////////////////////////////

GraphicsPS :: GraphicsPS( String filename, Point size )
{
	m = new _Private();

	// Create the surface
	m->_Surface = cairo_ps_surface_create( filename.c_str(), size.x, size.y );

	GraphicsBackend::SetCairoSurface( m->_Surface );
}

GraphicsPS :: ~GraphicsPS()
{
	delete m;
}
	
void GraphicsPS :: SetSize( Point size )
{
	cairo_ps_surface_set_size( m->_Surface, size.x, size.y );
}

void GraphicsPS :: BeginSetup()
{
	cairo_ps_surface_dsc_begin_setup( m->_Surface );
}

void GraphicsPS :: BeginPageSetup()
{
	cairo_ps_surface_dsc_begin_page_setup( m->_Surface );
}

void GraphicsPS :: Comment( String comment)
{
	cairo_ps_surface_dsc_comment( m->_Surface, comment.c_str() );
}


