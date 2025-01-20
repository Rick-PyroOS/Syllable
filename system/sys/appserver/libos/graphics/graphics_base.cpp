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
/* G R A P H I C S    C L A S S			 									 */
/*																			 */
///////////////////////////////////////////////////////////////////////////////

#include <graphics/graphics_base.h>

using namespace os;
using namespace graphics;

#define ACCEL_INDICATOR "_"  // Which character that shall indicate an accel key

///////////////////////////////////////////////////////////////////////////////
//
// T H E   C L A S S
//
///////////////////////////////////////////////////////////////////////////////
Graphics :: Graphics()
{
	m_Settings = new _Private();
}

Graphics :: ~Graphics()
{
	delete m_Settings;
}

void Graphics :: SetAntiAlias( enum eAntiAlias eAlias )
{
	m_Settings->eAlias = eAlias;
}

enum eAntiAlias Graphics :: GetAntiAlias()
{
	return m_Settings->eAlias;
}

void Graphics :: SetOperator( enum eOperator eOperator )
{
	m_Settings->eOperator = eOperator;
}

enum eOperator Graphics :: GetOperator()
{
	return m_Settings->eOperator;
}

void Graphics :: SetFont( const GFont& cFont )
{
	m_Settings->cFont = cFont;
}

void Graphics :: GetFont( GFont& cFont )
{
	cFont = m_Settings->cFont;
}

void Graphics :: SetPen( Penstyle& cPen )
{
	m_Settings->cPen = cPen;
}

void Graphics :: GetPen( Penstyle& cPen )
{
	cPen = m_Settings->cPen;
}

void Graphics :: SetFillstyle( Fillstyle& cStyle )
{
	m_Settings->cFillstyle = cStyle;
}

void Graphics :: GetFillstyle( Fillstyle& cStyle )
{
	cStyle = m_Settings->cFillstyle;
}

void Graphics :: DrawRectangle( const Point cP1, const Point cP2 )
{
	DrawRectangle( Rect( cP1, cP2 ) );
}

void Graphics :: DrawRoundRectangle( const Point cP1, const Point cP2, const int nRadius )
{
	DrawRoundRectangle( Rect( cP1, cP2 ), nRadius );
}

void Graphics :: DrawBitmap( Bitmap* pcBitmap, const os::Point& cOrigin )
{
	Rect cSrc = pcBitmap->GetBounds(); 
	Rect cDst = Rect( cOrigin, cOrigin );
	cDst.right += cSrc.Width();
	cDst.bottom += cSrc.Height();

	DrawBitmap( pcBitmap, cSrc, cDst );
}

void Graphics :: DrawBitmap( Bitmap* pcBitmap, const Rect& cDst )
{
	Rect cSrc = pcBitmap->GetBounds(); 

	DrawBitmap( pcBitmap, cSrc, cDst );
}

void Graphics :: DrawString( const Point& cOrigin, const String& cText, bool bDisplayAccel )
{
	TextLayout cLayout;
	cLayout.SetFont( m_Settings->cFont );

	if( bDisplayAccel )
	{
		String cAccel = ACCEL_INDICATOR;
		String cAccelMark;
		cLayout.SetMarkupWithAccel( cText, cAccel, cAccelMark );
	}
	else
		cLayout.SetText( cText );

	DrawTextLayout( cOrigin, cLayout );
}

void Graphics :: DrawText( const Rect& cBound, const String& cText, enum eAlignmentMode eAlignment, bool bDisplayAccel, eVertAlignmentMode eVertAlignment )
{
	TextLayout cLayout;

	if( bDisplayAccel )
	{
		String cAccel = ACCEL_INDICATOR;
		String cAccelMark;
		cLayout.SetMarkupWithAccel( cText, cAccel, cAccelMark );
	}
	else
		cLayout.SetText( cText );

	cLayout.SetAlignment( eAlignment );
	cLayout.SetVertAlignment( eVertAlignment );

	DrawTextLayout( cBound, cLayout );
}

void Graphics :: DrawTextLayout( const Rect& cBound, TextLayout& cLayout )
{
	Point cOrigin = cBound.LeftTop();

	cLayout.SetFont( m_Settings->cFont );
	cLayout.SetWidth( cBound.Width() );
	cLayout.SetHeight( cBound.Height() );
	Point size = cLayout.GetPixelSize();

	switch( cLayout.GetVertAlignment() )
	{
		case TEXTLAYOUT_ALIGN_TOP:
			break;
		case TEXTLAYOUT_ALIGN_MIDDLE:
			cOrigin.y = ( cBound.Height() - size.y ) / 2 + cBound.top;
			break;
		case TEXTLAYOUT_ALIGN_BOTTOM:
			cOrigin.y = cBound.bottom - size.y;
			break;
	}

	DrawTextLayout( cOrigin, cLayout );
}

void Graphics :: FillRectangle( const Point cP1, const Point cP2 )
{
	FillRectangle( Rect( cP1, cP2 ) );
}

void Graphics :: FillRoundRectangle( const Point cP1, const Point cP2, const int nRadius )
{
	FillRoundRectangle( Rect( cP1, cP2 ), nRadius );
}

