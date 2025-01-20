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
/* G R A P H I C S   C L A S S  											 */
/*																			 */
///////////////////////////////////////////////////////////////////////////////

#include <string>
#include <appserver/protocol.h>
#include <cairo/cairo.h>
#include <gui/window.h>
#include <graphics/graphics_view.h>

using namespace os;
using namespace std;
using namespace graphics;

///////////////////////////////////////////////////////////////////////////////
//
// P R I V A T E
//
///////////////////////////////////////////////////////////////////////////////

class GraphicsView :: _Private
{
public:
	_Private()
	{		
	}

	void _SendTextLayout( const Point& cOrigin, TextLayout& cLayout )
	{
		Window *pcWindow = pcView->GetWindow();

		if( pcWindow != NULL )
		{
			String text;
			String accel;
			enum eTextType type;
			cLayout.GetTextWithType( type, text );
			accel = cLayout.GetAccel();
			String font = cLayout.GetFontDescription();
			int total_size = sizeof( GRndTextLayout_s ) + text.size() + 1 + font.size() + 1 + accel.size() + 1;
			GRndTextLayout_s *psCmd = static_cast < GRndTextLayout_s * >( pcWindow->_AllocRenderCmd( GFX_DRAW_TEXTLAYOUT, pcView, total_size ) );

			if( psCmd != NULL )
			{
				psCmd->cPos = cOrigin;
				psCmd->nWidth = cLayout.GetWidth();
				psCmd->nHeight = cLayout.GetHeight();
				psCmd->nWrapMode = (int) cLayout.GetWrap();
				psCmd->nEllipsizeMode = (int) cLayout.GetEllipsize();
				psCmd->nIndent = cLayout.GetIndent();
				psCmd->nSpacing = cLayout.GetSpacing();
				psCmd->bJustifyMode = cLayout.GetJustify();
				psCmd->bAutoDirMode = cLayout.GetAutoDir();
				psCmd->nAlignmentMode = cLayout.GetAlignment();
				psCmd->bSingleParagraphMode = cLayout.GetSingleParagraphMode();
				psCmd->nTextType = type;
				psCmd->nLengthFont = font.size() + 1;
				psCmd->nLengthText = text.size() + 1;
				strncpy( psCmd->zString, font.c_str(), font.size() + 1 );
				strncpy( &psCmd->zString[ psCmd->nLengthFont ], text.c_str(), text.size() + 1 );
				strncpy( &psCmd->zString[ psCmd->nLengthFont + psCmd->nLengthText ], accel.c_str(), accel.size() + 1 );
				pcWindow->_PutRenderCmd();
			}
		}
	}

	View* pcView;
};

///////////////////////////////////////////////////////////////////////////////
//
// T H E   C L A S S
//
///////////////////////////////////////////////////////////////////////////////

GraphicsView :: GraphicsView( View* pcView ) : Graphics()
{
	m = new _Private();
	m->pcView = pcView;
}

GraphicsView :: ~GraphicsView()
{
	delete m;
}

void GraphicsView :: ShowPage()
{
}

void GraphicsView :: CopyPage()
{
}

void GraphicsView :: BeginUpdate()
{
	Window *pcWindow = m->pcView->GetWindow();

	if( pcWindow != NULL )
	{
		pcWindow->_AllocRenderCmd( GFX_BEGIN_UPDATE, m->pcView, sizeof( GRndHeader_s ) );
		pcWindow->_PutRenderCmd();
	}
}

void GraphicsView :: EndUpdate()
{
	Window *pcWindow = m->pcView->GetWindow();

	if( pcWindow != NULL )
	{
		pcWindow->_AllocRenderCmd( GFX_END_UPDATE, m->pcView, sizeof( GRndHeader_s ) );
		pcWindow->_PutRenderCmd();	
	}
}

void GraphicsView :: Flush()
{
	Window *pcWindow = m->pcView->GetWindow();
	if( pcWindow != NULL )
		pcWindow->Flush();
}

void GraphicsView :: Sync()
{
	Window *pcWindow = m->pcView->GetWindow();
	if( pcWindow != NULL )
		pcWindow->Sync();
}

void GraphicsView :: SetAntiAlias( enum eAntiAlias eAlias )
{
	Graphics::SetAntiAlias( eAlias );

	Window *pcWindow = m->pcView->GetWindow();

	if( pcWindow != NULL )
	{
		GRndSetAliasMode_s *psCmd = static_cast < GRndSetAliasMode_s * >( pcWindow->_AllocRenderCmd( GFX_SET_ANTIALIAS, m->pcView, sizeof( GRndSetAliasMode_s ) ) );
		psCmd->nAliasMode = (int) eAlias;
		pcWindow->_PutRenderCmd();
	}
}

void GraphicsView :: SetOperator( enum eOperator eOperator )
{
	Graphics::SetOperator( eOperator );

	Window *pcWindow = m->pcView->GetWindow();

	if( pcWindow != NULL )
	{
		GRndSetDrawingMode_s *psCmd = static_cast < GRndSetDrawingMode_s * >( pcWindow->_AllocRenderCmd( GFX_SET_OPERATOR, m->pcView, sizeof( GRndSetDrawingMode_s ) ) );
		psCmd->nDrawingMode = (int) eOperator;
		pcWindow->_PutRenderCmd();
	}
}

void GraphicsView :: SetClip( const Shape& cShape )
{
	Window *pcWindow = m->pcView->GetWindow();

	if( pcWindow != NULL )
	{		
		cairo_path_t* path = cShape.GetCairoPath();

		if( path->num_data == 0 )
		{
			cairo_path_destroy( path );
			return;
		}

		GRndPath_s *psCmd = static_cast < GRndPath_s * >( pcWindow->_AllocRenderCmd( GFX_SET_CLIP, m->pcView, sizeof( GRndPath_s ) + path->num_data * sizeof( cairo_path_data_t ) ) );
		psCmd->m_nNumData = path->num_data;
		memcpy( (void*) (psCmd->m_zData), (void*) path->data, sizeof( cairo_path_data_t ) * path->num_data );

		cairo_path_destroy( path );

		pcWindow->_PutRenderCmd();
	}
}

void GraphicsView :: RemoveClip()
{
	Window *pcWindow = m->pcView->GetWindow();

	if( pcWindow != NULL )
	{		
		static_cast < GRndPath_s * >( pcWindow->_AllocRenderCmd( GFX_REMOVE_CLIP, m->pcView, sizeof( GRndHeader_s ) ) );		
		pcWindow->_PutRenderCmd();
	}
}

void GraphicsView :: SetFont( const GFont& cFont )
{
	Graphics::SetFont( cFont );

	Window *pcWindow = m->pcView->GetWindow();

	if( pcWindow != NULL )
	{		
		String desc = cFont.GetDescription();

		GRndFont_s *psCmd = static_cast < GRndFont_s * >( pcWindow->_AllocRenderCmd( GFX_SET_FONT, m->pcView, sizeof( GRndFont_s ) + desc.size() + 1 ) );
	
		psCmd->nLength = -1;
		strncpy( psCmd->zString, desc.c_str(), desc.size() + 1 );

		pcWindow->_PutRenderCmd();
	}
}

void GraphicsView :: SetPen( Penstyle& cPen )
{
	Graphics::SetPen( cPen );

	Window *pcWindow = m->pcView->GetWindow();

	if( pcWindow != NULL )
	{
		vector<double> cDashList;
		double nDashOffset;
		cPen.GetDashes( cDashList, nDashOffset );

		GRndPen_s *psCmd = static_cast < GRndPen_s * >( pcWindow->_AllocRenderCmd( GFX_SET_PENSTYLE, m->pcView, sizeof( GRndPen_s ) + cDashList.size() * sizeof( double ) ) );

		Colour cColour;
		cPen.GetColour( cColour );
 
		psCmd->m_nRed = cColour.m_nRed;
		psCmd->m_nGreen = cColour.m_nGreen;
		psCmd->m_nBlue = cColour.m_nBlue;
		psCmd->m_nAlpha = cColour.m_nAlpha;
		psCmd->m_nLinewidth = cPen.GetLinewidth();
		psCmd->m_nLineCap = cPen.GetLineCap();
		psCmd->m_nLineJoin = cPen.GetLineJoin();
		psCmd->m_nMiterLimit = cPen.GetMiterLimit();
		psCmd->m_nDashOffset = nDashOffset;
		psCmd->m_nNoDashes = (int) cDashList.size();
		
		if(	psCmd->m_nNoDashes > 0 ) // only copy data if there is any
			memcpy( (void*) &(psCmd->m_zDashes), (void*) &cDashList[0], cDashList.size() * sizeof( double ) );

		pcWindow->_PutRenderCmd(); 
	}
}

void GraphicsView :: SetFillstyle( Fillstyle& cStyle )
{
	Graphics::SetFillstyle( cStyle );

	Window *pcWindow = m->pcView->GetWindow();

	if( pcWindow != NULL )
	{
		int size = sizeof( GRndFillStyle_s );
		Fillstyle::eFillstyleType type = cStyle.GetType();

		switch( type )
		{
			case Fillstyle :: FillNone:	// Nothing to do
				return;
			case Fillstyle :: FillSolid:
				size += sizeof( double ) * 4;
				break;
			case Fillstyle :: FillLinear:
				size += sizeof( double ) * cStyle.ColourStopCount() * 5 + sizeof( double ) * 4;
				break;
			case Fillstyle :: FillRadial:
				size += sizeof( double ) * cStyle.ColourStopCount() * 5 + sizeof( double ) * 6;
				break;
		}
		
		GRndFillStyle_s *psCmd = static_cast < GRndFillStyle_s * >( pcWindow->_AllocRenderCmd( GFX_SET_FILLSTYLE, m->pcView, size ) );

		switch( type )
		{
			case Fillstyle :: FillNone:
				break;
			case Fillstyle :: FillSolid:
			{
				psCmd->nType = 0; // SOLID
				Colour cColour;
				cStyle.GetColour( cColour );
				psCmd->m_nNoColourStops = 1;
				psCmd->m_zColourData[ 0 ] = cColour.m_nRed;
				psCmd->m_zColourData[ 1 ] = cColour.m_nGreen;
				psCmd->m_zColourData[ 2 ] = cColour.m_nBlue;
				psCmd->m_zColourData[ 3 ] = cColour.m_nAlpha;
				break;
			}
			case Fillstyle :: FillLinear:
			{
				psCmd->nType = 1; // LINEAR
				Point cStart, cEnd;
				cStyle.GetLinearStartStop( cStart, cEnd );
				psCmd->m_zColourData[ 0 ] = cStart.x;
				psCmd->m_zColourData[ 1 ] = cStart.y;
				psCmd->m_zColourData[ 2 ] = cEnd.x;
				psCmd->m_zColourData[ 3 ] = cEnd.y;

				psCmd->m_nNoColourStops = cStyle.ColourStopCount();
				for( uint i = 0 ; i < cStyle.ColourStopCount() ; i++ )
				{
					Colour cColour;
					double nOffset;
			
					cStyle.GetColourStop( i, nOffset, cColour );
					psCmd->m_zColourData[ i * 5 + 0 + 4 ] = nOffset;
					psCmd->m_zColourData[ i * 5 + 1 + 4 ] = cColour.m_nRed;
					psCmd->m_zColourData[ i * 5 + 2 + 4 ] = cColour.m_nGreen;
					psCmd->m_zColourData[ i * 5 + 3 + 4 ] = cColour.m_nBlue;
					psCmd->m_zColourData[ i * 5 + 4 + 4 ] = cColour.m_nAlpha;
				}
				break;
			}
			case Fillstyle :: FillRadial:
			{
				psCmd->nType = 2; // RADIAL
				Point cCenter1, cCenter2;
				double nRadius1, nRadius2;
				cStyle.GetRadialStartStop( cCenter1, nRadius1, cCenter2, nRadius2 );
				psCmd->m_zColourData[ 0 ] = cCenter1.x;
				psCmd->m_zColourData[ 1 ] = cCenter1.y;
				psCmd->m_zColourData[ 2 ] = nRadius1;
				psCmd->m_zColourData[ 3 ] = cCenter2.x;
				psCmd->m_zColourData[ 4 ] = cCenter2.y;
				psCmd->m_zColourData[ 5 ] = nRadius2;

				psCmd->m_nNoColourStops = cStyle.ColourStopCount();
				for( uint i = 0 ; i < cStyle.ColourStopCount() ; i++ )
				{
					Colour cColour;
					double nOffset;
			
					cStyle.GetColourStop( i, nOffset, cColour );
					psCmd->m_zColourData[ i * 5 + 0 + 6 ] = nOffset;
					psCmd->m_zColourData[ i * 5 + 1 + 6 ] = cColour.m_nRed;
					psCmd->m_zColourData[ i * 5 + 2 + 6 ] = cColour.m_nGreen;
					psCmd->m_zColourData[ i * 5 + 3 + 6 ] = cColour.m_nBlue;
					psCmd->m_zColourData[ i * 5 + 4 + 6 ] = cColour.m_nAlpha;
				}			}
		}

		pcWindow->_PutRenderCmd(); 
	}
}

void GraphicsView :: PopState()
{
}

void GraphicsView :: PushState()
{
}

void GraphicsView :: ApplyTransform( const Transform& cTransform )
{
}

void GraphicsView :: SetTransform( const Transform& cTransform )
{

}

void GraphicsView :: GetTransform( Transform& cTransform )
{
}

void GraphicsView :: ResetTransform()
{
}

void GraphicsView :: Clear( const Colour& cColour, const Rect& cBound )
{
	Window *pcWindow = m->pcView->GetWindow();

	if( pcWindow != NULL )
	{
		GRndClearRectangle_s *psCmd = static_cast < GRndClearRectangle_s * >( pcWindow->_AllocRenderCmd( GFX_CLEAR, m->pcView, sizeof( GRndClearRectangle_s ) ) );

		if( psCmd != NULL )
		{
			psCmd->cBound = cBound;			
			psCmd->nRed = cColour.m_nRed;
			psCmd->nBlue = cColour.m_nBlue;
			psCmd->nGreen = cColour.m_nGreen;
			psCmd->nAlpha = cColour.m_nAlpha;
			pcWindow->_PutRenderCmd();
		}
	}
}

void GraphicsView :: DrawShape( const Shape& cShape  )
{
	Window *pcWindow = m->pcView->GetWindow();

	if( pcWindow != NULL )
	{		
		cairo_path_t* path = cShape.GetCairoPath();

		if( path->num_data == 0 )
		{			
			cairo_path_destroy( path );
			return;
		}

		GRndPath_s* psCmd = static_cast < GRndPath_s * >( pcWindow->_AllocRenderCmd( GFX_STROKE_PATH, m->pcView, sizeof( GRndPath_s ) + path->num_data * sizeof( cairo_path_data_t ) ) );
		
		psCmd->m_bUseMatrix = false;

		psCmd->m_nNumData = path->num_data;
		memcpy( (void*) (psCmd->m_zData), (void*) path->data, sizeof( cairo_path_data_t ) * path->num_data );

		cairo_path_destroy( path );

		pcWindow->_PutRenderCmd();
	}
}

void GraphicsView :: DrawShape( const Shape& cShape, const Transform& cTransform  )
{
	Window *pcWindow = m->pcView->GetWindow();

	if( pcWindow != NULL )
	{			
		cairo_path_t* path = cShape.GetCairoPath();

		if( path->num_data == 0 )
		{			
			cairo_path_destroy( path );
			return;
		}

		GRndPath_s* psCmd = static_cast < GRndPath_s * >( pcWindow->_AllocRenderCmd( GFX_STROKE_PATH, m->pcView, sizeof( GRndPath_s ) + path->num_data * sizeof( cairo_path_data_t ) ) );

		// Set up transform matrix
		psCmd->m_bUseMatrix = true;
		cTransform.GetCoefficents( psCmd->m_nMatrix[ 0 ], psCmd->m_nMatrix[ 1 ], psCmd->m_nMatrix[ 2 ],
								   psCmd->m_nMatrix[ 3 ], psCmd->m_nMatrix[ 4 ], psCmd->m_nMatrix[ 5 ] );

		psCmd->m_nNumData = path->num_data;
		memcpy( (void*) (psCmd->m_zData), (void*) path->data, sizeof( cairo_path_data_t ) * path->num_data );

		cairo_path_destroy( path );

		pcWindow->_PutRenderCmd();		
	}
}

void GraphicsView :: FillShape( const Shape& cShape  )
{
	Window *pcWindow = m->pcView->GetWindow();

	if( pcWindow != NULL )
	{		
		cairo_path_t* path = cShape.GetCairoPath();

		if( path->num_data == 0 )
		{
			printf( "Path is empty\n" );
			cairo_path_destroy( path );
			return;
		}

		GRndPath_s *psCmd = static_cast < GRndPath_s * >( pcWindow->_AllocRenderCmd( GFX_FILL_PATH, m->pcView, sizeof( GRndPath_s ) + path->num_data * sizeof( cairo_path_data_t ) ) );

		psCmd->m_bUseMatrix = false;
		psCmd->m_nNumData = path->num_data;
		memcpy( (void*) (psCmd->m_zData), (void*) path->data, sizeof( cairo_path_data_t ) * path->num_data );

		cairo_path_destroy( path );

		pcWindow->_PutRenderCmd();
	}
}

void GraphicsView :: FillShape( const Shape& cShape, const Transform& cTransform  )
{
	Window *pcWindow = m->pcView->GetWindow();

	if( pcWindow != NULL )
	{		
		cairo_path_t* path = cShape.GetCairoPath();

		if( path->num_data == 0 )
		{
			printf( "Path is empty\n" );
			cairo_path_destroy( path );
			return;
		}

		GRndPath_s *psCmd = static_cast < GRndPath_s * >( pcWindow->_AllocRenderCmd( GFX_FILL_PATH, m->pcView, sizeof( GRndPath_s ) + path->num_data * sizeof( cairo_path_data_t ) ) );

		// Set up transform matrix
		psCmd->m_bUseMatrix = true;
		cTransform.GetCoefficents( psCmd->m_nMatrix[ 0 ], psCmd->m_nMatrix[ 1 ], psCmd->m_nMatrix[ 2 ],
								   psCmd->m_nMatrix[ 3 ], psCmd->m_nMatrix[ 4 ], psCmd->m_nMatrix[ 5 ] );

		psCmd->m_nNumData = path->num_data;
		memcpy( (void*) (psCmd->m_zData), (void*) path->data, sizeof( cairo_path_data_t ) * path->num_data );

		cairo_path_destroy( path );

		pcWindow->_PutRenderCmd();
	}
}

void GraphicsView :: DrawLine( const Point cP1, const Point cP2 )
{
	Window *pcWindow = m->pcView->GetWindow();

	if( pcWindow != NULL )
	{
		GRndLine_s *psCmd = static_cast < GRndLine_s * >( pcWindow->_AllocRenderCmd( GFX_DRAW_LINE, m->pcView, sizeof( GRndLine_s ) ) );

		if( psCmd != NULL )
		{
			psCmd->cFrom = cP1;
			psCmd->cTo = cP2;
			pcWindow->_PutRenderCmd();
		}
	}
}

void GraphicsView :: DrawCircle( const Point cCenter, const int nRadius )
{
	Window *pcWindow = m->pcView->GetWindow();

	if( pcWindow != NULL )
	{
		GRndCircle_s *psCmd = static_cast < GRndCircle_s * >( pcWindow->_AllocRenderCmd( GFX_DRAW_CIRCLE, m->pcView, sizeof( GRndCircle_s ) ) );

		if( psCmd != NULL )
		{
			psCmd->cCenter = cCenter;
			psCmd->nRadius = nRadius;
			psCmd->bFilled = false;
			pcWindow->_PutRenderCmd();
		}
	}
}

void GraphicsView :: DrawArc( const Point cCenter, const int nRadius, const double nStartAngle, const double nEndAngle )
{
	Window *pcWindow = m->pcView->GetWindow();

	if( pcWindow != NULL )
	{
		GRndArc_s *psCmd = static_cast < GRndArc_s * >( pcWindow->_AllocRenderCmd( GFX_DRAW_ARC, m->pcView, sizeof( GRndArc_s ) ) );

		if( psCmd != NULL )
		{
			psCmd->cCenter = cCenter;
			psCmd->nRadius = nRadius;
			psCmd->nStartAngle = nStartAngle;
			psCmd->nEndAngle = nEndAngle;
			pcWindow->_PutRenderCmd();
		}
	}
}

void GraphicsView :: DrawEllipse( const Point cCenter, const int nMajor, const int nMinor, const double nAngle )
{
	Window *pcWindow = m->pcView->GetWindow();

	if( pcWindow != NULL )
	{
		GRndEllipse_s *psCmd = static_cast < GRndEllipse_s * >( pcWindow->_AllocRenderCmd( GFX_DRAW_ELLIPSE, m->pcView, sizeof( GRndEllipse_s ) ) );

		if( psCmd != NULL )
		{
			psCmd->cCenter = cCenter;
			psCmd->nMajor = nMajor;
			psCmd->nMinor = nMinor;
			psCmd->nAngle = nAngle;
			psCmd->bFilled = false;
			pcWindow->_PutRenderCmd();
		}
	}
}

void GraphicsView ::  DrawRectangle( const Rect cRect )
{
	Window *pcWindow = m->pcView->GetWindow();

	if( pcWindow != NULL )
	{
		GRndRectangle_s *psCmd = static_cast < GRndRectangle_s * >( pcWindow->_AllocRenderCmd( GFX_DRAW_RECTANGLE, m->pcView, sizeof( GRndRectangle_s ) ) );

		if( psCmd != NULL )
		{
			psCmd->cBound = cRect;
			psCmd->nRadius = 0;
			psCmd->bFilled = false;
			pcWindow->_PutRenderCmd();
		}
	}
}

void GraphicsView :: DrawRoundRectangle( const Rect cRect, const int nRadius )
{
	Window *pcWindow = m->pcView->GetWindow();

	if( pcWindow != NULL )
	{
		GRndRectangle_s *psCmd = static_cast < GRndRectangle_s * >( pcWindow->_AllocRenderCmd( GFX_DRAW_RECTANGLE, m->pcView, sizeof( GRndRectangle_s ) ) );

		if( psCmd != NULL )
		{
			psCmd->cBound = cRect;
			psCmd->nRadius = nRadius;
			psCmd->bFilled = false;
			pcWindow->_PutRenderCmd();
		}
	}
}

void GraphicsView :: DrawCurve( const Point cFrom, const Point cControl1, const Point cControl2, const Point cTo )
{
	Window *pcWindow = m->pcView->GetWindow();

	if( pcWindow != NULL )
	{
		GRndCurve_s *psCmd = static_cast < GRndCurve_s * >( pcWindow->_AllocRenderCmd( GFX_DRAW_CURVE, m->pcView, sizeof( GRndCurve_s ) ) );

		if( psCmd != NULL )
		{
			psCmd->cFrom = cFrom;
			psCmd->cControl1 = cControl1;
			psCmd->cControl2 = cControl2;
			psCmd->cTo = cTo;
			pcWindow->_PutRenderCmd();
		}
	}
}

void GraphicsView :: DrawBitmap( Bitmap* pcBitmap, const Rect& cSrc, const Rect& cDest )
{
	Window *pcWindow = m->pcView->GetWindow();

	if( pcWindow != NULL )
	{
		GRndDrawBitmap_s *psCmd = static_cast < GRndDrawBitmap_s * >( pcWindow->_AllocRenderCmd( GFX_DRAW_BITMAP, m->pcView, sizeof( GRndDrawBitmap_s ) ) );

		if( psCmd != NULL )
		{
			psCmd->hBitmapToken = pcBitmap->GetHandle();
			psCmd->cSrcRect = cSrc;
			psCmd->cDstRect = cDest;

			pcWindow->_PutRenderCmd();
		}
	}
}

void GraphicsView :: DrawTextLayout( const Point& cOrigin, TextLayout& cLayout )
{
	m->_SendTextLayout( cOrigin, cLayout );
}

void GraphicsView :: FillCircle( const Point cCenter, const int nRadius )
{
	Window *pcWindow = m->pcView->GetWindow();

	if( pcWindow != NULL )
	{
		GRndCircle_s *psCmd = static_cast < GRndCircle_s * >( pcWindow->_AllocRenderCmd( GFX_DRAW_CIRCLE, m->pcView, sizeof( GRndCircle_s ) ) );

		if( psCmd != NULL )
		{
			psCmd->cCenter = cCenter;
			psCmd->nRadius = nRadius;
			psCmd->bFilled = true;
			pcWindow->_PutRenderCmd();
		}
	}
}

void GraphicsView :: FillEllipse( const Point cCenter, const int nMajor, const int nMinor, const double nAngle )
{
	Window *pcWindow = m->pcView->GetWindow();

	if( pcWindow != NULL )
	{
		GRndEllipse_s *psCmd = static_cast < GRndEllipse_s * >( pcWindow->_AllocRenderCmd( GFX_DRAW_ELLIPSE, m->pcView, sizeof( GRndEllipse_s ) ) );

		if( psCmd != NULL )
		{
			psCmd->cCenter = cCenter;
			psCmd->nMajor = nMajor;
			psCmd->nMinor = nMinor;
			psCmd->nAngle = nAngle;
			psCmd->bFilled = true;
			pcWindow->_PutRenderCmd();
		}
	}
}

void GraphicsView :: FillRectangle( const Rect cRect )
{
	Window *pcWindow = m->pcView->GetWindow();

	if( pcWindow != NULL )
	{
		GRndRectangle_s *psCmd = static_cast < GRndRectangle_s * >( pcWindow->_AllocRenderCmd( GFX_DRAW_RECTANGLE, m->pcView, sizeof( GRndRectangle_s ) ) );

		if( psCmd != NULL )
		{
			psCmd->cBound = cRect;
			psCmd->nRadius = 0;
			psCmd->bFilled = true;
			pcWindow->_PutRenderCmd();
		}
	}
}

void GraphicsView :: FillRoundRectangle( const Rect cRect, const int nRadius )
{
	Window *pcWindow = m->pcView->GetWindow();

	if( pcWindow != NULL )
	{
		GRndRectangle_s *psCmd = static_cast < GRndRectangle_s * >( pcWindow->_AllocRenderCmd( GFX_DRAW_RECTANGLE, m->pcView, sizeof( GRndRectangle_s ) ) );

		if( psCmd != NULL )
		{
			psCmd->cBound = cRect;
			psCmd->nRadius = nRadius;
			psCmd->bFilled = true;
			pcWindow->_PutRenderCmd();
		}
	}
}

