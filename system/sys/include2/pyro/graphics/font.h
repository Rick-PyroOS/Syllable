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

#ifndef __GRAPHICS_FONT_H_
#define __GRAPHICS_FONT_H_

#include <util/string.h>
#include <pango/pango.h>

enum eFontStyle
{
	FONT_STYLE_NORMAL, 	// the font is upright.
	FONT_STYLE_OBLIQUE,	// the font is slanted, but in a roman style.
	FONT_STYLE_ITALIC 	// the font is slanted in an italic style.
};

enum eFontVariant
{
	FONT_VARIANT_NORMAL,	// A normal font
	FONT_VARIANT_SMALL,		// font with the lower case characters replaced by smaller variants of the capital characters.
};

enum eFontWeight
{
	FONT_WEIGHT_ULTRALIGHT,  // the ultralight weight (= 200)
	FONT_WEIGHT_LIGHT,		 // the light weight (=300)
	FONT_WEIGHT_NORMAL,	 	 // the default weight (= 400)
	FONT_WEIGHT_SEMIBOLD,	 // a weight intermediate between normal and bold (=600)
	FONT_WEIGHT_BOLD,		 // the bold weight (= 700)
	FONT_WEIGHT_ULTRABOLD,   // the ultrabold weight (= 800)
	FONT_WEIGHT_HEAVY		 // the heavy weight (= 900)
};


enum eFontStretch
{
	FONT_STRETCH_ULTRA_CONDENSED,	// ultra condensed width
	FONT_STRETCH_EXTRA_CONDENSED,	// extra condensed width
	FONT_STRETCH_CONDENSED,			// condensed width
	FONT_STRETCH_SEMI_CONDENSED,	// semi condensed width
	FONT_STRETCH_NORMAL,			// the normal width
	FONT_STRETCH_SEMI_EXPANDED, 	// semi expanded width
	FONT_STRETCH_EXPANDED,			// expanded width
	FONT_STRETCH_EXTRA_EXPANDED,	// extra expanded width
	FONT_STRETCH_ULTRA_EXPANDED,	// ultra expanded width
};

///////////////////////////////////////////////////////////////////////////////
/*																			 */
/* G F O N T   C L A S S			  										 */
/*																			 */
///////////////////////////////////////////////////////////////////////////////

class GFont
{
public:
	GFont();
	GFont( const os::String cDescription );
	GFont( const char * zDescription );
	~GFont();

	void SetFamily( const os::String& cFamily );
	os::String GetFamily();

	enum eFontStyle GetStyle();
	void SetStyle( enum eFontStyle eStyle );

	enum eFontVariant GetVariant();
	void SetVariant( enum eFontVariant eVariant );

	enum eFontStretch GetStretch();
	void SetStretch( enum eFontStretch eStretch );

	enum eFontWeight GetWeight();
	void SetWeight( enum eFontWeight eWeight );
	
	void SetSize( int nSize );
	int GetSize();

	void SetAbsoluteSize( double nSize );
	bool IsAbsoluteSize();

	os::String GetDescription() const;
	PangoFontDescription* GetPangoFont() const;

	GFont& operator=( const GFont& cFont );

private:
	class _Private;
	_Private* m;	
};

#endif
