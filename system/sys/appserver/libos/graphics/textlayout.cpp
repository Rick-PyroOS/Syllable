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
/* T E X T L A Y O U T    C L A S S		 									 */
/*																			 */
///////////////////////////////////////////////////////////////////////////////

#include <graphics/textlayout.h>

using namespace os;
using namespace std;
using namespace graphics;

///////////////////////////////////////////////////////////////////////////////
//
// P R I V A T E
//
///////////////////////////////////////////////////////////////////////////////

class TextLayout :: _Private
{
public:

	_Private()
	{	
		psPangoContext = NULL;
		eTextType = TEXT_NONE;
	}

	PangoContext* _GetPangoContext()
	{
		if( psPangoContext == NULL )
		{
			PangoFontMap* fontmap = pango_cairo_font_map_get_default();
			psPangoContext = pango_cairo_font_map_create_context( (PangoCairoFontMap*) fontmap );			
		}

		return psPangoContext;
	}

	enum eTextType eTextType;
	String cText;
	String cAccel;
	
	eVertAlignmentMode eVertAlignment;

	GFont cFont;

	PangoContext* psPangoContext;
	PangoLayout* psLayout;
};

///////////////////////////////////////////////////////////////////////////////
//
// T H E   C L A S S
//
///////////////////////////////////////////////////////////////////////////////
TextLayout :: TextLayout()
{
	m = new _Private();
	
	m->psLayout = pango_layout_new( m->_GetPangoContext() );
	m->eVertAlignment = TEXTLAYOUT_ALIGN_TOP;
}

TextLayout :: ~TextLayout()
{
	g_object_unref( m->psLayout );
	g_object_unref( m->psPangoContext );
	delete m;
}

void TextLayout :: SetText( const String& cText )
{
	m->cText = cText;
	m->eTextType = TEXT_NO_MARKUP;

	pango_layout_set_text( m->psLayout, cText.c_str(), -1 );
}

String TextLayout :: GetText() const
{
	return String( pango_layout_get_text( m->psLayout ) );
}

void TextLayout :: GetTextWithType( enum eTextType& eType, String& cText )
{
	eType = m->eTextType;
	cText = m->cText;
}

void TextLayout :: SetMarkup( const String& cText )
{
	m->cText = cText;
	m->eTextType = TEXT_MARKUP;

	pango_layout_set_markup( m->psLayout, cText.c_str(), -1 );
}

void TextLayout :: SetMarkupWithAccel( const String& cText, String& zAccelMarker, String& zFirstAccelMarker )
{
	m->cText = cText;
	m->eTextType = TEXT_MARKUP_ACCEL;
	m->cAccel = zAccelMarker;

	gunichar* accel_marker = g_utf8_to_ucs4( zAccelMarker.c_str(), -1, NULL, NULL, NULL );
	gunichar accel_char;

	pango_layout_set_markup_with_accel( m->psLayout, cText.c_str(), -1, *accel_marker, &accel_char );

	// Convert unicode to string
	int bytes = g_unichar_to_utf8( accel_char, NULL );
	char t[ bytes ];
	g_unichar_to_utf8( accel_char, t );
	zFirstAccelMarker = String( t );

	// Cleanup
	g_free( accel_marker );
}

void TextLayout :: SetFont( GFont& cFont )
{
	m->cFont = cFont;

	pango_layout_set_font_description( m->psLayout, pango_font_description_from_string( cFont.GetDescription().c_str() ) );
}

GFont TextLayout :: GetFont()
{
	return m->cFont;
}

String TextLayout :: GetFontDescription()
{
	return m->cFont.GetDescription();
}

void TextLayout :: SetWidth( double nWidth )
{
	pango_layout_set_width( m->psLayout, pango_units_from_double( nWidth ) );
}

double TextLayout :: GetWidth()
{
	return pango_units_to_double( pango_layout_get_width( m->psLayout ) );
}

void TextLayout :: SetHeight( double nHeight )
{
	pango_layout_set_height( m->psLayout, pango_units_from_double( nHeight ) );
}

double TextLayout :: GetHeight()
{
	return pango_units_to_double( pango_layout_get_height( m->psLayout ) );
}

void TextLayout :: SetWrap( enum eWrapMode eWrap )
{
	pango_layout_set_wrap( m->psLayout, (PangoWrapMode) eWrap );
}

enum eWrapMode TextLayout :: GetWrap()
{
	return (enum eWrapMode) pango_layout_get_wrap( m->psLayout );
}

bool TextLayout :: IsWrapped()
{
	return pango_layout_is_wrapped( m->psLayout );
}

void TextLayout :: SetEllipsize( enum eEllipsizeMode eEllipsize )
{
	pango_layout_set_ellipsize( m->psLayout, (PangoEllipsizeMode) eEllipsize );
}

enum eEllipsizeMode TextLayout :: GetEllipsize()
{
	return (enum eEllipsizeMode) pango_layout_get_ellipsize( m->psLayout );
}

bool TextLayout :: IsEllipsize()
{
	return pango_layout_is_ellipsized( m->psLayout );
}

void TextLayout :: SetIndent( int nIndent )
{
	pango_layout_set_indent( m->psLayout, nIndent );
}

int TextLayout :: GetIndent()
{
	return pango_layout_get_indent( m->psLayout );
}

void TextLayout :: SetSpacing( int nSpacing )
{
	pango_layout_set_spacing( m->psLayout, nSpacing );
}

int TextLayout :: GetSpacing()
{
	return pango_layout_get_spacing( m->psLayout );
}

void TextLayout :: SetJustify( bool bJustify )
{
	pango_layout_set_justify( m->psLayout, bJustify );
}

bool TextLayout :: GetJustify()
{
	return pango_layout_get_justify( m->psLayout );
}

void TextLayout :: SetAutoDir( bool bAutoDir )
{
	pango_layout_set_auto_dir( m->psLayout, bAutoDir );
}

bool TextLayout :: GetAutoDir()
{
	return pango_layout_get_auto_dir( m->psLayout );
}

void TextLayout :: SetAlignment( eAlignmentMode eAlignment )
{
	pango_layout_set_alignment( m->psLayout, (PangoAlignment) eAlignment );
}

eAlignmentMode TextLayout :: GetAlignment()
{
	return (eAlignmentMode) pango_layout_get_alignment( m->psLayout );
}

void TextLayout :: SetVertAlignment( eVertAlignmentMode eAlignment )
{
	m->eVertAlignment = eAlignment;
}

eVertAlignmentMode TextLayout :: GetVertAlignment()
{
	return m->eVertAlignment;
}

void TextLayout :: SetSingleParagraphMode( bool bMode )
{
	pango_layout_set_single_paragraph_mode( m->psLayout, bMode );
}

bool TextLayout :: GetSingleParagraphMode()
{
	return pango_layout_get_single_paragraph_mode( m->psLayout );
}

Point TextLayout :: GetPixelSize()
{
	int width, height;
	
	pango_layout_get_pixel_size( m->psLayout, &width, &height );

	return Point( width, height );
}

String TextLayout :: GetAccel()
{
	return m->cAccel;
}

PangoLayout* TextLayout :: GetPangoLayout() const
{
	return m->psLayout;
}

