#include "mainwindow.h"
#include "messages.h"


#include <gui/layoutview.h>
#include <gui/window.h>

MainWindow::MainWindow() : os::Window( os::Rect( 0, 0, 550, 300 ), "main_wnd", "Widget Gallery" )
{
	/* Set Icon */
	os::Resources cCol( get_image_id() );
	os::ResStream *pcStream = cCol.GetResourceStream( "icon.png" );
	os::BitmapImage *pcIcon = new os::BitmapImage( pcStream );
	SetIcon( pcIcon->LockBitmap() );
	delete( pcIcon );

	m_pcView = new WidgetGalleryView(GetBounds());
	AddChild(m_pcView);
}

void MainWindow::HandleMessage( os::Message* pcMessage )
{
	switch( pcMessage->GetCode() )
	{
		case M_APP_QUIT:
			PostMessage( os::M_QUIT );
			break;

		default:
			os::Window::HandleMessage(pcMessage);
			break;
	}
}

bool MainWindow::OkToQuit()
{
	os::Application::GetInstance()->PostMessage( os::M_QUIT );
	return( true );
}







