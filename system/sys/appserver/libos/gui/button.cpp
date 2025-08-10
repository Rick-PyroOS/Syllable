/*  libsyllable.so - the highlevel API library for Syllable
 *  Copyright (C) 1999 - 2001 Kurt Skauen
 *  Copyright (C) 2003 - 2004 The Syllable Team
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of version 2 of the GNU Library
 *  General Public License as published by the Free Software
 *  Foundation.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public
 *  License along with this library; if not, write to the Free
 *  Software Foundation, Inc., 59 Temple Place - Suite 330, Boston,
 *  MA 02111-1307, USA
 */

#include <atheos/types.h>

#include <util/looper.h>
#include <util/message.h>
#include <util/shortcutkey.h>
#include <gui/button.h>
#include <gui/window.h>
#include <gui/font.h>
#include <graphics/theme/theme.h>

#include <macros.h>

using namespace os;

class Button::Private {
	public:
	bool m_bClicked;
	bool m_bMouseOver;
	InputMode m_eInputMode;
};

//----------------------------------------------------------------------------
// NAME:
// DESC:
// NOTE:
// SEE ALSO:
//----------------------------------------------------------------------------

Button::Button( Rect cFrame, const String& cName, const String& cLabel, Message * pcMessage, uint32 nResizeMask, uint32 nFlags )
 : Control( cFrame, cName, cLabel, pcMessage, nResizeMask, nFlags )
{
	m = new Private;
	m->m_bClicked = m->m_bMouseOver = false;
	m->m_eInputMode = InputModeNormal;
}

//----------------------------------------------------------------------------
// NAME:
// DESC:
// NOTE:
// SEE ALSO:
//----------------------------------------------------------------------------

Button::~Button()
{
	delete m;
}

//----------------------------------------------------------------------------
// NAME:
// DESC:
// NOTE:
// SEE ALSO:
//----------------------------------------------------------------------------

Point Button::GetPreferredSize( bool bLargest ) const
{
	if( bLargest )
	{
		return Point( COORD_MAX, COORD_MAX );
	} else {
		font_height sHeight;
		GetFontHeight( &sHeight );
		Point cStringExt = GetTextExtent( GetLabel() );
		return Point( cStringExt.x+16, cStringExt.y + 10 + sHeight.line_gap );
	}
}

void Button::PostValueChange( const Variant & cNewValue )
{
	Invalidate();
	Flush();
}

void Button::LabelChanged( const String & cNewLabel )
{
	Invalidate();
	Flush();
}

void Button::EnableStatusChanged( bool bIsEnabled )
{
	Invalidate();
	Flush();
}

bool Button::Invoked( Message * pcMessage )
{
	Control::Invoked( pcMessage );
	return ( true );
}

//----------------------------------------------------------------------------
// NAME:
// DESC:
// NOTE:
// SEE ALSO:
//----------------------------------------------------------------------------

void Button::KeyDown( const char *pzString, const char *pzRawString, uint32 nQualifiers )
{
	if( IsEnabled() == false )
	{
		View::KeyDown( pzString, pzRawString, nQualifiers );
		return;
	}
	if( ( pzString[1] == '\0' && ( pzString[0] == VK_ENTER || pzString[0] == ' ' ) ) ||
		( GetShortcut() == ShortcutKey( pzRawString, nQualifiers ) ) )
	{
		if( m->m_eInputMode == InputModeToggle ) {
			SetValue( GetValue().AsBool() ? 0 : 1, false );
		} else {
			SetValue( 1, false );
		}
		MakeFocus();
	}
	else
	{
		if( m->m_eInputMode == InputModeNormal ) SetValue( 0, false );
		Control::KeyDown( pzString, pzRawString, nQualifiers );
	}
}

void Button::KeyUp( const char *pzString, const char *pzRawString, uint32 nQualifiers )
{
	if( IsEnabled() == false )
	{
		View::KeyUp( pzString, pzRawString, nQualifiers );
		return;
	}
	if( ( pzString[1] == '\0' && ( pzString[0] == VK_ENTER || pzString[0] == ' ' ) ) ||
		( GetShortcut() == ShortcutKey( pzRawString, nQualifiers ) ) )
	{
		if( m->m_eInputMode == InputModeNormal && GetValue().AsBool(  ) == true )
		{
			SetValue( false );
		}
	}
	else
	{
		Control::KeyDown( pzString, pzRawString, nQualifiers );
	}
}

//----------------------------------------------------------------------------
// NAME:
// DESC:
// NOTE:
// SEE ALSO:
//----------------------------------------------------------------------------

void Button::MouseDown( const Point & cPosition, uint32 nButton )
{
	if( nButton != 1 || IsEnabled() == false )
	{
		View::MouseDown( cPosition, nButton );
		return;
	}

	MakeFocus( true );

	m->m_bClicked = GetBounds().DoIntersect( cPosition );
	if( m->m_eInputMode == InputModeToggle ) {
		if( m->m_bClicked ) {
			SetValue( GetValue().AsBool() ? 0 : 1 );
		}
	} else if( m->m_eInputMode == InputModeRadio ) {
		SetValue( m->m_bClicked );
	} else {
		SetValue( m->m_bClicked, false );
	}
}

//----------------------------------------------------------------------------
// NAME:
// DESC:
// NOTE:
// SEE ALSO:
//----------------------------------------------------------------------------

void Button::MouseUp( const Point & cPosition, uint32 nButton, Message * pcData )
{
	if( nButton != 1 || IsEnabled() == false )
	{
		View::MouseUp( cPosition, nButton, pcData );
		return;
	}
	if( m->m_eInputMode == InputModeNormal ) {
		if( GetValue().AsBool(  ) != false )
		{
			SetValue( false );
		}
	}
	m->m_bClicked = false;
}

//----------------------------------------------------------------------------
// NAME:
// DESC:
// NOTE:
// SEE ALSO:
//----------------------------------------------------------------------------

void Button::MouseMove( const Point & cPosition, int nCode, uint32 nButtons, Message * pcData )
{
	if( IsEnabled())
	{

		if( m->m_bClicked && m->m_eInputMode == InputModeNormal )
		{
			uint32 nButtons;

			GetMouse( NULL, &nButtons );

			if( nButtons == 0 )
			{
				return;
			}

			if( nCode == MOUSE_OUTSIDE )
			{
				SetValue( false, false );
			}
			else
			{
				SetValue( true, false );
			}
		}
	
		else if (nCode == MOUSE_INSIDE)
		{
			m->m_bMouseOver = true;
		
			Invalidate();
			Flush();
		}
		else
		{
			m->m_bMouseOver = false;
			Invalidate();
			Flush();
		}
	}
	View::MouseMove( cPosition, nCode, nButtons, pcData );
}

void Button::Activated( bool bIsActive )
{
	Invalidate();
	Flush();
}

void Button::Paint( const Rect & cUpdateRect )
{
	Color32_s sTextColor;
	Color32_s sFrameColor;
	Color32_s sFGColor;
	
	
	if (IsEnabled())
	{
		sFGColor = get_default_color(COL_NORMAL);
		
		if (HasFocus())
		{
			sFrameColor = get_default_color(COL_FOCUS);
			sTextColor = os::Color32_s(0,0,0);
		}
		else if (m->m_bMouseOver)
		{
			sFrameColor = get_default_color(COL_ICON_SELECTED);
			sFGColor = get_default_color(COL_ICON_SELECTED);
			sTextColor = os::Color32_s(0,0,0);
		}
		else if (GetWindow(  )->GetDefaultButton(  ) == this )
		{
			sFrameColor = os::Color32_s(0,0,0);
			sTextColor = os::Color32_s(0,0,0);
		}
		else
		{
			sFrameColor = get_default_color(COL_SHADOW);
			sTextColor = os::Color32_s(0,0,0);
		}		
	}
	
	else
	{
		sFGColor = os::Color32_s(100,100,100);
		sTextColor = os::Color32_s(100,100,100);
		sFrameColor = os::Color32_s(100,100,100);
	}

	
	SetFgColor(sFGColor);
	SetBgColor(sFGColor);
	EraseRect(GetBounds());
	DrawRoundedFrame(os::Rect(1,4,-1,-4),sFrameColor);
	
	SetFgColor(sTextColor);
	DrawText( GetBounds(), GetLabel(), DTF_ALIGN_CENTER | DTF_UNDERLINES );
	
}

void Button::Paint(const Rect& cUpdateRect,graphics::Graphics& cGraphics){
//    if (GetStyle() == NULL){
//        dbprintf("Loading default theme for button");
//        SetStyle(os::Application()->GetTheme()->GetButtonStyle());
//    }

	Rect cBounds = GetBounds();
	cBounds.Resize( 0.5f, 0.5f, -0.5f, -0.5f );

	graphics::PenStyle cText( "black" );
	graphics::PenStyle cOuterBorder( "black", 1.0f );
	graphics::PenStyle cInnerBorder( "#eeeeee", 1.0f );
	graphics::FillStyleSolid cBackground( graphics::Colour( graphics::Colour::COL_NORMAL ) );

	// Set up appearance of the button based on the current status of the button
	if (IsEnabled())
	{
		if (m->m_bMouseOver)
			cInnerBorder.SetColour( graphics::Colour( graphics::Colour::COL_ICON_SELECTED ) );
	}
	else 
	{
	}

	// Create the looks of the outer border for the button
	graphics::Shape cOuterShape;
	cOuterShape.RoundRectangle( cBounds, 15 );

	// Create the looks of the outer border for the button
	graphics::Shape cInnerShape;
	cInnerShape.RoundRectangle( cBounds.Resize( 2, 2, -2, -2 ), 15 );

	// Draw button background
	cGraphics.SetFillStyle( cBackground );
	cGraphics.FillShape( cOuterShape );

	// Draw the outer border
	cGraphics.SetPen( cOuterBorder );
	cGraphics.DrawShape( cOuterShape );

	// Draw the inner border
	cGraphics.SetPen( cInnerBorder );
	cGraphics.DrawShape( cInnerShape );

	// Draw focus if any
	if( HasFocus() )
	{
		graphics::theme::style::BorderStyle style = graphics::theme::style::BorderStyle();
		style.SetMargin(os::Rect(2,2,-2,-2));
		style.SetPenStyleColour("red");
        style.SetPenStyleWidth(1);
		style.SetRadius(15);
		style.SetLineStyle(graphics::theme::style::BorderStyle::BORDER);
		graphics::PenStyle penStyle = style.GetPenStyle();
		cGraphics.SetPen( penStyle );
		cGraphics.DrawShape(style.GetShape(cBounds));
	}

	// And finally draw the text within the button
	cGraphics.SetPen( cText );
	cGraphics.DrawText( GetBounds(), GetLabel(), graphics::TEXTLAYOUT_ALIGN_CENTER, true, graphics::TEXTLAYOUT_ALIGN_MIDDLE );
}

void Button::SetInputMode( InputMode eInputMode )
{
	m->m_eInputMode = eInputMode;
	Flush();
}

Button::InputMode Button::GetInputMode() const
{
	return m->m_eInputMode;
}

