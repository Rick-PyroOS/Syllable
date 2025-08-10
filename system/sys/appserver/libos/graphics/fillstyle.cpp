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

#include <vector>

#include <graphics/fillstyle.h>

using namespace os;
using namespace std;
using namespace graphics;

///////////////////////////////////////////////////////////////////////////////
/*																			 */
/* F I L L S T Y L E   C L A S S   											 */
/*																			 */
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//
// P R I V A T E
//
///////////////////////////////////////////////////////////////////////////////

class FillStyle :: _Private
{
public:
	class _ColourStop
	{
	public:
		_ColourStop()
		{
		}

		_ColourStop( const double nOffset, const Colour& cColour )
		{
			m_cColour = cColour;
			m_nOffset = nOffset;
		}

		_ColourStop& operator=( const _ColourStop& cColourStop )
		{
			if( this == &cColourStop )  // Selfassigment
				return *this;

			m_cColour = cColourStop.m_cColour;
			m_nOffset = cColourStop.m_nOffset;
			return *this;
		}

		Colour m_cColour;
		double m_nOffset;
	};

	_Private()
	{		
	}

	vector< _ColourStop* > cColourStops;
	
	enum eFillStyleType eFillType;
	Colour cColour;
	Point cStart;
	Point cStop;
	Point cCenter1;
	Point cCenter2;
	double nRadius1;
	double nRadius2;
};

///////////////////////////////////////////////////////////////////////////////
//
// T H E   C L A S S
//
///////////////////////////////////////////////////////////////////////////////

FillStyle :: FillStyle()
{
	m = new _Private();
	m->eFillType = FillNone;
}

FillStyle :: FillStyle( enum eFillStyleType type )
{
	m = new _Private();
	m->eFillType = type;
}

FillStyle :: ~FillStyle()
{
	// Free our used colourstops
	for( uint i = 0 ; i < ColourStopCount() ; i++ )
		delete m->cColourStops[ i ];

	delete m;
}

void FillStyle :: AddColourStop( const double nOffset, const Colour& cColour )
{
	m->cColourStops.push_back( new _Private::_ColourStop( nOffset, cColour ) );
}

void FillStyle :: AddColourStop( const double nOffset, const os::String& cColourName )
{
	_Private::_ColourStop cColourStop( nOffset, Colour( cColourName ) );
	m->cColourStops.push_back( new _Private::_ColourStop( nOffset, Colour( cColourName ) ) );
}

uint FillStyle :: ColourStopCount() const
{
	return m->cColourStops.size();
}

void FillStyle :: GetColourStop( uint nIndex, double& nOffset, Colour& cColour ) const
{
	_Private::_ColourStop* cColourStop = m->cColourStops[ nIndex ];
	nOffset = cColourStop->m_nOffset;
	cColour = cColourStop->m_cColour;
}

FillStyle& FillStyle :: operator=( const FillStyle& cFillStyle )
{
	if( this == &cFillStyle )  // Selfassigment
		return *this;

	delete m;

	m = new _Private();

	// Create duplicate of the colour list
	for( uint i = 0 ; i < cFillStyle.ColourStopCount() ; i++ )
	{
		_Private::_ColourStop* cColourStop = cFillStyle.m->cColourStops[ i ];
		m->cColourStops.push_back( new _Private::_ColourStop( cColourStop->m_nOffset, cColourStop->m_cColour ) );
	}

	m->eFillType = cFillStyle.m->eFillType;
	m->cColour = cFillStyle.m->cColour;
	m->cStart = cFillStyle.m->cStart;
	m->cStop = cFillStyle.m->cStop;
	m->cCenter1 = cFillStyle.m->cCenter1;
	m->cCenter2 = cFillStyle.m->cCenter2;
	m->nRadius1 = cFillStyle.m->nRadius1;
	m->nRadius2 = cFillStyle.m->nRadius2;

	return *this;
}

FillStyle :: eFillStyleType FillStyle :: GetType()
{
	return m->eFillType;
}

void FillStyle :: GetColour( Colour& cColour  )
{
	cColour = m->cColour;
}

void FillStyle :: GetRadialStartStop( Point& cCenter1, double& nRadius1, Point& cCenter2, double& nRadius2 )
{
	cCenter1 = m->cCenter1;
	nRadius1 = m->nRadius1;
	cCenter2 = m->cCenter2;
	nRadius2 = m->nRadius2;
}

void FillStyle :: GetLinearStartStop( Point& cStart, Point& cEnd )
{
	cStart = m->cStart;
	cEnd = m->cStop;
}


cairo_pattern_t* FillStyle :: GetCairoPattern() const
{
	cairo_pattern_t* t = NULL;

	switch( m->eFillType )
	{
		case FillNone:
			break;
		case FillSolid:
			t = cairo_pattern_create_rgba( m->cColour.m_nRed,
		  		 				   	  	   m->cColour.m_nGreen,
									  	   m->cColour.m_nBlue,
									  	   m->cColour.m_nAlpha );
			break;
		case FillRadial:
		{
			t = cairo_pattern_create_radial( m->cCenter1.x, m->cCenter1.y, m->nRadius1,
											 m->cCenter2.x, m->cCenter2.y, m->nRadius2 );

			for( uint i = 0 ; i < ColourStopCount() ; i++ )
			{
				double offset;
				Colour col;
				GetColourStop( i, offset, col );
				cairo_pattern_add_color_stop_rgba( t, offset, col.m_nRed,col.m_nGreen, col.m_nBlue, col.m_nAlpha );
			}

			break;
		}
		case FillLinear:
		{
			t = cairo_pattern_create_linear( m->cStart.x, m->cStart.y, m->cStop.x, m->cStop.y );

			for( uint i = 0 ; i < ColourStopCount() ; i++ )
			{
				double offset;
				Colour col;
				GetColourStop( i, offset, col );
				cairo_pattern_add_color_stop_rgba( t, offset, col.m_nRed,col.m_nGreen, col.m_nBlue, col.m_nAlpha );
			}
			break;
		}
	}

	return t;
}

///////////////////////////////////////////////////////////////////////////////
/*																			 */
/* F I L L S T Y L E S O L I D   C L A S S   								 */
/*																			 */
///////////////////////////////////////////////////////////////////////////////

FillStyleSolid :: FillStyleSolid( const Colour& cColour ) : FillStyle( FillStyle::FillSolid )
{
	m->cColour = cColour;
}

FillStyleSolid :: FillStyleSolid( const os::String cColourName ) : FillStyle( FillStyle::FillSolid )
{
	m->cColour = Colour( cColourName );
}

void FillStyleSolid :: SetColour( const Colour& cPenColour )
{
	m->cColour = cPenColour;
}

void FillStyleSolid :: SetColour( const os::String cColourName )
{
	m->cColour = Colour( cColourName );
}

///////////////////////////////////////////////////////////////////////////////
/*																			 */
/* F I L L S T Y L E L I N E A R   C L A S S  								 */
/*																			 */
///////////////////////////////////////////////////////////////////////////////

FillStyleLinear :: FillStyleLinear( const Point& cStart, const Point& cStop ) : FillStyle( FillStyle::FillLinear )
{
	m->cStart = cStart;
	m->cStop = cStop;
}

///////////////////////////////////////////////////////////////////////////////
/*																			 */
/* F I L L S T Y L E R A D I A L   C L A S S  								 */
/*																			 */
///////////////////////////////////////////////////////////////////////////////


FillStyleRadial :: FillStyleRadial( const Point& cCenter1, const double& nRadius1,
								 	 const Point& cCenter2, const double& nRadius2 ) : FillStyle( FillStyle::FillRadial )
{
	m->cCenter1 = cCenter1;
	m->nRadius1 = nRadius1;
	m->cCenter2 = cCenter2;
	m->nRadius2 = nRadius2;
}

