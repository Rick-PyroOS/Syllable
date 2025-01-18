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

#ifndef __GRAPHICS_BASE_H_
#define __GRAPHICS_BASE_H_

///////////////////////////////////////////////////////////////////////////////
/*																			 */
/* G R A P H I C S   C L A S S  											 */
/*																			 */
///////////////////////////////////////////////////////////////////////////////

#include <gui/point.h>
#include <gui/bitmap.h>

#include <graphics/shape.h>
#include <graphics/gfont.h>
#include <graphics/transform.h>
#include <graphics/colour.h>
#include <graphics/penstyle.h>
#include <graphics/fillstyle.h>
#include <graphics/textlayout.h>
#include <graphics/common.h>

namespace graphics
{

///////////////////////////////////////////////////////////////////////////////
/*																			 */
/* G R A P H I C S   C L A S S												 */
/*																			 */
///////////////////////////////////////////////////////////////////////////////

class Graphics
{
protected:
	// This keeps all current settings and is protected so that
	// its subclasses can reach it
	class _Private
	{
	public:
		_Private()
		{		
			cFillstyle = FillstyleSolid( "black" );
			eAlias = ANTIALIAS_DEFAULT;
			eOperator = OPERATOR_SOURCE;
		}

		eAntiAlias eAlias;
		enum eOperator eOperator;		
		Penstyle cPen;
		Fillstyle cFillstyle;
		GFont cFont;
	};

public:

	Graphics();
	virtual ~Graphics();

	virtual void ShowPage() = 0;
	virtual void CopyPage() = 0;

	virtual void SetAntiAlias( enum eAntiAlias eAlias );
	virtual enum eAntiAlias GetAntiAlias();

	virtual void SetOperator( enum eOperator eOperator );
	virtual enum eOperator GetOperator();

	virtual void SetClip( const Shape& cShape ) = 0;
	virtual void RemoveClip() = 0;

	virtual void SetFont( const GFont& cFont );
	void GetFont( GFont& cFont );

	virtual void SetPen( Penstyle& cPen );
	virtual void GetPen( Penstyle& cPen );

	virtual void PopState() = 0;
	virtual void PushState() = 0;
	virtual void ApplyTransform( const Transform& cTransform ) = 0;
	virtual void SetTransform( const Transform& cTransform ) = 0;
	virtual void GetTransform( Transform& cTransform ) = 0;
	virtual void ResetTransform() = 0;

	virtual void SetFillstyle( Fillstyle& cStyle );
	virtual void GetFillstyle( Fillstyle& cStyle );

	virtual void Clear( const Colour& cColour, const os::Rect& cBound ) = 0;

	virtual void DrawShape( const Shape& cShape  ) = 0;
	virtual void DrawShape( const Shape& cShape, const Transform& cTransform  ) = 0;
	virtual void FillShape( const Shape& cShape  ) = 0;
	virtual void FillShape( const Shape& cShape, const Transform& cTransform  ) = 0;

	virtual void DrawLine( const os::Point cP1, const os::Point cP2 ) = 0;
	virtual void DrawCircle( const os::Point cCenter, const int nRadius ) = 0;
	virtual void DrawArc( const os::Point cCenter, const int nRadius, const double nStartAngle, const double nEndAngle ) = 0;
	virtual void DrawEllipse( const os::Point cCenter, const int nMajor, const int nMinor, const double nAngle = 0.0f ) = 0;
	virtual void DrawRectangle( const os::Point cP1, const os::Point cP2 );
	virtual void DrawRectangle( const os::Rect cRect ) = 0;
	virtual void DrawRoundRectangle( const os::Point cP1, const os::Point cP2, const int nRadius = 5 );
	virtual void DrawRoundRectangle( const os::Rect cRect, const int nRadius = 5  ) = 0;
	virtual void DrawCurve( const os::Point cFrom, const os::Point cControl1, const os::Point cControl2, const os::Point cTo ) = 0;
	virtual void DrawBitmap( os::Bitmap* pcBitmap, const os::Point& cOrigin );
	virtual void DrawBitmap( os::Bitmap* pcBitmap, const os::Rect& cDst );
	virtual void DrawBitmap( os::Bitmap* pcBitmap, const os::Rect& cSrc, const os::Rect& cDst ) = 0;
	virtual void DrawString( const os::Point& cOrigin, const os::String& cText, bool bDisplayAccel = true );
	virtual void DrawText( const os::Rect& cBound, const os::String& cText, enum eAlignmentMode eAlignment = TEXTLAYOUT_ALIGN_LEFT, bool bDisplayAccel = true, eVertAlignmentMode eVertAlignment = TEXTLAYOUT_ALIGN_TOP );
	virtual void DrawTextLayout( const os::Rect& cBound, TextLayout& cLayout );
	virtual void DrawTextLayout( const os::Point& cOrigin, TextLayout& cLayout ) = 0;

	virtual void FillCircle( const os::Point cCenter, const int nRadius ) = 0;
	virtual void FillEllipse( const os::Point cCenter, const int nMajor, const int nMinor, const double nAngle = 0.0f ) = 0;
	virtual void FillRectangle( const os::Point cP1, const os::Point cP2 );
	virtual void FillRectangle( const os::Rect cRect ) = 0;
	virtual void FillRoundRectangle( const os::Point cP1, const os::Point cP2, const int nRadius = 5 );
	virtual void FillRoundRectangle( const os::Rect cRect, const int nRadius = 5 ) = 0;
protected:
	_Private* m_Settings;
};

} // End of namespace

#endif
