#include <util/application.h>
#include <gui/image.h>
#include <gui/imageview.h>
#include <iostream>

#include "win.h"
#include "gui/window.h"

using namespace os;

Win::Win( const Rect& cRect )
 : Window( cRect, "Win", "Double Buffering Example", os::WND_NOT_RESIZABLE | os::WND_NO_ZOOM_BUT | os::WND_NO_DEPTH_BUT )
{
	// 1. Create a BitmapImage the size of the window. This will be our background buffer -
	//    the buffer we render new frames into while the previous frame is still showing on
	//	  screen.
	m_pcImage = new BitmapImage();
	m_pcImage->ResizeCanvas( Point( GetBounds().Width() + 1, GetBounds().Height() + 1 ) );
	// 2. Create an ImageView. This ImageView will be responsible for displaying the
	//    background buffer, once we've finished rendering to it.
	m_pcImgView = new ImageView( GetBounds(), "m_pcImgView", m_pcImage, ImageView::NORMAL, CF_FOLLOW_ALL );
	AddChild( m_pcImgView );
	// 3. Add a Timer with the desired frame delay. In this case, the frame delay is 20 ms,
	//    which is the time each frame is visible.
	AddTimer( this, 1, 20000, false );

	// Initialize our animated objects
	Rect cBounds( GetBounds() );
	for( int i = 0; i < NUM_BOXES; i++ ) {
		m_caBoxes[ i ].Initialize( Point( cBounds.Width()/2, cBounds.Height()/2 ) );
	}
}

// This method gets called every 20 ms.
void Win::TimerTick( int nID )
{
	// 1. In case the last frame hasn't finished rendering yet, we'll have to
	//    extend the 20 ms frame delay until it is finished. 
	m_pcImage->Sync();
	// 2. Now we can display the background buffer. We must sync afterwards to make
	//    sure that we don't start changing the background buffer before it has been
	//    copied to the graphics card's frame buffer.
	m_pcImgView->Refresh();
	m_pcImgView->Sync();
	// 3. Time to render the next frame. The Flush() is asynchronous, so we can go on
	//    doing other things while the AppServer is busy compositing our new frame.
	RenderFrame( m_pcImage->GetView() );
	m_pcImage->Flush();
}

// This method is called from TimerTick() every 20 ms.
void Win::RenderFrame( View *pcView )
{
	Rect cBounds( m_pcImage->GetBounds() );

	pcView->SetDrawingMode( DM_COPY );
	pcView->FillRect( cBounds, 0xFF7F7F7F );
	pcView->SetDrawingMode( DM_BLEND );
	for( int i = 0; i < NUM_BOXES; i++ ) {
		m_caBoxes[ i ].Animate( cBounds, pcView );
	}
}

Win::~Win()
{
}
	
bool Win::OkToQuit()
{
    Application::GetInstance()->PostMessage( M_QUIT );
    return true;
}

//
// Methods for "BouncingBox" from this point on.
//
// The bouncing box consists of a 20x20 bitmap image, with random colour and a random
// velocity vector (m_cDelta).
// Each time Animate() is called, it moves itself in the direction given by m_cDelta,
// until it crashes into a wall (window border). When it does, it reverses it's direction.
//

BouncingBox::BouncingBox()
{
	m_cDelta.x = ( rand() % 100000 ) / 25000.0 - 2.0;
	m_cDelta.y = ( rand() % 100000 ) / 25000.0 - 2.0;
	m_cColor = 0x7F000000 | ( rand() % 255 ) << 16 | ( rand() % 255 ) << 8 | ( rand() % 255 );
	m_pcBoxImage = new BitmapImage( Bitmap::SHARE_FRAMEBUFFER | Bitmap::ACCEPT_VIEWS );
	m_pcBoxImage->ResizeCanvas( Point( 20, 20 ) );
	View* pcView = m_pcBoxImage->GetView();
	Rect cRect( pcView->GetBounds() );
	pcView->FillRect( cRect, 0xFFFFFFFF );
	cRect.Resize( 2, 2, -2, -2 );
	pcView->FillRect( cRect, m_cColor );
	m_pcBoxImage->Sync();
}

BouncingBox::~BouncingBox()
{
	delete m_pcBoxImage;
}

void BouncingBox::Initialize( const Point &cPos )
{
	m_cPosition = cPos;
}

void BouncingBox::Animate( Rect& cBounds, View* pcView )
{
	m_pcBoxImage->Draw( m_cPosition, pcView );

	m_cPosition.x += m_cDelta.x;
	if( m_cPosition.x < cBounds.left && m_cDelta.x < 0 ) {
		m_cDelta.x = -m_cDelta.x;
	}
	if( m_cPosition.x + 20 > cBounds.right && m_cDelta.x > 0 ) {
		m_cDelta.x = -m_cDelta.x;
	}

	m_cPosition.y += m_cDelta.y;
	if( m_cPosition.y < cBounds.top && m_cDelta.y < 0 ) {
		m_cDelta.y = -m_cDelta.y;
	}
	if( m_cPosition.y + 20 > cBounds.bottom && m_cDelta.y > 0 ) {
		m_cDelta.y = -m_cDelta.y;
	}
}

