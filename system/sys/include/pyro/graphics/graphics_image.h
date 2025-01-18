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

#ifndef __GRAPHICS_IMAGE_H_
#define __GRAPHICS_IMAGE_H_

#include <atheos/types.h>
#include <gui/bitmap.h>

#include <graphics/graphics_backend.h>

namespace graphics
{

///////////////////////////////////////////////////////////////////////////////
/*																			 */
/* G R A P H I C S I M A G E  C L A S S										 */
/*																			 */
///////////////////////////////////////////////////////////////////////////////

class GraphicsImage : public GraphicsBackend
{
public:
	GraphicsImage( os::IPoint size );
	GraphicsImage( os::IPoint size, const Colour& cColour );
	GraphicsImage( os::Bitmap* bmp );
	GraphicsImage( int nWidth, int nHeight, uint8* pRaster, os::color_space eColorSpace, int nBytesPerLine );

	~GraphicsImage();

	os::Bitmap* GetImage();

	static GraphicsImage* CreateFromFile( os::String cFilename, os::String cMimeType );
	static GraphicsImage* CreateFromResource( os::String cResourceName );
	static GraphicsImage* CreateFromIconset( os::String cFilename, os::IPoint cSize);

private:
	class _Private;
	_Private* m;
};

} // End of namespace

#endif
