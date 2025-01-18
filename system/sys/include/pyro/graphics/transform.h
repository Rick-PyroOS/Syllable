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

#ifndef __GRAPHICS_TRANSFORM_H_
#define __GRAPHICS_TRANSFORM_H_

#include <cairo/cairo.h>
#include <gui/point.h>

namespace graphics
{

///////////////////////////////////////////////////////////////////////////////
/*																			 */
/* T R A N S F O R M   C L A S S	  										 */
/*																			 */
///////////////////////////////////////////////////////////////////////////////

class Transform
{
public:
	Transform();
	Transform( const double nV0, const double nV1, const double nV2, const double nV3, const double nV4, const double nV5 );
	~Transform();

	bool IsIdentity();

	os::Point Calculate( const os::Point& cValue );

	void Translate( const os::Point& cDelta );
	void Rotate( const double nAngle );
	void Scale( const double nX, const double nY );
	void Scale( const double nS );
	void FlipX();
	void FlipY();

	void SetCoefficents( double nSx, double nShx, double nTx, double nShy, double nSy, double nTy );
	void GetCoefficents( double& nV0, double& nV1, double& nV2, double& nV3, double& nV4, double& nV5 ) const;
	void GetCairoMatrix( cairo_matrix_t* matrix ) const;

	Transform& operator=( const Transform& cTransform );

private:
	class _Private;
	_Private* m;	
};

} // End of namespace

#endif


