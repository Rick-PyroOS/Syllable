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

#ifndef __GRAPHICS_TEXTLAYOUT_H_
#define __GRAPHICS_TEXTLAYOUT_H_

#include <gui/point.h>
#include <util/string.h>
#include <graphics/gfont.h>
#include <pango/pangocairo.h>

namespace graphics
{

enum eWrapMode
{
	TEXTLAYOUT_WRAP_WORD,		// wrap lines at word boundaries.
	TEXTLAYOUT_WRAP_CHAR,		// wrap lines at character boundaries.
	TEXTLAYOUT_WRAP_WORD_CHAR	// wrap lines at word boundaries, but fall back to character boundaries if there is not enough space for a full word.
};

enum eEllipsizeMode
{
	TEXTLAYOUT_ELLIPSIZE_NONE, 		// No ellipsization
	TEXTLAYOUT_ELLIPSIZE_START, 	// Omit characters at the start of the text
	TEXTLAYOUT_ELLIPSIZE_MIDDLE, 	// Omit characters in the middle of the text
	TEXTLAYOUT_ELLIPSIZE_END  		// Omit characters at the end of the text
};

enum eAlignmentMode
{
	TEXTLAYOUT_ALIGN_LEFT,
	TEXTLAYOUT_ALIGN_CENTER,
	TEXTLAYOUT_ALIGN_RIGHT
};

enum eVertAlignmentMode
{
	TEXTLAYOUT_ALIGN_TOP,
	TEXTLAYOUT_ALIGN_MIDDLE,
	TEXTLAYOUT_ALIGN_BOTTOM
};

enum eTextType 
{ 
	TEXT_NONE, 			// No text have been set yet
	TEXT_NO_MARKUP, 	// The text was set by SetText
	TEXT_MARKUP, 		// The text was set by SetMarkup
	TEXT_MARKUP_ACCEL 	// The text was set byt SetMarkupWithAccel
};

// Text flags, used in DrawString and DrawText
#define TEXT_DISPLAY_ACCEL	0x01
#define TEXT_ALIGN_LEFT 	0x02
#define TEXT_ALIGN_CENTER 	0x04
#define TEXT_ALIGN_RIGHT 	0x08
#define TEXT_ALIGN_TOP 		0x10
#define TEXT_ALIGN_MIDDLE 	0x20
#define TEXT_ALIGN_BOTTOM 	0x40

///////////////////////////////////////////////////////////////////////////////
/*																			 */
/* T E X T L A Y O U T   C L A S S 											 */
/*																			 */
///////////////////////////////////////////////////////////////////////////////

class TextLayout
{
public:
	TextLayout();
	~TextLayout();

	void SetText( const os::String& cText );
	os::String GetText() const;
	void GetTextWithType( enum eTextType& eType, os::String& cText );

	void SetMarkup( const os::String& cText );
	void SetMarkupWithAccel( const os::String& cText, os::String& zAccelMarker, os::String& zFirstAccelMarker );

	void SetFont( GFont& cFont );
	GFont GetFont();
	os::String GetFontDescription();

	void SetWidth( double nWidth );
	double GetWidth();

	void SetHeight( double nHeight );
	double GetHeight();

	void SetWrap( enum eWrapMode eWrap );
	enum eWrapMode GetWrap();
	bool IsWrapped();

	void SetEllipsize( enum eEllipsizeMode eEllipsize );
	enum eEllipsizeMode GetEllipsize();
	bool IsEllipsize();

	void SetIndent( int nIndent );
	int GetIndent();

	void SetSpacing( int nSpacing );
	int GetSpacing();

	void SetJustify( bool bJustify );
	bool GetJustify();

	void SetAutoDir( bool bAutoDir );
	bool GetAutoDir();

	void SetAlignment( eAlignmentMode eAlignment );
	eAlignmentMode GetAlignment();

	void SetVertAlignment( eVertAlignmentMode eAlignment );
	eVertAlignmentMode GetVertAlignment();

	void SetSingleParagraphMode( bool bMode );
	bool GetSingleParagraphMode();

	os::Point GetPixelSize();

	os::String GetAccel();
	PangoLayout* GetPangoLayout() const;
private:
	class _Private;
	_Private* m;
};

} // End of namespace

#endif

