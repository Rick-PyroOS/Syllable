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
/* G F O N T   C L A S S			  										 */
/*																			 */
///////////////////////////////////////////////////////////////////////////////

#include <pango/pango.h>

#include <graphics/gfont.h>

using namespace os;
using namespace graphics;

///////////////////////////////////////////////////////////////////////////////
//
// P R I V A T E
//
///////////////////////////////////////////////////////////////////////////////

class GFont :: _Private
{
public:
	_Private() { psFont = NULL; }

	PangoFontDescription* psFont;
};

///////////////////////////////////////////////////////////////////////////////
//
// T H E   C L A S S
//
///////////////////////////////////////////////////////////////////////////////

GFont :: GFont()
{
	m = new _Private();
	m->psFont = pango_font_description_from_string( "FreeSans 8" );
}

GFont :: GFont( const String cDescription )
{
	m = new _Private();
	m->psFont = pango_font_description_from_string( cDescription.c_str() );
}

GFont :: GFont( const char* zDescription)
{
	m = new _Private();
	m->psFont = pango_font_description_from_string( zDescription );
}

GFont :: ~GFont()
{
	if( m->psFont != NULL )
		pango_font_description_free( m->psFont );

	delete m;
}

void GFont :: SetFamily( const os::String& cFamily )
{
	if( m->psFont != NULL )
		pango_font_description_set_family( m->psFont, cFamily.c_str() );
}

os::String GFont :: GetFamily()
{
	if( m->psFont == NULL )
		throw String( "Font is not set" );

	return String( pango_font_description_get_family( m->psFont ) );
}

enum eFontStyle GFont :: GetStyle()
{
	if( m->psFont == NULL )
		throw String( "Font is not set" );

	return (enum eFontStyle) pango_font_description_get_style( m->psFont );
}

void GFont :: SetStyle( enum eFontStyle eStyle )
{
	if( m->psFont != NULL )
		pango_font_description_set_style( m->psFont, (PangoStyle) eStyle );	
}

enum eFontVariant GFont :: GetVariant()
{
	if( m->psFont == NULL )
		throw String( "Font is not set" );

	return (enum eFontVariant) pango_font_description_get_variant( m->psFont );
}

void GFont :: SetVariant( enum eFontVariant eVariant )
{
	if( m->psFont != NULL )
		pango_font_description_set_variant( m->psFont, (PangoVariant) eVariant );	
}

enum eFontStretch GFont :: GetStretch()
{
	if( m->psFont == NULL )
		throw String( "Font is not set" );

	return (enum eFontStretch) pango_font_description_get_stretch( m->psFont );	
}

void GFont :: SetStretch( enum eFontStretch eStretch )
{
	if( m->psFont != NULL )
		pango_font_description_set_stretch( m->psFont, (PangoStretch) eStretch );	

}

enum eFontWeight GFont :: GetWeight()
{
	if( m->psFont == NULL )
		throw String( "Font is not set" );

	return (enum eFontWeight) pango_font_description_get_weight( m->psFont );	
}

void GFont :: SetWeight( enum eFontWeight eWeight )
{
	if( m->psFont != NULL )
		pango_font_description_set_weight( m->psFont, (PangoWeight) eWeight );	
}
	
void GFont :: SetSize( int nSize )
{
	if( m->psFont != NULL )
		pango_font_description_set_size( m->psFont, nSize );	
}

int GFont :: GetSize()
{
	if( m->psFont == NULL )
		throw String( "Font is not set" );

	return pango_font_description_get_size( m->psFont );	
}

void GFont :: SetAbsoluteSize( double nSize )
{
	if( m->psFont != NULL )
		pango_font_description_set_absolute_size( m->psFont, nSize );	
}

bool GFont :: IsAbsoluteSize()
{
	if( m->psFont == NULL )
		throw String( "Font is not set" );

	return pango_font_description_get_size_is_absolute( m->psFont );	
}

String GFont :: GetDescription() const
{
	if( m->psFont == NULL )
		throw String( "Font is not set" );

	char* desc = pango_font_description_to_string( m->psFont );
	String c( desc );
	g_free( desc );

	return c;
}

PangoFontDescription* GFont :: GetPangoFont() const
{
	if( m->psFont == NULL )
		throw String( "Font is not set" );

	return m->psFont;
}

GFont& GFont :: operator=( const GFont& cFont )
{
	if( this == &cFont )  // Selfassigment
		return *this;

	if( m == NULL )
		m = new _Private();
	else if( m->psFont != NULL )
		pango_font_description_free( m->psFont );

	m->psFont = pango_font_description_copy( cFont.m->psFont );

	return *this;
}
