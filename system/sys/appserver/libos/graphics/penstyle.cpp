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
/* P E N S T Y L E   C L A S S   											 */
/*																			 */
///////////////////////////////////////////////////////////////////////////////

#include <cairo/cairo.h>
#include <map>

#include <graphics/penstyle.h>
#include <util/exceptions.h>

using namespace os;
using namespace std;
using namespace graphics;

///////////////////////////////////////////////////////////////////////////////
//
// P R I V A T E
//
///////////////////////////////////////////////////////////////////////////////

	
class Penstyle :: _Private
{
	
public:
	_Private()
	{		
		bUseFillstyle = false;
		cColour = Colour( 1.0f, 0.0f, 0.0f, 1.0f );
		nLinewidth = 2.0f;
		eLineCap = CAIRO_LINE_CAP_BUTT;
		eLineJoin = CAIRO_LINE_JOIN_MITER;
		nMiterLimit = 1.0f;
		nDashOffset = 0.0f;	
	}

	void _CreatePredefinedStyles()
	{
		vector< double > cList;

		// Solid (just an empty vector)
		cStyles[ "solid" ] = cList;

		// Dashed
		cList.push_back( 12.0f );
		cList.push_back( 6.0f );
		cStyles[ "dashed" ] = cList;

		// Dot
		cList.clear();
		cList.push_back( 0.1f );
		cList.push_back( 6.2f );
		cStyles[ "dotted" ] = cList;

		// DashDot
		cList.clear();
		cList.push_back( 12.0f );
		cList.push_back( 5.95f );
		cList.push_back( 0.1f );
		cList.push_back( 5.95f );
		cStyles[ "dashdotted" ] = cList;

		// Divide
		cList.clear();
		cList.push_back( 12.0f );
		cList.push_back( 5.9f );
		cList.push_back( 0.15f );
		cList.push_back( 5.9f );
		cList.push_back( 0.15f );
		cList.push_back( 5.9f );
		cStyles[ "divide" ] = cList;

		// Center
		cList.clear();
		cList.push_back( 32.0f );
		cList.push_back( 6.0f );
		cList.push_back( 6.0f );
		cList.push_back( 6.0f );
		cStyles[ "divide" ] = cList;

		// Border
		cList.clear();
		cList.push_back( 12.0f );
		cList.push_back( 6.0f );
		cList.push_back( 12.0f );
		cList.push_back( 5.95f );
		cList.push_back( 0.1f );
		cList.push_back( 5.95f );
		cStyles[ "border" ] = cList;
	}

	bool bUseFillstyle;
	Fillstyle cFillstyle;

	Colour cColour;
	double nLinewidth;
	cairo_line_cap_t eLineCap;
	cairo_line_join_t eLineJoin;
	double nMiterLimit;
	double nDashOffset;
	vector< double > cDashes;

	static map< string, vector< double > > cStyles;
};

map< string, vector< double > > Penstyle :: _Private :: cStyles;

///////////////////////////////////////////////////////////////////////////////
//
// T H E   C L A S S
//
///////////////////////////////////////////////////////////////////////////////

Penstyle :: Penstyle()
{
	m = new _Private();
}

Penstyle :: Penstyle( const Colour& nPenColour )
{
	m = new _Private();
	m->cColour = nPenColour;
}

Penstyle :: Penstyle( const String cColourName )
{
	m = new _Private();

	m->cColour = Colour( cColourName );
}

Penstyle :: Penstyle( const Colour& nPenColour, const double nLinewidth )
{
	m = new _Private();
	m->cColour = nPenColour;
	m->nLinewidth = nLinewidth;
}

Penstyle :: Penstyle( const String cColourName, const double nLinewidth )
{
	m = new _Private();
	m->cColour = Colour( cColourName );
	m->nLinewidth = nLinewidth;
}

Penstyle :: Penstyle( const Fillstyle& cFillstyle )
{
	m = new _Private();
	m->bUseFillstyle = true;
	m->cFillstyle = cFillstyle;
}

Penstyle :: Penstyle( const Fillstyle& cFillstyle, const double nLinewidth  )
{
	m = new _Private();
	m->bUseFillstyle = true;
	m->cFillstyle = cFillstyle;
	m->nLinewidth = nLinewidth;
}

Penstyle :: ~Penstyle()
{
	delete m;
}

bool Penstyle :: IsFillstyleType()
{
	return m->bUseFillstyle;
}

void Penstyle :: GetFillstyle( Fillstyle& cFillstyle )
{
	cFillstyle = m->cFillstyle;
}

void Penstyle :: SetColour( const Colour& nPenColour )
{
	m->cColour = nPenColour;
}

void Penstyle :: SetColour( const String cColourName )
{
	m->cColour = Colour( cColourName );	
}

void Penstyle :: GetColour( Colour& cColour )
{
	cColour = m->cColour;
}

void Penstyle :: SetLinewidth( const double nWidth )
{
	m->nLinewidth = nWidth;
}

double Penstyle :: GetLinewidth()
{
	return m->nLinewidth;
}

void Penstyle :: SetMiterLimit( const double nMiterLimit )
{
	m->nMiterLimit = nMiterLimit;
}

double Penstyle :: GetMiterLimit()
{
	return m->nMiterLimit;
}

void Penstyle :: SetDashes( const String& cStyle )
{
	if( cStyle == "" )
		throw os::errno_exception( "No line style specified" );

	if( m->cStyles.empty() )
		m->_CreatePredefinedStyles();	

	// Replace colour name with hexcode
	if( m->cStyles.find( cStyle ) == m->cStyles.end() )
		throw os::errno_exception( String( "Invalid line style name \"" ) + cStyle + String( "\"" ) );

	SetDashes( m->cStyles[ cStyle ], 0.0f );
}

void Penstyle :: SetDashes( vector< double > cDashList, const double nOffset )
{
	if( cDashList.empty() )
		ClearDashes();
	else
	{
		m->nDashOffset = nOffset;
		m->cDashes = cDashList;
	}
}

void Penstyle :: ClearDashes()
{
	m->nDashOffset = 0.0f;
	m->cDashes.clear();
}

void Penstyle :: GetDashes( vector< double >& cDashList, double& nOffset )
{
	cDashList = m->cDashes;
	nOffset = m->nDashOffset;
}

void Penstyle :: SetLineJoin( const enum eLineJoin eLineJoin )
{
	// Translate to Cairo
	switch( eLineJoin )
	{
		case LINE_JOIN_MITER:	
			m->eLineJoin = CAIRO_LINE_JOIN_MITER;
			break;
    	case LINE_JOIN_ROUND:
			m->eLineJoin = CAIRO_LINE_JOIN_ROUND;
			break;
    	case LINE_JOIN_BEVEL:	
			m->eLineJoin = CAIRO_LINE_JOIN_BEVEL;
			break;
		default:
			throw String( "Invalid line join" );
	}
}

enum eLineJoin Penstyle :: GetLineJoin()
{
	// Translate from Cairo
	switch( m->eLineJoin )
	{
		case CAIRO_LINE_JOIN_MITER:
			return LINE_JOIN_MITER;
    	case CAIRO_LINE_JOIN_ROUND:
			return LINE_JOIN_ROUND;
    	case CAIRO_LINE_JOIN_BEVEL:
			return LINE_JOIN_BEVEL;
	}

	throw String( "Invalid line join" );
}

void Penstyle :: SetLineCap( const enum eLineCap eLineCap )
{
	// Translate to Cairo
	switch( eLineCap )
	{
		case LINE_CAP_BUTT:	
			m->eLineCap = CAIRO_LINE_CAP_BUTT;
			break;
    	case LINE_CAP_ROUND:
			m->eLineCap = CAIRO_LINE_CAP_ROUND;
			break;
    	case LINE_CAP_SQUARE:	
			m->eLineCap = CAIRO_LINE_CAP_SQUARE;
			break;
		default:
			throw String( "Invalid line cap" );
	}
}

enum eLineCap Penstyle :: GetLineCap()
{
	// Translate from Cairo
	switch( m->eLineCap )
	{
		case CAIRO_LINE_CAP_BUTT:	
			return LINE_CAP_BUTT;
    	case CAIRO_LINE_CAP_ROUND:
			return LINE_CAP_ROUND;
    	case CAIRO_LINE_CAP_SQUARE:	
			return LINE_CAP_SQUARE;
		
	}

	throw String( "Invalid line cap" );
}

Penstyle& Penstyle :: operator=( const Penstyle& cPenstyle )
{
	if( this == &cPenstyle )  // Selfassigment
		return *this;

	if( m != NULL )
		delete m;

	m = new _Private();

	m->cColour = cPenstyle.m->cColour;
	m->nLinewidth = cPenstyle.m->nLinewidth;
	m->eLineCap = cPenstyle.m->eLineCap;
	m->eLineJoin = cPenstyle.m->eLineJoin;
	m->nMiterLimit = cPenstyle.m->nMiterLimit;
	m->nDashOffset = cPenstyle.m->nDashOffset;
	m->cDashes = cPenstyle.m->cDashes;

	return *this;
}
