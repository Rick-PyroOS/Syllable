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
/* T R A N S F O R M   C L A S S											 */
/*																			 */
///////////////////////////////////////////////////////////////////////////////

#include <math.h>

#include <graphics/transform.h>

using namespace os;
using namespace graphics;

///////////////////////////////////////////////////////////////////////////////
//
// P R I V A T E
//
///////////////////////////////////////////////////////////////////////////////

class Transform :: _Private
{
public:
	
	_Private()
	{		
	}

	void _Multiply( Transform& transform )
	{
		double t0 = nSx * transform.m->nSx + nShy * transform.m->nShx;
		double t2 = nShx * transform.m->nSx + nSy * transform.m->nShx;
		double t4 = nTx  * transform.m->nSx + nTy * transform.m->nShx + transform.m->nTx;
		nShy = nSx  * transform.m->nShy + nShy * transform.m->nSy;
		nSy  = nShx * transform.m->nShy + nSy  * transform.m->nSy;
		nTy  = nTx  * transform.m->nShy + nTy  * transform.m->nSy + transform.m->nTy;
		nSx  = t0;
		nShx = t2;
		nTx  = t4;
	}

	double nSx, nShy, nShx, nSy, nTx, nTy;

};

///////////////////////////////////////////////////////////////////////////////
//
// T H E   C L A S S
//
///////////////////////////////////////////////////////////////////////////////

Transform :: Transform()
{
	m = new _Private();

	m->nSx = 1.0f; 
	m->nShy = 0.0f; 
	m->nShx = 0.0f;
	m->nSy = 1.0f;
	m->nTx = 0.0;
	m->nTy = 0.0;
}

Transform :: Transform( const double nV0, const double nV1, const double nV2, const double nV3, const double nV4, const double nV5 )
{
	m = new _Private();

	m->nSx = nV0;
	m->nShy = nV1;
	m->nShx = nV2;
	m->nSy = nV3;
	m->nTx = nV4;
	m->nTy = nV5;
}

Transform :: ~Transform()
{
	delete m;
}

bool Transform :: IsIdentity()
{
	return ( m->nSx == 1.0f ) && ( m->nShy == 0.0f ) && ( m->nShx == 0.0f ) && 
		   ( m->nSy == 1.0f ) && ( m->nTx == 0.0f )	&& ( m->nTy == 0.0f );
}

os::Point Transform :: Calculate( const os::Point& cValue )
{
	Point r( cValue.x * m->nSx  + cValue.y * m->nShx + m->nTx, 
			 cValue.x * m->nShy + cValue.y * m->nSy  + m->nTy );
	return r;
}

void Transform :: Translate( const os::Point& cDelta )
{
	Transform t( 1.0f, 0.0f, 0.0f, 1.0f, cDelta.x, cDelta.y );

	m->_Multiply( t );
}

void Transform :: Rotate( const double nAngle )
{
	Transform t( cos( nAngle ), sin( nAngle ), -sin( nAngle ), cos( nAngle ), 0.0f, 0.0f );

	m->_Multiply( t );
}

void Transform :: Scale( const double nX, const double nY )
{
	Transform t( nX, 0.0f, 0.0f, nY, 0.0f, 0.0f );

	m->_Multiply( t );
}

void Transform :: Scale( const double nS )
{
	Transform t( nS, 0.0f, 0.0f, nS, 0.0f, 0.0f );

	m->_Multiply( t );
}

void Transform :: FlipX()
{
	m->nSx  = -m->nSx;
	m->nShy = -m->nShy;
	m->nTx  = -m->nTx;
}

void Transform :: FlipY()
{
	m->nShx = -m->nShx;
	m->nSy  = -m->nSy;
	m->nTy  = -m->nTy;
}

Transform& Transform :: operator=( const Transform& cTransform )
{
	if( this == &cTransform )  // Selfassigment
		return *this;

	m = new _Private();

	m->nSx = cTransform.m->nSx;
	m->nShy = cTransform.m->nShy;
	m->nShx = cTransform.m->nShx;
	m->nSy = cTransform.m->nSy;
	m->nTx = cTransform.m->nTx;
	m->nTy = cTransform.m->nTy;
	
	return *this;
}

void Transform :: SetCoefficents( double nSx, double nShx, double nTx, double nShy, double nSy, double nTy )
{
	m->nSx = nSx; 
	m->nShx = nShx;
	m->nTx = nTx; 
	m->nShy = nShy;
	m->nSy = nSy;
	m->nTy = nTy;
}

void Transform :: GetCoefficents( double& nV0, double& nV1, double& nV2, double& nV3, double& nV4, double& nV5 ) const
{
	nV0 = m->nSx; 
	nV1 = m->nShx;
	nV2 = m->nTx; 
	nV3 = m->nShy;
	nV4 = m->nSy;
	nV5 = m->nTy; 
}

void Transform :: GetCairoMatrix( cairo_matrix_t* matrix ) const
{
	cairo_matrix_init( matrix, m->nSx, m->nShy, m->nShx, m->nSy, m->nTx, m->nTy );
}
