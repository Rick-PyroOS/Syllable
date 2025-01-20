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
/* C O L O U R   C L A S S		   											 */
/*																			 */
///////////////////////////////////////////////////////////////////////////////

#include <map>
#include <string>

#include <util/exceptions.h>

#include <graphics/colour.h>

// To get access to default colours
#include <gui/gfxtypes.h>
#include <gui/view.h>

using namespace os;
using namespace std;
using namespace graphics;

///////////////////////////////////////////////////////////////////////////////
//
// P R I V A T E
//
///////////////////////////////////////////////////////////////////////////////

class Colour :: _Private
{
private:
	

public:
	
	_Private()
	{		
	}

	int _ConvertHexToInt( const char value )
	{
		int hexval = 0;
		if( value >= '0' && value <= '9' )
			hexval = value - '0';
		else if( value >= 'A' && value <= 'F' )
			hexval = value - 'A' + 10;
		else if( value >= 'a' && value <= 'f')
			hexval = value - 'a' + 10;
		else
			throw os::errno_exception( "Value out of range" );

		return hexval;

	}
	double _ConvertHex2Dbl( const char msb, const char lsb )
	{
		int tmp;

		tmp = ( _ConvertHexToInt( msb )  << 4 ) + _ConvertHexToInt( lsb );

		if( tmp < 0 || tmp > 255 )
			throw os::errno_exception( "Value out of range" );

		return (double) tmp / 255.0f;
	}


	static void _CreatePredefinedColours()
	{
		cColourNames[ String( "aliceblue" ) ] = "#F0F8FF";
		cColourNames[ String( "antiquewhite" ) ] = "#FAEBD7";
		cColourNames[ String( "aqua" ) ] = "#00FFFF";
		cColourNames[ String( "aquamarine" ) ] = "#7FFFD4";
		cColourNames[ String( "azure" ) ] = "#F0FFFF";
		cColourNames[ String( "beige" ) ] = "#F5F5DC";
		cColourNames[ String( "bisque" ) ] = "#FFE4C4";
		cColourNames[ String( "black" ) ] = "#000000";
		cColourNames[ String( "blanchedalmond" ) ] = "#FFEBCD";
		cColourNames[ String( "blue" ) ] = "#0000FF";
		cColourNames[ String( "blueviolet" ) ] = "#8A2BE2";
		cColourNames[ String( "brown" ) ] = "#A52A2A";
		cColourNames[ String( "burlywood" ) ] = "#DEB887";
		cColourNames[ String( "cadetblue" ) ] = "#5F9EA0";
		cColourNames[ String( "chartreuse" ) ] = "#7FFF00";
		cColourNames[ String( "chocolate" ) ] = "#D2691E";
		cColourNames[ String( "coral" ) ] = "#FF7F50";
		cColourNames[ String( "cornflowerblue" ) ] = "#6495ED";
		cColourNames[ String( "cornsilk" ) ] = "#FFF8DC";
		cColourNames[ String( "crimson" ) ] = "#DC143C";
		cColourNames[ String( "cyan" ) ] = "#00FFFF";
		cColourNames[ String( "darkblue" ) ] = "#00008B";
		cColourNames[ String( "darkcyan" ) ] = "#008B8B";
		cColourNames[ String( "darkgoldenRod" ) ] = "#B8860B";
		cColourNames[ String( "darkgray" ) ] = "#A9A9A9";
		cColourNames[ String( "darkgrey" ) ] = "#A9A9A9";
		cColourNames[ String( "darkgreen" ) ] = "#006400";
		cColourNames[ String( "darkkhaki" ) ] = "#BDB76B";
		cColourNames[ String( "darkmagenta" ) ] = "#8B008B";
		cColourNames[ String( "darkolivegreen" ) ] = "#556B2F";
		cColourNames[ String( "darkorange" ) ] = "#FF8C00";
		cColourNames[ String( "darkorchid" ) ] = "#9932CC";
		cColourNames[ String( "darkred" ) ] = "#8B0000";
		cColourNames[ String( "darksalmon" ) ] = "#E9967A";
		cColourNames[ String( "darkseagreen" ) ] = "#8FBC8F";
		cColourNames[ String( "darkslateblue" ) ] = "#483D8B";
		cColourNames[ String( "darkslategray" ) ] = "#2F4F4F";
		cColourNames[ String( "darkslategrey" ) ] = "#2F4F4F";
		cColourNames[ String( "darkturquoise" ) ] = "#00CED1";
		cColourNames[ String( "darkviolet" ) ] = "#9400D3";
		cColourNames[ String( "deeppink" ) ] = "#FF1493";
		cColourNames[ String( "deepskyblue" ) ] = "#00BFFF";
		cColourNames[ String( "dimgray" ) ] = "#696969";
		cColourNames[ String( "dimgrey" ) ] = "#696969";
		cColourNames[ String( "dodgerblue" ) ] = "#1E90FF";
		cColourNames[ String( "firebrick" ) ] = "#B22222";
		cColourNames[ String( "floralwhite" ) ] = "#FFFAF0";
		cColourNames[ String( "forestgreen" ) ] = "#228B22";
		cColourNames[ String( "fuchsia" ) ] = "#FF00FF";
		cColourNames[ String( "gainsboro" ) ] = "#DCDCDC";
		cColourNames[ String( "ghostwhite" ) ] = "#F8F8FF";
		cColourNames[ String( "gold" ) ] = "#FFD700";
		cColourNames[ String( "goldenrod" ) ] = "#DAA520";
		cColourNames[ String( "gray" ) ] = "#808080";
		cColourNames[ String( "grey" ) ] = "#808080";
		cColourNames[ String( "green" ) ] = "#008000";
		cColourNames[ String( "greenyellow" ) ] = "#ADFF2F";
		cColourNames[ String( "honeydew" ) ] = "#F0FFF0";
		cColourNames[ String( "hotpink" ) ] = "#FF69B4";
		cColourNames[ String( "indianred" ) ] = "#CD5C5C";
		cColourNames[ String( "indigo" ) ] = "#4B0082";
		cColourNames[ String( "ivory" ) ] = "#FFFFF0";
		cColourNames[ String( "khaki" ) ] = "#F0E68C";
		cColourNames[ String( "lavender" ) ] = "#E6E6FA";
		cColourNames[ String( "lavenderblush" ) ] = "#FFF0F5";
		cColourNames[ String( "lawngreen" ) ] = "#7CFC00";
		cColourNames[ String( "lemonchiffon" ) ] = "#FFFACD";
		cColourNames[ String( "lightblue" ) ] = "#ADD8E6";
		cColourNames[ String( "lightcoral" ) ] = "#F08080";
		cColourNames[ String( "lightcyan" ) ] = "#E0FFFF";
		cColourNames[ String( "lightgoldenrodyellow" ) ] = "#FAFAD2";
		cColourNames[ String( "lightgray" ) ] = "#D3D3D3";
		cColourNames[ String( "lightgrey" ) ] = "#D3D3D3";
		cColourNames[ String( "lightgreen" ) ] = "#90EE90";
		cColourNames[ String( "lightpink" ) ] = "#FFB6C1";
		cColourNames[ String( "lightsalmon" ) ] = "#FFA07A";
		cColourNames[ String( "lightseagreen" ) ] = "#20B2AA";
		cColourNames[ String( "lightskyblue" ) ] = "#87CEFA";
		cColourNames[ String( "lightSlategray" ) ] = "#778899";
		cColourNames[ String( "lightSlategrey" ) ] = "#778899";
		cColourNames[ String( "lightSteelblue" ) ] = "#B0C4DE";
		cColourNames[ String( "lightyellow" ) ] = "#FFFFE0";
		cColourNames[ String( "lime" ) ] = "#00FF00";
		cColourNames[ String( "limegreen" ) ] = "#32CD32";
		cColourNames[ String( "Linen" ) ] = "#FAF0E6";
		cColourNames[ String( "magenta" ) ] = "#FF00FF";
		cColourNames[ String( "maroon" ) ] = "#800000";
		cColourNames[ String( "mediumaquamarine" ) ] = "#66CDAA";
		cColourNames[ String( "mediumblue" ) ] = "#0000CD";
		cColourNames[ String( "mediumorchid" ) ] = "#BA55D3";
		cColourNames[ String( "mediumpurple" ) ] = "#9370D8";
		cColourNames[ String( "mediumseagreen" ) ] = "#3CB371";
		cColourNames[ String( "mediumslateblue" ) ] = "#7B68EE";
		cColourNames[ String( "mediumspringgreen" ) ] = "#00FA9A";
		cColourNames[ String( "mediumturquoise" ) ] = "#48D1CC";
		cColourNames[ String( "mediumvioletred" ) ] = "#C71585";
		cColourNames[ String( "midnightblue" ) ] = "#191970";
		cColourNames[ String( "mintcream" ) ] = "#F5FFFA";
		cColourNames[ String( "mistyrose" ) ] = "#FFE4E1";
		cColourNames[ String( "moccasin" ) ] = "#FFE4B5";
		cColourNames[ String( "navajowhite" ) ] = "#FFDEAD";
		cColourNames[ String( "navy" ) ] = "#000080";
		cColourNames[ String( "oldlace" ) ] = "#FDF5E6";
		cColourNames[ String( "olive" ) ] = "#808000";
		cColourNames[ String( "olivedrab" ) ] = "#6B8E23";
		cColourNames[ String( "orange" ) ] = "#FFA500";
		cColourNames[ String( "orangered" ) ] = "#FF4500";
		cColourNames[ String( "orchid" ) ] = "#DA70D6";
		cColourNames[ String( "palegoldenrod" ) ] = "#EEE8AA";
		cColourNames[ String( "palegreen" ) ] = "#98FB98";
		cColourNames[ String( "paleturquoise" ) ] = "#AFEEEE";
		cColourNames[ String( "palevioletred" ) ] = "#D87093";
		cColourNames[ String( "papayaWhip" ) ] = "#FFEFD5";
		cColourNames[ String( "peachpuff" ) ] = "#FFDAB9";
		cColourNames[ String( "peru" ) ] = "#CD853F";
		cColourNames[ String( "pink" ) ] = "#FFC0CB";
		cColourNames[ String( "plum" ) ] = "#DDA0DD";
		cColourNames[ String( "powderblue" ) ] = "#B0E0E6";
		cColourNames[ String( "purple" ) ] = "#800080";
		cColourNames[ String( "red" ) ] = "#FF0000";
		cColourNames[ String( "rosybrown" ) ] = "#BC8F8F";
		cColourNames[ String( "royalblue" ) ] = "#4169E1";
		cColourNames[ String( "saddlebrown" ) ] = "#8B4513";
		cColourNames[ String( "salmon" ) ] = "#FA8072";
		cColourNames[ String( "sandybrown" ) ] = "#F4A460";
		cColourNames[ String( "seagreen" ) ] = "#2E8B57";
		cColourNames[ String( "seashell" ) ] = "#FFF5EE";
		cColourNames[ String( "sienna" ) ] = "#A0522D";
		cColourNames[ String( "silver" ) ] = "#C0C0C0";
		cColourNames[ String( "skyblue" ) ] = "#87CEEB";
		cColourNames[ String( "slateblue" ) ] = "#6A5ACD";
		cColourNames[ String( "slategray" ) ] = "#708090";
		cColourNames[ String( "slategrey" ) ] = "#708090";
		cColourNames[ String( "snow" ) ] = "#FFFAFA";
		cColourNames[ String( "springgreen" ) ] = "#00FF7F";
		cColourNames[ String( "steelblue" ) ] = "#4682B4";
		cColourNames[ String( "tan" ) ] = "#D2B48C";
		cColourNames[ String( "teal" ) ] = "#008080";
		cColourNames[ String( "thistle" ) ] = "#D8BFD8";
		cColourNames[ String( "tomato" ) ] = "#FF6347";
		cColourNames[ String( "turquoise" ) ] = "#40E0D0";
		cColourNames[ String( "violet" ) ] = "#EE82EE";
		cColourNames[ String( "wheat" ) ] = "#F5DEB3";
		cColourNames[ String( "white" ) ] = "#FFFFFF";
		cColourNames[ String( "whitesmoke" ) ] = "#F5F5F5";
		cColourNames[ String( "yellow" ) ] = "#FFFF00";
		cColourNames[ String( "yellowgreen" ) ] = "#9ACD32";
	}

	static map< string, string > cColourNames;
};


map< string, string > Colour :: _Private :: cColourNames;

///////////////////////////////////////////////////////////////////////////////
//
// T H E   C L A S S
//
///////////////////////////////////////////////////////////////////////////////

Colour :: Colour()
{
	m = new _Private();

	m_nRed = 0.0f;
	m_nGreen = 0.0f;
	m_nBlue = 0.0f;
	m_nAlpha = 1.0f;
}

Colour :: Colour(	const double nRed, const double nGreen, const double nBlue, const double nAlpha )
{
	if( nRed < 0.0f || nRed > 1.0f || 
		nGreen < 0.0f || nGreen > 1.0f || 
		nBlue < 0.0f || nBlue > 1.0f ||
		nAlpha < 0.0f || nAlpha > 1.0f ) 
		throw os::errno_exception( "Colour out of range" );

	m = new _Private();

	m_nRed = nRed;
	m_nGreen = nGreen;
	m_nBlue = nBlue;
	m_nAlpha = nAlpha;
}

Colour :: Colour( const os::String& cName, const double nAlpha )
{
	String cHexcode;

	if( cName == "" )
		throw os::errno_exception( "No colour name specified" );

	m = new _Private();

	if( m->cColourNames.empty() )
		m->_CreatePredefinedColours();	

	// Replace colour name with hexcode
	if( m->cColourNames.find( cName ) != m->cColourNames.end() )
		cHexcode = m->cColourNames[ cName ];
	else
		cHexcode = cName;
	
	// Make sure the first character is # and the size is #ffffff = 7 chars 
	if( cHexcode[ 0 ] != '#' || cHexcode.size() != 7 )
	{
		printf( "Invalid colour name %s", cName.c_str() );
		throw os::errno_exception( String( "Invalid colour name \"" ) + cName + String( "\"" ) );
	}

	try
	{
		m_nRed = m->_ConvertHex2Dbl( cHexcode[ 1 ], cHexcode[ 2 ] );
		m_nGreen = m->_ConvertHex2Dbl( cHexcode[ 3 ], cHexcode[ 4 ] );
		m_nBlue = m->_ConvertHex2Dbl( cHexcode[ 5 ], cHexcode[ 6 ] );
		m_nAlpha = nAlpha;
	}
	catch(...)
	{
		printf( "Error parsing\n" );
	}
}

Colour :: Colour( default_colour_t nColourType )
{
	m = new _Private();

	os::Color32_s col = os::get_default_color( (default_color_t)  nColourType ); // TODO: This should really be a part of the graphic library
																				 // and remove the brutal casting from one enum to the other

	m_nRed = col.red / 255.0f;
	m_nGreen = col.green / 255.0f;
	m_nBlue = col.blue / 255.0f;

	m_nAlpha = 1.0f;
}

Colour :: ~Colour()
{
	delete m;
}

Colour& Colour :: operator=( const Colour& cColour )
{
	if( this == &cColour )  // Selfassigment
		return *this;

	m_nRed = cColour.m_nRed;
	m_nBlue = cColour.m_nBlue;
	m_nGreen = cColour.m_nGreen;
	m_nAlpha = cColour.m_nAlpha;
	
	return *this;
}
