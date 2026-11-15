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

#include <storage/registrar.h>
#include <storage/volumes.h>
#include <gui/imagebutton.h>
#include <gui/stringview.h>
#include <gui/layoutview.h>
#include "../common.h"
#include "address_field.h"
#include "address_field_button.h"
#include "../iconserver.h"

using namespace os;

///////////////////////////////////////////////////////////////////////////////
//
// P R I V A T E
//
///////////////////////////////////////////////////////////////////////////////

class AddressField :: _Private
{
public:
	_Private()
	{
		LeftArrowVisible = false;
		RightArrowVisible = false;
		LeftArrow = NULL;
		RightArrow = NULL;
		ScrollAt = 0.0f;
	};

	Point _GetSize()
	{
		if( m_ListOfButtons.empty() )
			return Point( 0, 0 );

		Point size;

		for( uint i = 0 ; i < m_ListOfButtons.size() ; i++ )
		{
			Point button = m_ListOfButtons[ i ]->GetPreferredSize( false );

			size.x += button.x;
			size.y = MAX( size.y, button.y );
		}

		// Add offsets
		if( m_ListOfButtons.size() > 0 )
			size.x += 8.0f * ( m_ListOfButtons.size() );
		size.x += 8.0f;
		size.y += 4.0f;

		return size;
	}

	void _AddButton( View* view, String label, String path )
	{
		// Make message for button
		Message* msg = NULL;

		if( m_theMessage != NULL )
		{
			msg = new Message( *m_theMessage );
			msg->AddString( "file/path", path );
		}
	
		// Create new button
		AddressFieldButton* button = new AddressFieldButton( Rect(), label, IconServer::GetInstance()->GetIcon( path, IconServer::SMALL ), msg );
		m_ListOfButtons.push_back( button );
		view->AddChild( button );
	}

	void _Clean( View* view )
	{

		for( uint i = 0 ; i < m_ListOfButtons.size() ; i++ )
		{
			view->RemoveChild( m_ListOfButtons[ i ] );
			delete m_ListOfButtons[ i ];
		}

		// Clear old buttons
		m_ListOfButtons.clear();
	}

	void _CalculateButtonPosition( View* view )
	{
		Rect bound = view->GetBounds();

		// Calculate position of button
		float x = 8.0f - ScrollAt;

		if( LeftArrowVisible )
			bound.left += LeftArrow->GetFrame().right;

		if( RightArrowVisible )
			bound.right = RightArrow->GetFrame().left - 4.0f;

		for( uint t = 0 ; t < m_ListOfButtons.size() ; t++ )
		{
			Rect r( x, 2.0f, x + m_ListOfButtons[ t ]->GetPreferredSize( false ).x, m_ListOfButtons[ t ]->GetPreferredSize( false ).y + 2.0f );
			x = r.right + 8.0f;  // Next button position

			// If the button is not visible lets hide it
			if( r.right < bound.left || r.left > bound.right )
				r = Rect( -1, -1, -1, -1 );
			else if( ( r.left < bound.left && ( r.right - bound.left ) < 15.0f ) ||
					 ( r.right > bound.right && ( bound.right - r.left ) < 15.0f ) )
				r = Rect( -1, -1, -1, -1 );
			else
			{
				// If the button is covered with a large part lets hide
				if( r.left < bound.left && ( r.right - bound.left ) >= 15.0f) 
					r.left = bound.left + 4.0f;

				if( r.right > bound.right && ( bound.right - r.left ) >= 15.0f) 
					r.right = bound.right - 4.0f;
			}

			m_ListOfButtons[ t ]->SetFrame( r );
		}
	}

	void _UpdatePath( View* view )
	{
		_Clean( view );
	
		String path = m_Path;
		String button_path;

		// Sanity check
		if( path[0] != '/' )
			throw String("Not a valid path");

		// Remove the very first separator
		path = String( path, 1, path.size() );

		// Create root button
		_AddButton( view, String( "/" ), String( "/" ) );
		
		while( path != "" )
		{
			// Find next directory splitter "/"
			size_t s = path.find( "/" );

			// Extract folder
			String label = String(path, 0, s);

			// Remove it from the path
			path = String( path, s+1, path.size() );

			// Create nice path for the buttons
			button_path +=  String("/") + label;

			_AddButton( view, label, button_path );
	
			// No more path separator found
			if( s == std::string::npos)
				break;
		}

		_UpdateView( view );
	}

	void _UpdateView( View* view )
	{
		Rect bound = view->GetBounds();
		Point size = _GetSize();
		
		if( bound.Width() < size.x )
		{
			if( ScrollAt > 0.0f )
			{
				LeftArrowVisible = true;
				LeftArrow->SetFrame( Rect( bound.left + 4.0f, bound.top + 2.0f, 4.0f + bound.left + LeftArrow->GetPreferredSize( false ).x, bound.bottom - 2.0f ));
			}

			RightArrowVisible = true;
			RightArrow->SetFrame( Rect( bound.right - RightArrow->GetPreferredSize( false ).x - 4.0f , bound.top + 2.0f, bound.right - 4.0f , bound.bottom - 2.0f ));
		}
		else
		{
			if( LeftArrowVisible )
			{
				LeftArrowVisible = false;
				LeftArrow->SetFrame( Rect( -1, -1, -1, -1 ) );
			}
			if( RightArrowVisible )
			{
				RightArrowVisible = false;
				RightArrow->SetFrame( Rect( -1, -1, -1, -1 ));
			}
	
			ScrollAt = 0.0f;
		}

		_CalculateButtonPosition( view );
	}

	enum AddressFieldMessage { MSG_SCROLL_LEFT, MSG_SCROLL_RIGHT };

	std::vector< AddressFieldButton* > m_ListOfButtons;

	String m_Path;
	Message* m_theMessage;
	Handler* m_theHandler;
	Looper* m_theLooper;

	float ScrollAt;

	bool LeftArrowVisible;
	bool RightArrowVisible;

	Button* LeftArrow;
	Button* RightArrow;
};

///////////////////////////////////////////////////////////////////////////////
//
// T H E   C L A S S
//
///////////////////////////////////////////////////////////////////////////////

AddressField :: AddressField( const String path, Message* msg ) : View( Rect(), "" )
{
	// Create the private class
	m = new _Private();

	m->LeftArrow = new Button( Rect(), "", "<", new Message( _Private::MSG_SCROLL_LEFT ) );
	AddChild( m->LeftArrow );
	m->RightArrow = new Button( Rect(), "", ">", new Message( _Private::MSG_SCROLL_RIGHT ) );
	AddChild( m->RightArrow );
	m->m_Path = path;
	m->m_theHandler=NULL;
	m->m_theLooper=NULL;
	m->m_theMessage = msg;
	m->_UpdatePath( this );	

	SetTarget( m->m_theHandler, m->m_theLooper );
}

AddressField :: ~AddressField()
{
	delete m->m_theMessage;
	delete m;
}

void AddressField :: SetFrame( const Rect& cRect, bool notifyServer )
{
	View::SetFrame( cRect, notifyServer );

	m->_UpdateView( this );
}

void AddressField :: SetPath( os::String path )
{
	m->m_Path = path;
	m->_UpdatePath( this );
	SetTarget( m->m_theHandler, m->m_theLooper );
}

String AddressField :: GetPath()
{
	return m->m_Path;
}

Point AddressField :: GetPreferredSize( bool bLargest ) const
{
	if( bLargest )
		return Point( COORD_MAX, COORD_MAX );

	return m->_GetSize();
}


void AddressField :: AttachedToWindow()
{
	if( m->LeftArrow != NULL )
		m->LeftArrow->SetTarget( this );
	if( m->RightArrow != NULL )
		m->RightArrow->SetTarget( this );
}

void AddressField :: HandleMessage( Message* pcMessage )
{
	switch( pcMessage->GetCode() )
	{
		case _Private::MSG_SCROLL_LEFT:
			if( m->ScrollAt > 0.0f )
				m->ScrollAt -= 8.0f;
			if( m->ScrollAt < 0.0f )
				m->ScrollAt = 0.0f;

			m->_UpdateView( this );
			Invalidate();
			Flush();

			break;
		case _Private::MSG_SCROLL_RIGHT:
			m->ScrollAt += 8.0f;
			m->_UpdateView( this );
			Invalidate();
			Flush();
		break;
	}

}

status_t AddressField :: SetTarget( const os::Handler* pcHandler, const Looper* pcLooper )
{
	m->m_theHandler = (os::Handler*) pcHandler;
	m->m_theLooper = (os::Looper*) pcLooper;

	// Set target for the all button
	for( uint i = 0 ; i < m->m_ListOfButtons.size() ; i++ )
		m->m_ListOfButtons[ i ]->SetTarget( m->m_theHandler, m->m_theLooper );

	return Invoker::SetTarget( m->m_theHandler, m->m_theLooper );
}

