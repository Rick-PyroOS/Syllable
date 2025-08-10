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

#include <graphics/graphics_image.h>
#include <storage/file.h>
#include <gui/image.h>
#include <util/resources.h>

using namespace os;
using namespace graphics;

///////////////////////////////////////////////////////////////////////////////
/*																			 */
/* G R A P H I C S I M A G E  C L A S S										 */
/*																			 */
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//
// S T A T I C
//
///////////////////////////////////////////////////////////////////////////////

GraphicsImage* GraphicsImage :: CreateFromFile( String cFilename, String cMimeType )
{
	File cFile( "picture.png" );
	BitmapImage *pcImage;
	
	pcImage = new BitmapImage( &cFile );

    return new GraphicsImage( pcImage->LockBitmap() );
}

GraphicsImage* GraphicsImage :: CreateFromResource( String cResourceName )
{
	File cSelf( open_image_file( get_image_id() ) );
    Resources cCol( &cSelf );
    ResStream* pcStream = cCol.GetResourceStream( cResourceName );
    BitmapImage* pcImage = new BitmapImage( pcStream );
    delete( pcStream );

    return new GraphicsImage( pcImage->LockBitmap() );
}

GraphicsImage* GraphicsImage :: CreateFromIconset( String cFilename, IPoint cSize)
{
	dbprintf( "GraphicsImage::CreateFromIconset not yet implemented\n" );

	return new GraphicsImage( cSize );
}



///////////////////////////////////////////////////////////////////////////////
//
// P R I V A T E
//
///////////////////////////////////////////////////////////////////////////////

class GraphicsImage :: _Private
{
public:
	_Private()
	{	
		pcBitmap = NULL;
	}

	Bitmap* pcBitmap;
};


///////////////////////////////////////////////////////////////////////////////
//
// T H E   C L A S S
//
///////////////////////////////////////////////////////////////////////////////

GraphicsImage :: GraphicsImage( Bitmap* bmp ) : GraphicsBackend()
{
	m = new _Private();

	m->pcBitmap = bmp;

	// Create the surface
	cairo_surface_t* surface = cairo_image_surface_create_for_data(
				                        m->pcBitmap->LockRaster(),
                        				CAIRO_FORMAT_ARGB32,
                        				(int)bmp->GetBounds().Width(), (int)bmp->GetBounds().Height(),
                        				(int)m->pcBitmap->GetBytesPerRow() );
	m->pcBitmap->UnlockRaster();

	GraphicsBackend::SetCairoSurface( surface );
}

GraphicsImage :: GraphicsImage( IPoint size ) : GraphicsBackend()
{
	m = new _Private();

	// Create the image
	m->pcBitmap = new Bitmap( size.x, size.y, CS_RGBA32 );

	// Create the surface
	cairo_surface_t* surface = cairo_image_surface_create_for_data(
				                        m->pcBitmap->LockRaster(),
                        				CAIRO_FORMAT_ARGB32,
                        				size.x, size.y,
                        				(int)m->pcBitmap->GetBytesPerRow() );
	m->pcBitmap->UnlockRaster();

	GraphicsBackend::SetCairoSurface( surface );
}

GraphicsImage :: GraphicsImage( IPoint size, const Colour& cColour ) : GraphicsBackend()
{
	m = new _Private();

	// Create the image
	m->pcBitmap = new Bitmap( size.x, size.y, CS_RGBA32 );

	// Create the surface
	cairo_surface_t* surface = cairo_image_surface_create_for_data(
				                        m->pcBitmap->LockRaster(),
                        				CAIRO_FORMAT_ARGB32,
                        				size.x, size.y,
                        				(int)m->pcBitmap->GetBytesPerRow() );
	m->pcBitmap->UnlockRaster();

	GraphicsBackend::SetCairoSurface( surface );

	PenStyle p( cColour );
	SetPen( p );
	FillRectangle( Rect( 0, 0, size.x + 1, size.y + 1 ) );
}

GraphicsImage :: GraphicsImage( int nWidth, int nHeight, uint8* pRaster, color_space eColorSpace, int nBytesPerLine )
{
	cairo_surface_t* surface;

	m->pcBitmap = NULL;	// Bitmap is not used!

dbprintf( "Creating GrapgicsImage \n" );
	switch( eColorSpace )
	{
		case CS_RGBA32:
			surface = cairo_image_surface_create_for_data( pRaster, CAIRO_FORMAT_ARGB32, nWidth, nHeight, nBytesPerLine);
			break;
		case CS_RGB32:
		case CS_RGB24:
			surface = cairo_image_surface_create_for_data( pRaster, CAIRO_FORMAT_ARGB32, nWidth, nHeight, nBytesPerLine);
			break;
		default:
			dbprintf( "GraphicsImage::GraphicsImage() Unsupported color space\n" );
			break;
	}

	if( surface == NULL )
		dbprintf( "GraphicsImage::GraphicsImage() Unable to create cairo surface\n" );

	GraphicsBackend::SetCairoSurface( surface );
}

GraphicsImage :: ~GraphicsImage()
{
	delete m->pcBitmap;
	delete m;
}

Bitmap* GraphicsImage :: GetImage()
{
	return m->pcBitmap;
}
