// EFileBrowser (C)opyright 2008 Jonas Jarvoll
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

#include "address_field_button.h"

using namespace os;

///////////////////////////////////////////////////////////////////////////////
//
// P R I V A T E
//
///////////////////////////////////////////////////////////////////////////////

class AddressFieldButton :: _Private
{
public:
	_Private() 
	{		
	}

	void _DrawRoundedFrame( View* view )
	{
		Rect cFillRect = view->GetBounds();
		cFillRect.Resize( 1, 4, -1 , -4 );
		view->SetFgColor( 180, 180, 180 );
		view->DrawLine( Point( 0, cFillRect.top ), Point( 0, cFillRect.bottom ) );
		view->DrawLine( Point( cFillRect.right + 1, cFillRect.top ), Point( cFillRect.right + 1, cFillRect.bottom ) );
		view->DrawLine( Point( 4, 0 ), Point( view->GetBounds().right - 4, 0 ) );
		view->DrawLine( Point( 2, 1 ), Point( 3, 1 ) );
		view->DrawLine( Point( 1, 2 ), Point( 1, 3 ) );
		view->DrawLine( Point( view->GetBounds().right - 2, 1 ), Point( view->GetBounds().right - 3, 1 ) );
		view->DrawLine( Point( view->GetBounds().right - 1, 2 ), Point( view->GetBounds().right - 1, 3 ) );

		cFillRect.top = view->GetBounds().bottom - 3;
		cFillRect.bottom = view->GetBounds().bottom;
		view->SetFgColor( 180, 180, 180 );
		view->DrawLine( Point( 4, cFillRect.bottom - 0 ), Point( view->GetBounds().right - 4, cFillRect.bottom - 0 ) );
		view->DrawLine( Point( 2, cFillRect.bottom - 1 ), Point( 3, cFillRect.bottom - 1 ) );
		view->DrawLine( Point( 1, cFillRect.bottom - 2 ), Point( 1, cFillRect.bottom - 3 ) );
		view->DrawLine( Point( view->GetBounds().right - 2, cFillRect.bottom - 1 ), Point(  view->GetBounds().right - 3, cFillRect.bottom - 1 ) );
		view->DrawLine( Point( view->GetBounds().right - 1, cFillRect.bottom - 2 ), Point(  view->GetBounds().right - 1, cFillRect.bottom - 3 ) );
		view->DrawLine( Point( 4, cFillRect.bottom ), Point( view->GetBounds().right - 4, cFillRect.bottom ) );
	}

	bool m_MouseOver;

	Image* m_Image;
	String m_Label;
	float m_FontHeight;
};

///////////////////////////////////////////////////////////////////////////////
//
// T H E   C L A S S
//
///////////////////////////////////////////////////////////////////////////////

AddressFieldButton :: AddressFieldButton( const Rect& cFrame, String label, Image* bitmap, Message* msg, uint32 nResizeMask, uint32 nFlags ) 
							    : Button( cFrame, "", "", msg, nResizeMask, nFlags )
{
	// Create the private class
	m = new _Private();

	m->m_Label = label;
	m->m_Image = bitmap;
	m->m_MouseOver = false;

	// Calculate font height
	font_height tmp;	
	GetFontHeight( &tmp );
	m->m_FontHeight = tmp.ascender + tmp.descender;	
}

AddressFieldButton :: ~AddressFieldButton()
{
	delete m->m_Image;
	delete m;
}

void AddressFieldButton :: AllDetached()
{
	m->m_MouseOver = false;
}

void AddressFieldButton :: MouseMove( const Point& cNewPos, int nCode, uint32 nButtons, Message* pcData )
{
	Button::MouseMove( cNewPos, nCode, nButtons, pcData ); 

	if( nCode == MOUSE_EXITED )
	{
		if( m->m_MouseOver )
		{
			m->m_MouseOver = false;
			Invalidate();
			Flush();
		}
	}
	else
	{
		bool mouseover = GetBounds().DoIntersect( cNewPos );

		if( mouseover != m->m_MouseOver )
		{
			m->m_MouseOver = mouseover;
			Invalidate();
			Flush();
		}
	}
}

void AddressFieldButton :: Paint( const Rect& cUpdate )
{
	SetDrawingMode( DM_COPY );

	float offset = 4.0f;
	Rect cFrame = GetBounds();

	// Draw selection if mouse is inside the button
	if( m->m_MouseOver )
	{
		Rect cFillRect = GetBounds();
		cFillRect.bottom = 3;
		cFillRect = GetBounds();
		cFillRect.Resize( 1, 4, -1 , -4 );
		FillRect( cFillRect, get_default_color( COL_ICON_SELECTED ) );
	
		SetFgColor( get_default_color( COL_ICON_SELECTED ) );
		DrawLine( Point( 4, 1 ), Point( GetBounds().right - 4, 1 ) );
		DrawLine( Point( 2, 2 ), Point( GetBounds().right - 2, 2 ) );
		DrawLine( Point( 2, 3 ), Point( GetBounds().right - 2, 3 ) );

		cFillRect.top = GetBounds().bottom - 3;
		cFillRect.bottom = GetBounds().bottom;
		FillRect( cFillRect, get_default_color( COL_NORMAL ) );

		SetFgColor( get_default_color( COL_ICON_SELECTED ) );
		DrawLine( Point( 4, cFillRect.bottom - 1 ), Point( GetBounds().right - 4, cFillRect.bottom - 1 ) );
		DrawLine( Point( 2, cFillRect.bottom - 2 ), Point( GetBounds().right - 2, cFillRect.bottom - 2 ) );
		DrawLine( Point( 2, cFillRect.bottom - 3 ), Point( GetBounds().right - 2, cFillRect.bottom - 3 ) );
	}
	else
		FillRect( cFrame, get_default_color( COL_NORMAL ) );

	// Always the draw the frame around the button
	m->_DrawRoundedFrame( this );

	// Draw the image
	SetDrawingMode( DM_BLEND );
	if( m->m_Image != NULL )
		m->m_Image->Draw( Point( cFrame.left + offset, cFrame.top + cFrame.Height() / 2 - m->m_Image->GetSize().y / 2 ), this );
	SetDrawingMode( DM_COPY );

	// Draw the label
	SetFgColor( 0, 0, 0 );
	GetFont()->SetFlags( FPF_BOLD | FPF_SMOOTHED ); // Doesnt FPF_BOLD work?
	DrawString( Point( cFrame.left + offset * 3 + m->m_Image->GetSize().x, cFrame.top + cFrame.Height() / 2 + m->m_FontHeight / 2 ), m->m_Label );
}

Point AddressFieldButton ::GetPreferredSize( bool bLargest ) const
{
	if( bLargest )
		return Point( COORD_MAX, COORD_MAX );

	Point size;

	// Calculate size of image
	if( m->m_Image != NULL )
		size = m->m_Image->GetSize();

	// Add width of text
	size.x += GetStringWidth( m->m_Label );

	// Add with some offsets
	size.x += 4 * 4;
	size.y += 4 * 2;

	return size;
}

