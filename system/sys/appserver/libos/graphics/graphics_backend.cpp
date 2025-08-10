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

#include <string>
#include <vector>
#include <pango/pangocairo.h>

#include <graphics/graphics_backend.h>

using namespace os;
using namespace graphics;

///////////////////////////////////////////////////////////////////////////////
/*																			 */
/* G R A P HI C S B A C K E N D   C L A S S   								 */
/*																			 */
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//
// P R I V A T E
//
///////////////////////////////////////////////////////////////////////////////

class GraphicsBackend :: _Private
{
public:
	_Private()
	{	
		psSurface = NULL;	
		psCairo = NULL;
		psPangoContext = NULL;
		psFillPattern = NULL;
		psPenPattern = NULL;
	}

	PangoContext* _GetPangoContext()
	{
		if( psPangoContext == NULL )
		{
			PangoFontMap* fontmap = pango_cairo_font_map_get_default();
			psPangoContext = pango_cairo_font_map_create_context( (PangoCairoFontMap*) fontmap );
			pango_cairo_update_context( psCairo, psPangoContext );
		}
		
		return psPangoContext;
	}

	void SetupStroke()
	{
		if( psPenPattern )
			cairo_set_source( psCairo, psPenPattern );
		else
			cairo_set_source_rgba( psCairo, cPenColour.m_nRed, cPenColour.m_nBlue, cPenColour.m_nGreen, cPenColour.m_nAlpha );
	}

	void Stroke()
	{
		SetupStroke();
		cairo_stroke( psCairo );
	}

	cairo_pattern_t* psFillPattern;
	cairo_pattern_t* psPenPattern;
	Colour cPenColour;

	cairo_surface_t* psSurface;
	cairo_t* psCairo;
	PangoContext* psPangoContext;
};

///////////////////////////////////////////////////////////////////////////////
//
// T H E   C L A S S
//
///////////////////////////////////////////////////////////////////////////////

GraphicsBackend :: GraphicsBackend() : Graphics()
{
	m = new _Private();
}

GraphicsBackend :: ~GraphicsBackend()
{
	if( m->psCairo )
		cairo_destroy( m->psCairo );
	if( m->psSurface )
		cairo_surface_destroy( m->psSurface );
	if( m->psFillPattern )
		cairo_pattern_destroy( m->psFillPattern );
	if( m->psPenPattern )
		cairo_pattern_destroy( m->psPenPattern );

	delete m;
}

void GraphicsBackend :: SetCairoSurface( cairo_surface_t* surface )
{
	m->psSurface = surface;
	m->psCairo = cairo_create( surface );
}

void GraphicsBackend :: ShowPage()
{
	cairo_show_page( m->psCairo );
}

void GraphicsBackend :: CopyPage()
{
	cairo_copy_page( m->psCairo );
}

void GraphicsBackend :: SetAntiAlias( enum eAntiAlias eAlias )
{
	Graphics::SetAntiAlias( eAlias );
	cairo_set_antialias( m->psCairo, (cairo_antialias_t) eAlias );
}

void GraphicsBackend :: SetOperator( enum eOperator eOperator )
{
	Graphics::SetOperator( eOperator );
	cairo_set_operator( m->psCairo, (cairo_operator_t) eOperator );
}

void GraphicsBackend :: SetClip( const Shape& cShape )
{
	RemoveClip();
	cairo_new_path( m->psCairo );
	cairo_append_path( m->psCairo, cShape.GetCairoPath() );
	cairo_clip( m->psCairo );
}

void GraphicsBackend :: RemoveClip()
{
	cairo_reset_clip( m->psCairo );
}

void GraphicsBackend :: SetPen( PenStyle& cPen )
{
	Graphics::SetPen( cPen );

	// Remove any pen pattern
	if( m->psPenPattern )
	{
		cairo_pattern_destroy( m->psPenPattern );
		m->psPenPattern = NULL;
	}

	// Check if the pen shall use a fillstyle to draw or not
	if( cPen.IsFillStyleType() )
	{
		FillStyle style;
		cPen.GetFillStyle( style );
		m->psPenPattern = style.GetCairoPattern();
		cairo_set_source( m->psCairo, m->psPenPattern );
	}
	else
	{
		cPen.GetColour( m->cPenColour );
		cairo_set_source_rgba( m->psCairo, m->cPenColour.m_nRed, m->cPenColour.m_nBlue, m->cPenColour.m_nGreen, m->cPenColour.m_nAlpha );
	}

	cairo_set_line_width( m->psCairo, cPen.GetLinewidth() );
	cairo_set_line_cap( m->psCairo, (cairo_line_cap_t)  cPen.GetLineCap() );
	cairo_set_line_join( m->psCairo, (cairo_line_join_t) cPen.GetLineCap() );
	cairo_set_miter_limit( m->psCairo, cPen.GetMiterLimit() );

	// Set up the line pattern if any
	std::vector< double > dash;
	double offset;
	cPen.GetDashes( dash, offset );
	if( dash.empty() )
		cairo_set_dash( m->psCairo, NULL, 0, 0.0f );
	else
	{
		double* list = new double[ dash.size() ];

		for( uint i = 0 ; i < dash.size() ; i++ )
			list[ i ] = dash[ i ];

		cairo_set_dash( m->psCairo, list, dash.size(), offset );
		delete list;
	}
}

void GraphicsBackend :: SetFillStyle( FillStyle& cStyle )
{
	Graphics::SetFillStyle( cStyle );

	cairo_pattern_t* pattern = cStyle.GetCairoPattern();
	cairo_set_source( m->psCairo, pattern );
	cairo_pattern_destroy( pattern );
}

void GraphicsBackend :: PopState()
{
	cairo_restore( m->psCairo );
}

void GraphicsBackend :: PushState()
{
	cairo_save( m->psCairo );
}

void GraphicsBackend :: ApplyTransform( const Transform& cTransform )
{
	cairo_matrix_t t;
	cTransform.GetCairoMatrix( &t );
	cairo_transform( m->psCairo, &t );
}

void GraphicsBackend :: SetTransform( const Transform& cTransform )
{
	cairo_matrix_t t;
	cTransform.GetCairoMatrix( &t );
	cairo_set_matrix( m->psCairo, &t );
}

void GraphicsBackend :: GetTransform( Transform& cTransform )
{
	cairo_matrix_t t;
	cairo_get_matrix( m->psCairo, &t );
	cTransform.SetCoefficents( t.xx, t.xy, t.x0, t.yx, t.yy, t.y0 );
}

void GraphicsBackend :: ResetTransform()
{
	cairo_identity_matrix( m->psCairo );
}

void GraphicsBackend :: Clear( const Colour& cColour, const os::Rect& cBound )
{
	cairo_set_source_rgba( m->psCairo, cColour.m_nRed, cColour.m_nGreen, cColour.m_nBlue, cColour.m_nAlpha );
	cairo_new_path( m->psCairo );
	cairo_rectangle( m->psCairo, cBound.left, cBound.top, cBound.Width() + 1, cBound.Height() + 1 );
	m->Stroke();
}

void GraphicsBackend :: DrawShape( const Shape& cShape  )
{
	cairo_path_t* path = cShape.GetCairoPath();

	// Empty path
	if( path->num_data == 0 )
	{			
		cairo_path_destroy( path );
		return;
	}

	cairo_append_path( m->psCairo, path );
	m->Stroke();
	cairo_path_destroy( path );
}

void GraphicsBackend :: DrawShape( const Shape& cShape, const Transform& cTransform  )
{
	cairo_path_t* path = cShape.GetCairoPath();

	// Empty path
	if( path->num_data == 0 )
	{			
		cairo_path_destroy( path );
		return;
	}

	PushState();
		ApplyTransform( cTransform );		
		cairo_append_path( m->psCairo, path );
		m->Stroke();
	PopState();

	cairo_path_destroy( path );
}

void GraphicsBackend :: FillShape( const Shape& cShape  )
{
	cairo_path_t* path = cShape.GetCairoPath();

	// Empty path
	if( path->num_data == 0 )
	{			
		cairo_path_destroy( path );
		return;
	}

	cairo_append_path( m->psCairo, path );
	cairo_fill( m->psCairo );
	cairo_path_destroy( path );
}

void GraphicsBackend :: FillShape( const Shape& cShape, const Transform& cTransform  )
{
	cairo_path_t* path = cShape.GetCairoPath();

	// Empty path
	if( path->num_data == 0 )
	{			
		cairo_path_destroy( path );
		return;
	}

	PushState();
		ApplyTransform( cTransform );
		cairo_append_path( m->psCairo, path );
		cairo_fill( m->psCairo );
	PopState();	

	cairo_path_destroy( path );
}

void GraphicsBackend :: DrawLine( const Point cP1, const Point cP2 )
{
	cairo_new_path( m->psCairo );
	cairo_move_to( m->psCairo, cP1.x, cP1.y );
	cairo_line_to( m->psCairo, cP2.x, cP2.y );
	m->Stroke();
}

void GraphicsBackend :: DrawCircle( const Point cCenter, const int nRadius )
{
	cairo_new_path( m->psCairo );
	cairo_arc( m->psCairo, cCenter.x, cCenter.y, nRadius, 0, 2 * M_PI );
	m->Stroke();
}

void GraphicsBackend :: DrawArc( const Point cCenter, const int nRadius, const double nStartAngle, const double nEndAngle )
{
	cairo_new_path( m->psCairo );
	cairo_arc( m->psCairo, cCenter.x, cCenter.y, nRadius, nStartAngle, nEndAngle );
	m->Stroke();
}

void GraphicsBackend :: DrawEllipse( const Point cCenter, const int nMajor, const int nMinor, const double nAngle )
{
	cairo_save( m->psCairo );
	cairo_translate( m->psCairo, cCenter.x, cCenter.y );
	cairo_rotate( m->psCairo, nAngle );
	cairo_scale( m->psCairo, nMajor / 2, nMinor / 2 );

	cairo_new_path( m->psCairo );
	cairo_arc( m->psCairo, 0.0f, 0.0f, 1, 0, 2 * M_PI );
	m->Stroke();
	cairo_restore( m->psCairo );
}

void GraphicsBackend :: DrawRectangle( const Rect cRect )
{
	cairo_new_path( m->psCairo );
	cairo_rectangle( m->psCairo, cRect.left, cRect.top, cRect.Width() + 1, cRect.Height() + 1 );
	m->Stroke();
}

void GraphicsBackend :: DrawRoundRectangle( const Rect cRect, const int nRadius  )
{
	if( nRadius < 0 )
		throw std::string( "Radius cannot be negative" );

	if( nRadius == 0 )
		DrawRectangle( cRect );
	else
	{
		double x = cRect.left;
		double y = cRect.top;
		double w = cRect.Width() + 1;
		double h = cRect.Height() + 1;		

		cairo_new_path( m->psCairo );
		cairo_move_to( m->psCairo, x + nRadius, y );
    	cairo_line_to( m->psCairo, x + w - nRadius, y );
		cairo_curve_to( m->psCairo, x + w, y, x + w, y, x + w, y + nRadius );
		cairo_line_to( m->psCairo, x + w, y + h - nRadius );
		cairo_curve_to( m->psCairo, x + w, y + h, x + w, y + h, x + w - nRadius, y + h );
		cairo_line_to( m->psCairo, x + nRadius, y + h );
		cairo_curve_to( m->psCairo, x, y + h, x, y + h, x, y + h - nRadius );
		cairo_line_to( m->psCairo, x, y + nRadius );
		cairo_curve_to( m->psCairo, x, y, x, y, x + nRadius, y );
		m->Stroke();
	}
}

void GraphicsBackend :: DrawCurve( const Point cFrom, const Point cControl1, const Point cControl2, const Point cTo )
{
	cairo_new_path( m->psCairo );
	cairo_move_to( m->psCairo, cFrom.x, cFrom.y );
	cairo_curve_to( m->psCairo, cControl1.x, cControl1.y, 
				  			    cControl2.x, cControl2.y, 
				 			    cTo.x, cTo.y );

	m->Stroke();
}

void GraphicsBackend :: DrawBitmap( Bitmap* pcBitmap, const Rect& cSrc,  const Rect& cDest )
{
	// Create a Cairo surface of the bitmap 
	cairo_surface_t* surface = cairo_image_surface_create_for_data(
				                        pcBitmap->LockRaster(),
                        				CAIRO_FORMAT_ARGB32,
                        				(int)cSrc.Width() + 1, (int)cSrc.Height() + 1,
                        				(int)pcBitmap->GetBytesPerRow() );

	cairo_set_source_surface( m->psCairo, surface, 0, 0 );
	
	// Set up the pattern matrix for scaling and translating the image	
	cairo_pattern_t* pattern = cairo_get_source( m->psCairo );
	cairo_matrix_t matrix;
	cairo_matrix_init_scale( &matrix, cSrc.Width() / cDest.Width(), cSrc.Height() / cDest.Height() );
	cairo_matrix_translate( &matrix, -cDest.left, -cDest.top );
	cairo_pattern_set_matrix( pattern, &matrix );

	cairo_rectangle( m->psCairo, cDest.left, cDest.top, cDest.Width() + 1,cDest.Height() + 1 );
	cairo_fill( m->psCairo );

	cairo_surface_destroy( surface );
}

void GraphicsBackend :: DrawTextLayout( const Point& cOrigin, TextLayout& cLayout )
{
	// Get the PangoLayout, we not allowed to delete this
  	PangoLayout* layout = cLayout.GetPangoLayout();
  
	cairo_move_to( m->psCairo, cOrigin.x, cOrigin.y );
	pango_cairo_show_layout( m->psCairo, layout );	
}

void GraphicsBackend :: FillCircle( const Point cCenter, const int nRadius )
{
	cairo_new_path( m->psCairo );
	cairo_arc( m->psCairo, cCenter.x, cCenter.y, nRadius, 0, 2 * M_PI );
	cairo_fill( m->psCairo );	
}

void GraphicsBackend :: FillEllipse( const Point cCenter, const int nMajor, const int nMinor, const double nAngle )
{
	cairo_save( m->psCairo );
	cairo_translate( m->psCairo, cCenter.x, cCenter.y );
	cairo_rotate( m->psCairo, nAngle );
	cairo_scale( m->psCairo, nMajor / 2, nMinor / 2 );

	cairo_new_path( m->psCairo );
	cairo_arc( m->psCairo, 0.0f, 0.0f, 1, 0, 2 * M_PI );
	cairo_fill( m->psCairo );
	cairo_restore( m->psCairo );
}

void GraphicsBackend :: FillRectangle( const Rect cRect )
{
	cairo_new_path( m->psCairo );
	cairo_rectangle( m->psCairo, cRect.left, cRect.top, cRect.Width() + 1, cRect.Height() + 1 );
	cairo_fill( m->psCairo );
}

void GraphicsBackend :: FillRoundRectangle( const Rect cRect, const int nRadius )
{
	if( nRadius < 0 )
		throw std::string( "Radius cannot be negative" );

	if( nRadius == 0 )
		FillRectangle( cRect );
	else
	{
		double x = cRect.left;
		double y = cRect.top;
		double w = cRect.Width() + 1;
		double h = cRect.Height() + 1;		

		cairo_new_path( m->psCairo );
		cairo_move_to( m->psCairo, x + nRadius, y );
    	cairo_line_to( m->psCairo, x + w - nRadius, y );
		cairo_curve_to( m->psCairo, x + w, y, x + w, y, x + w, y + nRadius );
		cairo_line_to( m->psCairo, x + w, y + h - nRadius );
		cairo_curve_to( m->psCairo, x + w, y + h, x + w, y + h, x + w - nRadius, y + h );
		cairo_line_to( m->psCairo, x + nRadius, y + h );
		cairo_curve_to( m->psCairo, x, y + h, x, y + h, x, y + h - nRadius );
		cairo_line_to( m->psCairo, x, y + nRadius );
		cairo_curve_to( m->psCairo, x, y, x, y, x + nRadius, y );
		cairo_fill( m->psCairo );
	}
}

