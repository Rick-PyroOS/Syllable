/*
 * Rounded: demo of a borderless window with rounded corners, for testing the
 * appserver's window shapes.  Drag it with the left button.  Click the right
 * button or press Escape to quit.  Put it over wallpaper and other windows,
 * and look at what shows in the corners.
 */

#include <math.h>

#include <gui/view.h>
#include <gui/window.h>
#include <gui/region.h>
#include <util/application.h>
#include <util/message.h>

using namespace os;

enum { WIDTH = 360, HEIGHT = 110, RADIUS = 18 };

static Region *MakeRounded( int w, int h, int r )
{
	Region *pcReg = new Region();

	for( int y = 0; y < r; ++y )
	{
		float d = r - y - 0.5f;
		int nInset = int ( r - sqrtf( r * r - d * d ) + 0.5f );

		pcReg->AddRect( IRect( nInset, y, w - 1 - nInset, y ) );
		pcReg->AddRect( IRect( nInset, h - 1 - y, w - 1 - nInset, h - 1 - y ) );
	}
	pcReg->AddRect( IRect( 0, r, w - 1, h - 1 - r ) );
	return pcReg;
}

class RoundedView : public View
{
public:
	RoundedView( const Rect & cFrame ) : View( cFrame, "rounded_view", CF_FOLLOW_ALL, WID_WILL_DRAW )
	{
		m_bDragging = false;
	}

	void AttachedToWindow()
	{
		MakeFocus( true );
	}

	void Paint( const Rect & cUpdateRect )
	{
		FillRect( GetBounds(), Color32_s( 46, 92, 150, 255 ) );
		FillRect( Rect( 0, 0, WIDTH - 1, HEIGHT / 2 ), Color32_s( 66, 122, 186, 255 ) );

		SetFgColor( 255, 255, 255, 255 );
		SetBgColor( 66, 122, 186, 255 );
		DrawString( Point( 28, 34 ), "Rounded window, current appserver" );
		SetBgColor( 46, 92, 150, 255 );
		DrawString( Point( 28, 78 ), "Left drag moves. Right click or Esc quits." );
	}

	void MouseDown( const Point & cPosition, uint32 nButtons )
	{
		if( nButtons == 2 )
		{
			Application::GetInstance()->PostMessage( M_QUIT );
			return;
		}
		m_bDragging = true;
		m_cGrab = cPosition;
		MakeFocus( true );
	}

	void MouseUp( const Point & cPosition, uint32 nButtons, Message * pcData )
	{
		m_bDragging = false;
	}

	void MouseMove( const Point & cNewPos, int nCode, uint32 nButtons, Message * pcData )
	{
		if( m_bDragging )
		{
			GetWindow()->MoveBy( cNewPos - m_cGrab );
			/* Window::MoveBy() only queues the move; unlike SetFrame() it doesn't flush */
			GetWindow()->Flush();
		}
	}

	void KeyDown( const char *pzString, const char *pzRawString, uint32 nQualifiers )
	{
		if( pzString[0] == 27 )
		{
			Application::GetInstance()->PostMessage( M_QUIT );
		}
	}

private:
	bool m_bDragging;
	Point m_cGrab;
};

class RoundedWindow : public Window
{
public:
	RoundedWindow( const Rect & cFrame ) : Window( cFrame, "rounded", "Rounded", WND_NO_BORDER | WND_SINGLEBUFFER )
	{
		AddChild( new RoundedView( GetBounds() ) );

		Region *pcReg = MakeRounded( WIDTH, HEIGHT, RADIUS );

		SetShapeRegion( *pcReg );
		delete pcReg;
	}

	bool OkToQuit()
	{
		Application::GetInstance()->PostMessage( M_QUIT );
		return false;
	}
};

int main()
{
	Application *pcApp = new Application( "application/x-vnd.syllable-Rounded" );

	RoundedWindow *pcWindow = new RoundedWindow( Rect( 200, 150, 200 + WIDTH - 1, 150 + HEIGHT - 1 ) );

	pcWindow->Show();
	pcWindow->MakeFocus();
	pcApp->Run();
	return 0;
}
