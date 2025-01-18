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

#ifndef __GRAPHICS_VIEW_H_
#define __GRAPHICS_VIEW_H_

#include <gui/view.h>

#include <graphics/graphics_base.h>

namespace graphics
{

///////////////////////////////////////////////////////////////////////////////
/*																			 */
/* G R A P H I C S V I E W  C L A S S										 */
/*																			 */
///////////////////////////////////////////////////////////////////////////////

class GraphicsView : public Graphics
{
public:
	GraphicsView( os::View* pcView );
	~GraphicsView();

	void ShowPage();
	void CopyPage();

	void BeginUpdate();
	void EndUpdate();
	void Flush();
	void Sync();

	void SetAntiAlias( enum eAntiAlias eAlias );

	void SetOperator( enum eOperator eOperator );

	void SetClip( const Shape& cShape );
	void RemoveClip();

	void SetFont( const GFont& cFont );

	void SetPen( Penstyle& cPen );

	void SetFillstyle( Fillstyle& cStyle );

	void PopState();
	void PushState();
	void ApplyTransform( const Transform& cTransform );
	void SetTransform( const Transform& cTransform );
	void GetTransform( Transform& cTransform );
	void ResetTransform();

	void Clear( const Colour& cColour, const os::Rect& cBound );

	void DrawShape( const Shape& cShape  );
	void DrawShape( const Shape& cShape, const Transform& cTransform  );
	void FillShape( const Shape& cShape  );
	void FillShape( const Shape& cShape, const Transform& cTransform  );

	void DrawLine( const os::Point cP1, const os::Point cP2 );
	void DrawCircle( const os::Point cCenter, const int nRadius );
	void DrawArc( const os::Point cCenter, const int nRadius, const double nStartAngle, const double nEndAngle );
	void DrawEllipse( const os::Point cCenter, const int nMajor, const int nMinor, const double nAngle = 0.0f );
	void DrawRectangle( const os::Rect cRect );
	void DrawRoundRectangle( const os::Rect cRect, const int nRadius = 5  );
	void DrawCurve( const os::Point cFrom, const os::Point cControl1, const os::Point cControl2, const os::Point cTo );
	void DrawBitmap( os::Bitmap* pcBitmap, const os::Rect& cSrc, const os::Rect& cDest );	
	void DrawTextLayout( const os::Point& cOrigin, TextLayout& cLayout );

	void FillCircle( const os::Point cCenter, const int nRadius );
	void FillEllipse( const os::Point cCenter, const int nMajor, const int nMinor, const double nAngle = 0.0f );
	void FillRectangle( const os::Rect cRect );
	void FillRoundRectangle( const os::Rect cRect, const int nRadius = 5 );
private:
	class _Private;
	_Private* m;
};

} // End of namespace

#endif
