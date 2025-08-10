/*
 *  The Pyro application server
 *  Copyright (C) 2008 Jonas Jarvoll
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of version 2 of the GNU Library
 *  General Public License as published by the Free Software
 *  Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <cairo/cairo.h>
#include <pango/pangocairo.h>
#include "swindow.h"
#include "layer.h"
#include "sprite.h"
#include "ddriver.h"
#include "graphics_cairo.h"

#include "bitmap.h"

using namespace os;

///////////////////////////////////////////////////////////////////////////////
//
// P R I V A T E
//
///////////////////////////////////////////////////////////////////////////////

class GraphicsCairo :: _Private
{
public:
	_Private()
	{	
	}

	void _BeginUpdate()
	{
		if( psSurface != NULL )
		{
			dbprintf( "Layer::_BeginUpdate() Error: Forgot to free graphic context (missing a EndUpdate?)\n" );

			cairo_destroy( psCairo );
			cairo_surface_destroy( psSurface );
		
			psSurface = NULL;
			psCairo = NULL;
		}

		if( pcView != NULL && pcView->m_pcBitmap != NULL )	
		{
			switch( pcView->m_pcBitmap->m_eColorSpc )
			{
				case CS_RGBA32:
					psSurface = cairo_image_surface_create_for_data( pcView->m_pcBitmap->m_pRaster, 
																	 CAIRO_FORMAT_ARGB32, 
																	 pcView->m_pcBitmap->m_nWidth,
																  	 pcView->m_pcBitmap->m_nHeight, 
																	 pcView->m_pcBitmap->m_nBytesPerLine);
					break;
				case CS_RGB32:
				case CS_RGB24:
					psSurface = cairo_image_surface_create_for_data( pcView->m_pcBitmap->m_pRaster, 
																	 CAIRO_FORMAT_ARGB32, 
																	 pcView->m_pcBitmap->m_nWidth,
																  	 pcView->m_pcBitmap->m_nHeight, 
																	 pcView->m_pcBitmap->m_nBytesPerLine);
					break;
				default:
					dbprintf( "Layer::_BeginUpdate() Unsupported color space (%d).", pcView->m_pcBitmap->m_eColorSpc );
					break;
			}		

			if( psSurface != NULL )
			{
				psCairo = cairo_create( psSurface );

				if( psCairo != NULL )
				{
					// Translate all coordinates so that the orgin ends up inside the window
					// and not on the windowborder
					Point cTopLeft = pcView->ConvertToBitmap( Point( 0, 0 ) );			
					cairo_translate( psCairo, cTopLeft.x, cTopLeft.y );
				
					// Find affected regions
					Region* pcReg = pcView->GetRegion();
					if( NULL != pcReg )
					{	
						// Hide mouse cursor if needed
						if( ( pcView->m_pcWindow == NULL || pcView->m_pcWindow->IsOffScreen() == false ) && pcView->m_pcBitmap == g_pcScreenBitmap )
							SrvSprite::Hide();
	
						cairo_save( psCairo );

						// Create list of clipping rectangles
						cairo_new_path( psCairo );	// Make sure the previous path is empty
						ClipRect *pcClip;
						ENUMCLIPLIST( &pcReg->m_cRects, pcClip )
						cairo_rectangle( psCairo, pcClip->m_cBounds.left, pcClip->m_cBounds.top, pcClip->m_cBounds.Width() + 1, pcClip->m_cBounds.Height() + 1 );

						// Set the clip active
						cairo_clip( psCairo );
					}
				}
				else
				{
					cairo_surface_destroy( psSurface );
					psSurface = NULL;
					dbprintf( "Layer::_BeginUpdate() Unable to create Cairo context.\n" );
				}
			}
			else
				dbprintf( "Layer::_BeginUpdate() Unable to create Cairo image surface.\n" );

		}
		else
			dbprintf( "Layer::_BeginUpdate() Error: Invalid view)\n" );
	}

	void _EndUpdate()
	{		
		cairo_restore( psCairo );

		// Show mouse
		if( ( pcView->m_pcWindow == NULL || pcView->m_pcWindow->IsOffScreen() == false ) && pcView->m_pcBitmap == g_pcScreenBitmap )
			SrvSprite::Unhide();
			
		pcView->PutRegion( pcReg );

		cairo_destroy( psCairo );
		cairo_surface_destroy( psSurface );
		
		psSurface = NULL;
		psCairo = NULL;
	}

	void _SetOperator( GRndSetDrawingMode_s* psMsg )
	{
		cairo_set_operator( psCairo, (cairo_operator_t) psMsg->nDrawingMode ); // FIXME: We better do this nicer
	}

	void _SetAlias( GRndSetAliasMode_s* psMsg )
	{
		cairo_set_antialias( psCairo, (cairo_antialias_t) psMsg->nAliasMode ); // FIXME: We better do this nicer
	}

	void _SetPenStyle( GRndPen_s* psMsg )
	{
		nRed = psMsg->m_nRed;
		nBlue = psMsg->m_nBlue;
		nGreen = psMsg->m_nGreen;
		nAlpha = psMsg->m_nAlpha;

		cairo_set_line_width( psCairo, psMsg->m_nLinewidth);
		cairo_set_line_cap( psCairo, (cairo_line_cap_t) psMsg->m_nLineCap );
		cairo_set_line_join( psCairo, (cairo_line_join_t) psMsg->m_nLineJoin );
		cairo_set_miter_limit( psCairo, psMsg->m_nMiterLimit );

		if( psMsg->m_nNoDashes > 0 )  // only set dash if there are any
			cairo_set_dash( psCairo, &psMsg->m_zDashes, psMsg->m_nNoDashes, psMsg->m_nDashOffset );
		else // Disable dashes
			cairo_set_dash( psCairo, NULL, 0, 0.0f );
	}

	void _SetFillStyle( GRndFillStyle_s* psMsg )
	{
		// Release previous pattern
		if( psPattern != NULL )
		{
			cairo_pattern_destroy( psPattern );
			psPattern = NULL;
		}

		switch( psMsg->nType )
		{
			case 0:		// Solid
			{
				psPattern = cairo_pattern_create_rgba( psMsg->m_zColourData[ 0 ],
													   psMsg->m_zColourData[ 1 ],
													   psMsg->m_zColourData[ 2 ],
													   psMsg->m_zColourData[ 3 ] ); 				
				break;
			}
			case 1:		// Linear
			{
				psPattern = cairo_pattern_create_linear( psMsg->m_zColourData[ 0 ],
								 					 	 psMsg->m_zColourData[ 1 ],
														 psMsg->m_zColourData[ 2 ],
														 psMsg->m_zColourData[ 3 ] ); 

				for( int i = 0 ; i < psMsg->m_nNoColourStops ; i++ )
					cairo_pattern_add_color_stop_rgba( psPattern, psMsg->m_zColourData[ i * 5 + 0 + 4 ],
																  psMsg->m_zColourData[ i * 5 + 1 + 4 ],
																  psMsg->m_zColourData[ i * 5 + 2 + 4 ],  
																  psMsg->m_zColourData[ i * 5 + 3 + 4 ],  
																  psMsg->m_zColourData[ i * 5 + 4 + 4 ] );
				break;
			}
			case 2:		// Radial
			{
				psPattern = cairo_pattern_create_radial( psMsg->m_zColourData[ 0 ],
											 		 	 psMsg->m_zColourData[ 1 ],
														 psMsg->m_zColourData[ 2 ],
														 psMsg->m_zColourData[ 3 ],
														 psMsg->m_zColourData[ 4 ],
														 psMsg->m_zColourData[ 5 ] ); 

				for( int i = 0 ; i < psMsg->m_nNoColourStops ; i++ )
					cairo_pattern_add_color_stop_rgba( psPattern, psMsg->m_zColourData[ i * 5 + 0 + 6 ],
																  psMsg->m_zColourData[ i * 5 + 1 + 6 ],																 
																  psMsg->m_zColourData[ i * 5 + 2 + 6 ],
																  psMsg->m_zColourData[ i * 5 + 3 + 6 ],
																  psMsg->m_zColourData[ i * 5 + 4 + 6 ] );
				break;
			}
		}		
	}

	void _Stroke()
	{		
		cairo_set_source_rgba( psCairo, nRed, nBlue, nGreen, nAlpha );
		cairo_stroke( psCairo );
	}

	void _Fill()
	{
		if( psPattern == NULL )
		{
			dbprintf( "Graphics::_Fill Pattern emptu (forgot SetFillStyle?)\n" );
			return;
		}

		cairo_set_source( psCairo, psPattern );
		cairo_fill( psCairo );
	}

	void _DrawShape( GRndPath_s* psMsg )
	{
		// Create cairo path and matrix from message
		_CreatePath( psMsg );

		// Apply the path's matrix
		if( psCurrentPathMatrix != NULL )
		{
			cairo_save( psCairo );
			cairo_transform( psCairo, psCurrentPathMatrix );
		}

		if( psCurrentPath != NULL )
		{
			cairo_new_path( psCairo );
			cairo_append_path( psCairo, psCurrentPath );
		}

		_Stroke();

		if( psCurrentPathMatrix != NULL )
			cairo_restore( psCairo );
	}

	void _FillShape( GRndPath_s* psMsg )
	{
		// Create cairo path and matrix from message
		_CreatePath( psMsg );

		// Apply the path's matrix
		if( psCurrentPathMatrix != NULL )
		{
			cairo_save( psCairo );
			cairo_transform( psCairo, psCurrentPathMatrix );
		}

		if( psCurrentPath != NULL )
		{
			cairo_new_path( psCairo );
			cairo_append_path( psCairo, psCurrentPath );
		}

		_Fill();

		if( psCurrentPathMatrix != NULL )
			cairo_restore( psCairo );
	}

	void _DrawLine( GRndLine_s* psMsg )
	{	
		cairo_new_path( psCairo );
		cairo_move_to( psCairo, psMsg->cFrom.x, psMsg->cFrom.y );
		cairo_line_to( psCairo, psMsg->cTo.x, psMsg->cTo.y );
		_Stroke();
	}

	void _DrawCircle( GRndCircle_s* psMsg )
	{	
		cairo_new_path( psCairo );
		cairo_arc( psCairo, psMsg->cCenter.x, psMsg->cCenter.y, psMsg->nRadius, 0, 2 * M_PI );

		if( psMsg->bFilled )
			_Fill();
		else
			_Stroke();
	}

	void _DrawArc( GRndArc_s* psMsg )
	{
		cairo_new_path( psCairo );
		cairo_arc( psCairo, psMsg->cCenter.x, psMsg->cCenter.y, psMsg->nRadius, psMsg->nStartAngle, psMsg->nEndAngle );

		_Stroke();
	}

	void _DrawCurve( GRndCurve_s* psMsg )
	{	
		cairo_new_path( psCairo );
		cairo_move_to( psCairo, psMsg->cFrom.x, psMsg->cFrom.y );
		cairo_curve_to( psCairo, psMsg->cControl1.x, psMsg->cControl1.y, 
					  			 psMsg->cControl2.x, psMsg->cControl2.y, 
					 			 psMsg->cTo.x, psMsg->cTo.y );

		_Stroke();
	}

	void _DrawRectangle( GRndRectangle_s* psMsg )
	{		
		cairo_new_path( psCairo );

		double x = psMsg->cBound.left;
		double y = psMsg->cBound.top;
		double w = psMsg->cBound.Width();
		double h = psMsg->cBound.Height();
		double r = (double) psMsg->nRadius;

		if( r == 0 )
			cairo_rectangle( psCairo, x ,y, w, h );
		else
		{
			cairo_move_to( psCairo, x + r, y );
    		cairo_line_to( psCairo, x + w - r, y );
			cairo_curve_to( psCairo, x + w, y, x + w, y, x + w, y + r);
			cairo_line_to( psCairo, x + w, y + h - r );
			cairo_curve_to( psCairo, x + w, y + h, x + w, y + h, x + w - r, y + h );
			cairo_line_to( psCairo, x + r, y + h );
			cairo_curve_to( psCairo, x, y + h, x, y + h, x, y + h - r );
			cairo_line_to( psCairo, x, y + r );
			cairo_curve_to( psCairo, x, y, x, y, x + r, y);
		}

		if( psMsg->bFilled )
			_Fill();
		else
			_Stroke();
	}

	void _DrawEllipse( GRndEllipse_s* psMsg )
	{
		cairo_save( psCairo );

		cairo_translate( psCairo, psMsg->cCenter.x, psMsg->cCenter.y );
		cairo_rotate( psCairo, psMsg->nAngle );
		cairo_scale( psCairo, psMsg->nMajor / 2, psMsg->nMinor / 2 );

		cairo_new_path( psCairo );
		cairo_arc( psCairo, 0.0f, 0.0f, 1, 0, 2 * M_PI );

		if( psMsg->bFilled )
			_Fill();
		else
			_Stroke();

		cairo_restore( psCairo );
	}

	void _DrawText( GRndTextLayout_s* psReq )
	{	
		// Create a PangoLayout, set the font and text
  		PangoLayout* layout = pango_layout_new( _GetPangoContext() );
  
		if( layout == NULL )
		{
			dbprintf( "Layer::_DrawText() Unable to create Pango layout\n" );
			return;
		}

		// Type of text
		switch( psReq->nTextType )
		{
			case 0:		// No text
				break;
			case 1:		// Normal text
				pango_layout_set_text( layout, &psReq->zString[ psReq->nLengthFont ], -1 );
				break;
			case 2:		// Markup text
				pango_layout_set_markup( layout, &psReq->zString[ psReq->nLengthFont ], -1 );
				break;
			case 3:		// Markup text with accel)
			{
				gunichar* accel_marker = g_utf8_to_ucs4( &psReq->zString[ psReq->nLengthFont + psReq->nLengthText ], -1, NULL, NULL, NULL );
				pango_layout_set_markup_with_accel( layout, &psReq->zString[ psReq->nLengthFont ], -1, *accel_marker, NULL );
				g_free( accel_marker );
				break;
			}
		}

		PangoFontDescription* desc = pango_font_description_from_string( psReq->zString );
		if( desc == NULL )
		{
			dbprintf( "Layer::_DrawText() Unable to create Pango font from \"%s\"\n", psReq->zString );
			return;
		}

		pango_layout_set_font_description( layout, desc );

		pango_layout_set_width( layout, pango_units_from_double( psReq->nWidth ) );
		pango_layout_set_height( layout, pango_units_from_double( psReq->nHeight ) );
		pango_layout_set_wrap( layout, (PangoWrapMode) psReq->nWrapMode );
		pango_layout_set_ellipsize( layout, (PangoEllipsizeMode) psReq->nEllipsizeMode );
		pango_layout_set_indent( layout, psReq->nIndent );
		pango_layout_set_spacing( layout, psReq->nSpacing );
		pango_layout_set_justify( layout, psReq->bJustifyMode );
		pango_layout_set_auto_dir( layout, psReq->bAutoDirMode );
		pango_layout_set_alignment( layout, (PangoAlignment) psReq->nAlignmentMode );
		pango_layout_set_single_paragraph_mode( layout, psReq->bSingleParagraphMode );

		// Set up cairo pen
		cairo_set_source_rgba( psCairo, nRed, nBlue, nGreen, nAlpha );

		cairo_move_to( psCairo, psReq->cPos.x, psReq->cPos.y );
		pango_cairo_show_layout( psCairo, layout );

		pango_font_description_free( desc );
		g_object_unref( layout );
	} 

	void _DrawBitmap( GRndDrawBitmap_s* psHdr )
	{		
		SrvBitmap* pcBitmap = g_pcBitmaps->GetObj( psHdr->hBitmapToken );

		if( pcBitmap == NULL )
		{
			dbprintf( "Layer::_DrawBitmap() Unable to find bitmap (%d)\n", psHdr->hBitmapToken );
			return;
		}

		// Create a Cairo surface of the bitmap 
		cairo_surface_t* surface = cairo_image_surface_create_for_data(
				                        pcBitmap->m_pRaster,
                        				CAIRO_FORMAT_ARGB32,
                        				(int)psHdr->cSrcRect.Width(), (int)psHdr->cSrcRect.Height(),
                        				(int)pcBitmap->m_nBytesPerLine );

		cairo_set_source_surface( psCairo, surface, 0, 0 );
	
		// Set up the pattern matrix for scaling and translating the image	
		cairo_pattern_t* pattern = cairo_get_source( psCairo );
		cairo_matrix_t matrix;
		cairo_matrix_init_scale( &matrix, psHdr->cSrcRect.Width() / psHdr->cDstRect.Width(), psHdr->cSrcRect.Height() /  psHdr->cDstRect.Height() );
		cairo_matrix_translate( &matrix, -psHdr->cDstRect.left, -psHdr->cDstRect.top );
		cairo_pattern_set_matrix( pattern, &matrix );

		cairo_rectangle( psCairo, psHdr->cDstRect.left, psHdr->cDstRect.top, psHdr->cDstRect.Width(), psHdr->cDstRect.Height() );
		cairo_fill( psCairo );

		cairo_surface_destroy( surface );
	}

	// Skip Cairo and do a very fast fill
	void _Clear( GRndClearRectangle_s* psHdr )
	{
		if( NULL == pcView->m_pcBitmap )
			return;

		SrvBitmap* pcBitmap = pcView->m_pcBitmap;
		Region *pcReg = pcView->GetRegion();
	
		if( NULL == pcReg )
			return;

		Point cTopLeft = pcView->ConvertToBitmap( Point( 0, 0 ) );
		IPoint cITopLeft( cTopLeft );
		ClipRect *pcClip;
		IRect cDstRect( psHdr->cBound + pcView->m_cScrollOffset );
		Color32_s sColour;

		sColour.red = (int) ( psHdr->nRed * 255.0f );
		sColour.green = (int) ( psHdr->nGreen * 255.0f );
		sColour.blue = (int) ( psHdr->nBlue * 255.0f );
		sColour.alpha = (int) ( psHdr->nAlpha * 255.0f );

		if( ( pcView->m_pcWindow == NULL || pcView->m_pcWindow->IsOffScreen() == false ) && pcView->m_pcBitmap == g_pcScreenBitmap )
			SrvSprite::Hide( cDstRect + cITopLeft );
		
		ENUMCLIPLIST( &pcReg->m_cRects, pcClip )
		{
			IRect cRect = cDstRect & pcClip->m_cBounds;

			if( cRect.IsValid() )
			{
				pcBitmap->m_pcDriver->FillRect( pcBitmap, cRect + cITopLeft, sColour, DM_OVER );
			}
		}
		if( ( pcView->m_pcWindow == NULL || pcView->m_pcWindow->IsOffScreen() == false ) && pcView->m_pcBitmap == g_pcScreenBitmap )
		{
			SrvSprite::Unhide();
		}

		pcView->PutRegion( pcReg );
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

	PangoFontDescription* _GetPangoFont()
	{
		if( psPangoFont == NULL )
		{
			// Create default font
			psPangoFont = pango_font_description_from_string( "FreeSans 12" );
		}
		
		return psPangoFont;
	}

	void _CreatePath( GRndPath_s* psMsg )
	{
dbprintf( "_CreatePath\n" );
		// Create transform matrix from message
		delete psCurrentPathMatrix; // Delete old matrix
		psCurrentPathMatrix = NULL;

		if(psMsg->m_bUseMatrix)
		{
			psCurrentPathMatrix = new cairo_matrix_t;
			psCurrentPathMatrix->xx = psMsg->m_nMatrix[ 0 ]; psCurrentPathMatrix->yx = psMsg->m_nMatrix[ 3 ];
   			psCurrentPathMatrix->xy = psMsg->m_nMatrix[ 1 ]; psCurrentPathMatrix->yy = psMsg->m_nMatrix[ 4 ];
	   		psCurrentPathMatrix->x0 = psMsg->m_nMatrix[ 2 ]; psCurrentPathMatrix->y0 = psMsg->m_nMatrix[ 5 ];
		}

		// We only create the path if the path consists of data
		// (If the new path is empty we use the old path)
		if( psMsg->m_nNumData > 0 )
		{
dbprintf( "Create new path\n" );
			// Delete old path
			if( psCurrentPath != NULL )
				delete psCurrentPath->data;
			delete psCurrentPath;	

			psCurrentPath = new cairo_path_t;
			psCurrentPath->status = CAIRO_STATUS_SUCCESS;
			psCurrentPath->num_data = psMsg->m_nNumData;
			psCurrentPath->data = new cairo_path_data_t[ psMsg->m_nNumData ];
			memcpy( psCurrentPath->data, (void *) psMsg->m_zData, sizeof( cairo_path_data_t ) * psCurrentPath->num_data );
		}
	}


	Layer* pcView;
	Region* pcReg;
	cairo_surface_t* psSurface;
	cairo_t* psCairo;
	cairo_path_t* psCurrentPath;
	cairo_matrix_t* psCurrentPathMatrix;
	PangoContext* psPangoContext;
	PangoFontDescription* psPangoFont;
	cairo_pattern_t* psPattern;

	// The pen colour
	double nRed;
	double nBlue;
	double nGreen;
	double nAlpha;
};

//----------------------------------------------------------------------------
// NAME:
// DESC:
// NOTE:
// SEE ALSO:
//----------------------------------------------------------------------------

GraphicsCairo :: GraphicsCairo( Layer* pcView )
{
	my = new _Private();

	my->psCairo = NULL;
	my->psSurface = NULL;
	my->psPangoContext = NULL;
	my->psPangoFont = NULL;
	my->pcView = pcView;
	my->psCurrentPath = NULL;
	my->psCurrentPathMatrix = NULL;
	my->psPattern = NULL;
}

//----------------------------------------------------------------------------
// NAME:
// DESC:
// NOTE:
// SEE ALSO:
//----------------------------------------------------------------------------

GraphicsCairo :: ~GraphicsCairo()
{
	cairo_surface_destroy( my->psSurface );
	cairo_destroy( my->psCairo );
	cairo_pattern_destroy( my->psPattern );

	if( my->psPangoContext != NULL )
		g_object_unref( my->psPangoContext );

	if( my->psPangoFont != NULL )
		pango_font_description_free( my->psPangoFont );

	delete my->psCurrentPathMatrix;

	// Clean up path
	if( my->psCurrentPath != NULL )
	{
		if( my->psCurrentPath->data != NULL )
			delete my->psCurrentPath->data;
		delete my->psCurrentPath;	
	}

	delete my;
}

//----------------------------------------------------------------------------
// NAME:
// DESC:
// NOTE:
// SEE ALSO:
//----------------------------------------------------------------------------

void GraphicsCairo :: DispatchCommand( GRndHeader_s* psHdr )
{
	if( ( my->psSurface == NULL || my->psCairo == NULL ) && psHdr->nCmd != GFX_BEGIN_UPDATE )
	{
		dbprintf( "GraphicsCairo :: DispatchCommand() Error: Forgot to allocate graphic context (missing a BeginUpdate?)\n" );
		return;
	}

	switch ( psHdr->nCmd )
	{
		case GFX_BEGIN_UPDATE:
			my->_BeginUpdate();
			break;
		case GFX_END_UPDATE:
			my->_EndUpdate();
			break;
		case GFX_SET_ANTIALIAS:
			my->_SetAlias( (GRndSetAliasMode_s*) psHdr );
			break;
		case GFX_SET_OPERATOR:
			my->_SetOperator( (GRndSetDrawingMode_s*) psHdr );
			break;
		case GFX_SET_CLIP:
			break;
		case GFX_REMOVE_CLIP:
			break;
		case GFX_SET_FONT:
			break;
		case GFX_SET_PENSTYLE:
			my->_SetPenStyle( (GRndPen_s*) psHdr );
			break;
		case GFX_SET_FILLSTYLE:
			my->_SetFillStyle( (GRndFillStyle_s*) psHdr );
			break;
		case GFX_STROKE_PATH:
			my->_DrawShape( (GRndPath_s*) psHdr );
			break;
		case GFX_FILL_PATH:
			my->_FillShape( (GRndPath_s*) psHdr );
			break;
		case GFX_DRAW_LINE:
			my->_DrawLine( ( GRndLine_s * ) psHdr );
			break;
		case GFX_DRAW_CIRCLE:
			my->_DrawCircle( ( GRndCircle_s * ) psHdr );
			break;
		case GFX_DRAW_ARC:
			my->_DrawArc( ( GRndArc_s * ) psHdr );
			break;
		case GFX_DRAW_ELLIPSE:
			my->_DrawEllipse( ( GRndEllipse_s * ) psHdr );
			break;
		case GFX_DRAW_RECTANGLE:
			my->_DrawRectangle( ( GRndRectangle_s * ) psHdr );
			break;
		case GFX_DRAW_CURVE:
			my->_DrawCurve( ( GRndCurve_s * ) psHdr );
			break;
		case GFX_DRAW_BITMAP:
			my->_DrawBitmap( ( GRndDrawBitmap_s * ) psHdr );
			break;	
		case GFX_DRAW_TEXTLAYOUT:
			my->_DrawText( ( GRndTextLayout_s * ) psHdr );
			break;
		case GFX_CLEAR:
			my->_Clear( ( GRndClearRectangle_s * ) psHdr );
			break;
		default:
			dbprintf( "Layer::DispatchCommand() Unknown command\n" );
	}

}


