#include "mainwindow.h"
#include "messages.h"
#include <unistd.h>
#include "Executer/commonfuncs.h"


MainWindow::MainWindow() : os::Window( os::Rect( 0, 0, 300, 300 ), "main_wnd", "Service Manager" )
{
	os::LayoutView* pcView = new os::LayoutView( GetBounds(), "layout_view" );
	#include "mainwindowLayout.cpp"
	m_pcServiceList->InsertColumn( "Service", 200 );
	m_pcServiceList->InsertColumn( "Status", 100 );
	pcView->SetRoot( m_pcRoot );
	AddChild( pcView );
	
	/* Set Icon */
	os::Resources cCol( get_image_id() );
	os::ResStream *pcStream = cCol.GetResourceStream( "icon48x48.png" );
	os::BitmapImage *pcIcon = new os::BitmapImage( pcStream );
	SetIcon( pcIcon->LockBitmap() );
	delete( pcIcon );

	LoadServices();
	SaveServices();
}


void MainWindow::HandleMessage( os::Message* pcMessage )
{
	switch( pcMessage->GetCode() )
	{
		case M_START:
		{
			StartService();
			sleep(3);
			LoadServices();
			SaveServices();
			break;
		}

		case M_STOP:
		{
			StopService();
			sleep(3);
			LoadServices();
			SaveServices();
			break;
		}

		case M_APP_QUIT:
		{
			SaveServices();
			PostMessage( os::M_QUIT );
		}
		break;
	}
}


void MainWindow::StopService()
{
	int nSelected = -1;
	nSelected = m_pcServiceList->GetLastSelected();
	if( nSelected != -1 )
	{
		os::Event cEvent;
		os::ListViewStringRow* pcRow = static_cast<os::ListViewStringRow*>( m_pcServiceList->GetRow( nSelected ) );
		os::String cEventMessageString = os::String( "services/" ) + pcRow->GetString(0) + os::String( "/Stop" );
		if( cEvent.SetToRemote( cEventMessageString, 0 ) == 0 )
		{
			os::Message cMsg;
			cEvent.PostEvent ( &cMsg, this, -1 );
		}
	}
}


void MainWindow::StartService()
{
	int nSelected = -1;
	nSelected = m_pcServiceList->GetLastSelected();
	if( nSelected != -1 )
	{
		os::ListViewStringRow* pcRow = static_cast<os::ListViewStringRow*>( m_pcServiceList->GetRow( nSelected ) );
		os::String cExec = os::String( "/boot/system/extensions/services/" ) + pcRow->GetString(0);

		os::Event cEvent;
		os::String cEventMessageString = os::String( "services/" ) + pcRow->GetString(0) + os::String( "/Alive" );

		int nEvent = cEvent.SetToRemote( cEventMessageString, 0 );
		if( nEvent != 0 ) {
			LaunchFile("/system/bin/FileBrowser");
		}
	}
}


void MainWindow::LoadServices()
{
	m_pcServiceList->Clear();

	pDir = opendir( "/boot/system/extensions/services" );
	if (pDir == NULL)
	{
		return;
	}

	while( ( psEntry = readdir( pDir ) ) != NULL )
	{
		if (strcmp( psEntry->d_name, ".") == 0 || strcmp( psEntry->d_name, ".." ) == 0) {
			continue;
		}

		os::Event cEvent;
		os::String cEventMessageString = os::String( "services/" ) + psEntry->d_name + os::String( "/Alive" );
		if( cEvent.SetToRemote( cEventMessageString, 0 ) == 0 )
		{
			os::Message cMsg;
			cEvent.PostEvent ( &cMsg, this, -1 );
			os::ListViewStringRow *row = new os::ListViewStringRow();
			row->AppendString( psEntry->d_name );
			row->AppendString( "Running" );
			m_pcServiceList->InsertRow( row );
		} else {
			os::ListViewStringRow *row = new os::ListViewStringRow();
			row->AppendString( psEntry->d_name );
			row->AppendString( "Stopped" );
			m_pcServiceList->InsertRow( row );
		}
	}
}


void MainWindow::SaveServices()
{
	os::String zPath = getenv( "HOME" );
	zPath += "/Settings/ServiceManager";
	try
	{
		mkdir( zPath.c_str(), S_IRWXU | S_IRWXG | S_IRWXO );
		zPath += "/Services";
		os::File cFile( zPath, O_RDWR | O_CREAT );	
		os::Settings* pcSettings = new os::Settings(new os::File( zPath ));

		int nRowCount = m_pcServiceList->GetRowCount();
		if( nRowCount > 0 )
		{
			for( int counter = 0 ; counter < nRowCount ; counter = counter + 1 )
			{
				bool bStarted;
				os::ListViewStringRow* pcRow = static_cast<os::ListViewStringRow*>( m_pcServiceList->GetRow( counter ) );

				if( pcRow->GetString(1) == "Running" ) {
					bStarted = true;
				} else {
					bStarted = false;
				}
				pcSettings->SetBool( pcRow->GetString(0).c_str(), bStarted );

				pcSettings->Save();
				delete( pcSettings );
			}
		}
	} catch(...) { }
}


bool MainWindow::OkToQuit()
{
	os::Application::GetInstance()->PostMessage( os::M_QUIT );
	return( true );
}

