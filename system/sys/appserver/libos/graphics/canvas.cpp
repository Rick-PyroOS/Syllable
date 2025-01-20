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
/* C A N V A S  C L A S S		   											 */
/*																			 */
///////////////////////////////////////////////////////////////////////////////

#include <graphics/canvas.h>
#include <graphics/graphics_image.h>

using namespace os;
using namespace graphics;

///////////////////////////////////////////////////////////////////////////////
//
// P R I V A T E
//
///////////////////////////////////////////////////////////////////////////////

class Canvas :: _Private
{
public:
	_Private()
	{		
		_Image = NULL;
	}
	
	void PrepareBuffer( const Rect size )
	{
		delete _Image;

		if( size.Width() > 0 && size.Height() > 0 )
			_Image = new GraphicsImage( IPoint( (int)size.Width() + 1, (int)size.Height() + 1 ), Colour( "white" ) );
		else
			_Image = NULL;
	}

	GraphicsImage* _Image;
};

///////////////////////////////////////////////////////////////////////////////
//
// T H E   C L A S S
//
///////////////////////////////////////////////////////////////////////////////

Canvas :: Canvas( const Rect& cFrame, const String& cTitle,
	  			  uint32 nResizeMask, uint32 nFlags ) : View( cFrame, cTitle, nResizeMask, nFlags )
{
	_m = new _Private();

	_m->PrepareBuffer( GetFrame() );
}

Canvas :: ~Canvas()
{
	delete _m->_Image;
	delete _m;
}

void Canvas :: SetFrame( const Rect& cRect, bool bNotifyServer )
{
	View::SetFrame( cRect, bNotifyServer );
	_m->PrepareBuffer( GetFrame() );
}

void Canvas :: Paint( Graphics* pcGraphics, const os::Rect& cUpdateRect )
{	
}

void Canvas :: Paint( const Rect& cUpdateRect )
{
	if( _m->_Image != NULL )
	{
		// For some reason the update rect seems to be too small??
		Rect u = cUpdateRect;
		u.Resize( 0.0f, 0.0f, 1.0f, 1.0f );
		Paint( _m->_Image, u );

		Bitmap* bmp = _m->_Image->GetImage();
		SetDrawingMode( DM_COPY );
		DrawBitmap( bmp, cUpdateRect, cUpdateRect );
	}
}




