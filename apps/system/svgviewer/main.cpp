// CTest (C)opyright 2008 Jonas Jarvoll
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

#include "main.h"
#include <util/exceptions.h>
#include <util/resources.h>
#include <gui/image.h>
#include <gui/imageview.h>

extern "C"
{
	#include <librsvg/rsvg.h>
	#include <librsvg/rsvg-cairo.h>
}

using namespace os;
using namespace std;


int main( int argc, char* argv[] )
{
	if( argc != 2 )
	{
		printf( "Usage: %s <filename>\n", argv[ 0 ] );
		return 0;

	}

 	SvgViewer* app = new SvgViewer( String( argv[ 1 ] ) );
	app->Run();

	return( 0 );
}

AppWindow* SvgViewer :: m_AppWindow = NULL;


SvgViewer :: SvgViewer( String cFilename ) : Application( "application/x-SvgViewer" )
{
	GError* pError = NULL;
	RsvgHandle* psHandle = NULL;

	// Init the librsvg libary
	rsvg_init();

	// Trying to load the file
	psHandle = rsvg_handle_new_from_file( cFilename.c_str(), &pError );
	
	if( psHandle == NULL )
		throw errno_exception( "Error while loading file\n" );
	
	// Get dimension of the SVG
	RsvgDimensionData sDimension;
	rsvg_handle_get_dimensions( psHandle, &sDimension );
	printf( "Size: %d x %d\n", sDimension.width, sDimension.height );
	
	// Create bitmap of this size to hold the SVG image
	Bitmap* pcBitmap = new Bitmap( (int)sDimension.width, (int)sDimension.height, CS_RGBA32 );
	
	// Create the surface
	cairo_surface_t* psSurface = cairo_image_surface_create_for_data(
				                       	pcBitmap->LockRaster(),
                        				CAIRO_FORMAT_ARGB32,
                        				(int)sDimension.width, (int)sDimension.height,
                        				(int)pcBitmap->GetBytesPerRow() );
	cairo_t* psCairo = cairo_create( psSurface );

	// Here the actual rendering takes place
	rsvg_handle_render_cairo( psHandle, psCairo );

	// Do some clean up
	cairo_destroy( psCairo );
	cairo_surface_destroy( psSurface );
	
	m_AppWindow = new AppWindow( pcBitmap, Rect( 100, 100, 400, 275 ) );
	m_AppWindow->Show();
	m_AppWindow->MakeFocus();
}

AppWindow :: AppWindow( Bitmap* pcBitmap, const Rect& cFrame ) : Window( cFrame, "main_window", "SvgViewer" )
{
	BitmapImage* pcImage = new BitmapImage();
	pcImage->SetBitmapData( pcBitmap->LockRaster(), IPoint( (int)pcBitmap->GetBounds().Width() + 1, (int)pcBitmap->GetBounds().Height() ), pcBitmap->GetColorSpace());
	pcBitmap->UnlockRaster();

	ImageView* pcView = new ImageView( GetBounds(), "", pcImage );
	AddChild( pcView );
	
}

AppWindow :: ~AppWindow()
{
}

