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

#ifndef __GRAPHICS_PS_H_
#define __GRAPHICS_PS_H_

#include "graphics_backend.h"

namespace graphics
{

///////////////////////////////////////////////////////////////////////////////
/*																			 */
/* G R A P H I C S I M A G E  C L A S S										 */
/*																			 */
///////////////////////////////////////////////////////////////////////////////

class GraphicsPS : public GraphicsBackend
{
public:
	GraphicsPS( os::String filename, os::Point size );
	~GraphicsPS();
	
	void SetSize( os::Point size );

	void BeginSetup();
	void BeginPageSetup();
	void Comment( os::String comment);



private:
	class _Private;
	_Private* m;
};

} // End of namespace

#endif
