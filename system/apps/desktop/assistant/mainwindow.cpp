#include "mainwindow.h"
#include "messages.h"
#include "top_view.h"

MainWindow::MainWindow() : os::Window( os::Rect( 0, 0, 450, 300 ), "main_wnd", "Assistant",os::WND_NO_TITLE | os::WND_NO_BORDER | os::WND_NO_CLOSE_BUT | os::WND_NO_ZOOM_BUT | os::WND_NO_DEPTH_BUT )
{
	/* Set Icon */
	os::Resources cCol( get_image_id() );
	os::ResStream *pcStream = cCol.GetResourceStream( "icon48x48.png" );
	os::BitmapImage *pcIcon = new os::BitmapImage( pcStream );
	SetIcon( pcIcon->LockBitmap() );
	delete( pcIcon );
	
	m_pcView = new TopView(this,GetFrame());
	AddChild(m_pcView);
	
	m_pcEscapeMessage = new os::Message(M_CHANGED);
	m_pcEscapeMessage->AddInt32("events",os::TextView::EI_ESC_PRESSED);
	AddShortcut(m_cEscapeShortcutKey = os::ShortcutKey(27), m_pcEscapeMessage);

	
}

MainWindow::~MainWindow(){
	RemoveShortcut(m_cEscapeShortcutKey);
	delete m_pcEscapeMessage;
}


void MainWindow::HandleMessage( os::Message* pcMessage )
{
	switch( pcMessage->GetCode() )
	{
		case M_APP_QUIT:
		{
			PostMessage( os::M_QUIT );
			break;
		}
		default:
			printf("window::handlemessage: %d\n",pcMessage->GetCode());
			Window::HandleMessage(pcMessage);
		break;
	}
}


bool MainWindow::OkToQuit()
{
	os::Application::GetInstance()->PostMessage( os::M_QUIT );
	return( true );
}

//TODO: figure out why the shortcut key is not being deleted
void MainWindow::SetFocus(){
	Window::MakeFocus(true);
	m_pcView->MakeFocus(true);
}
