#ifndef ssize_t
typedef int ssize_t;
#define ssize_t ssize_t
#endif

#include <os/string.h>
#include <posix/unistd.h>
#include <common/debug.h>
#include <signal.h>

#include "mainwindow.h"
#include "messages.h"


MainWindow* g_pcWin;

void Process( char* szData )
{
	FN_START();
	
	FN_PRINTF("Processing data:\n");
	FN_PRINTF("%s\n", szData);
	
	if(g_pcWin) {
		g_pcWin->Insert(szData);
	}
	
	FN_END();
}

MainWindow::MainWindow()
 : Window( Rect( 0, 0, 600, 500 ), "main_wnd", "Test-libcommon::Command" )
{
	FN_START();
	
	/* Set Icon */
	Resources cCol( get_image_id() );
	ResStream *pcStream = cCol.GetResourceStream( "icon48x48.png" );
	BitmapImage *pcIcon = new BitmapImage( pcStream );
	SetIcon( pcIcon->LockBitmap() );
	delete( pcIcon );
	
	/* Setup GUI */
	m_pcDisplayView = new TextView(Rect(), "m_pcDisplayView", NULL, CF_FOLLOW_ALL);
	m_pcDisplayView->SetMultiLine(true);
	//m_pcDisplayView->SetReadOnly(true);
	
	m_pcEntryView = new TextView(Rect(), "m_pcEntryView", NULL, CF_FOLLOW_ALL);
	
	m_pcEntryButton = new Button(Rect(), "m_pcEntryButton", "Send Command", new Message(M_COMMAND_SEND), CF_FOLLOW_ALL);
	
	m_pcClearButton = new Button(Rect(), "m_pcClearButton", "Clear View", new Message(M_VIEW_CLEAR), CF_FOLLOW_ALL);
	
	AddChild(m_pcDisplayView);
	AddChild(m_pcEntryView);
	AddChild(m_pcEntryButton);
	AddChild(m_pcClearButton);
	
	/* Setup App */
	g_pcWin = this;
	m_pcCommand = new Command((callback)&Process);
	
	_UpdateLayout();
	
	FN_END();
}

void MainWindow::HandleMessage( Message* pcMessage )
{
	FN_START();
	
	uint32 nCode = pcMessage->GetCode();
	String cTemp;
	char* szCmd = NULL;
	char* szArgs = NULL;
	uint32 nSplit;
	
	FN_PRINTF("Handling message number 0x%x\n", nCode);
	
	switch( nCode ) {
		case M_APP_QUIT: {
			FN_PRINTF("Attempting to quit\n");
			while(!OkToQuit()) {
			}
		} break;
		case M_COMMAND_SEND: {
			cTemp = m_pcEntryView->GetValue().AsString();
			if((nSplit = cTemp.find(" ")) != cTemp.npos) {
				/* Get command */
				String cCmd(cTemp, 0, nSplit);
				szCmd = (char*)cCmd.c_str();
				
				/* Get arguments */
				String cArgs(cTemp, nSplit + 1, cTemp.size() - (nSplit + 1));
				szArgs = (char*)cArgs.c_str();
			} else {
				szCmd = (char*)cTemp.c_str();
			}
			
			char* szExecute[] = { szCmd, szArgs, NULL };
			FN_PRINTF("Executing command:  %s %s\n", szExecute[0], szExecute[1]);
			m_pcCommand->Execute(szExecute);
			m_pcEntryView->Clear();
		} break;
		case M_VIEW_CLEAR: {
			FN_PRINTF("Clearing view\n");
			m_pcDisplayView->Clear();
		} break;
		default:break;
	}
	
	FN_END();
}

void MainWindow::Insert( char* szData )
{
	FN_START();
	
	m_pcDisplayView->Insert(szData);
	
	FN_END();
}

bool MainWindow::OkToQuit()  // Obsolete?
{
	FN_START();
	
	FN_PRINTF("Telling app to quit.\n");
	
	Application::GetInstance()->PostMessage( M_QUIT );
	
	FN_END();
	return( true );
}

void MainWindow::_UpdateLayout()
{
	FN_START();
	
	Rect cWindowBounds;
	
	float fSpacing = 4;
	
	Rect cDisplayViewBounds;
	Point cDisplayViewSize;
	
	Rect cEntryViewBounds;
	Point cEntryViewSize;
	
	Rect cEntryButtonBounds;
	Point cEntryButtonSize;
	
	Rect cClearButtonBounds;
	Point cClearButtonSize;
	
	/* The window's bounds are the basis for the rest of the measurements */
	cWindowBounds = GetBounds();
	
	/* Set Buttons */
	FN_PRINTF("Seting up clear button\n");
	cClearButtonSize = m_pcClearButton->GetPreferredSize(false);
	cClearButtonBounds.left = cWindowBounds.left + fSpacing;
	cClearButtonBounds.right = cWindowBounds.right - fSpacing;
	cClearButtonBounds.bottom = cWindowBounds.bottom - fSpacing;
	cClearButtonBounds.top = cClearButtonBounds.bottom - cClearButtonSize.y;
	m_pcClearButton->SetFrame(cClearButtonBounds);
	
	FN_PRINTF("Seting up entry button\n");
	cEntryButtonSize = m_pcEntryButton->GetPreferredSize(false);
	cEntryButtonBounds.left = cWindowBounds.left + fSpacing;
	cEntryButtonBounds.right = cWindowBounds.right - fSpacing;
	cEntryButtonBounds.bottom = cClearButtonBounds.top - fSpacing;
	cEntryButtonBounds.top = cEntryButtonBounds.bottom - cEntryButtonSize.y;
	m_pcEntryButton->SetFrame(cEntryButtonBounds);
	
	/* Set Views */
	FN_PRINTF("Seting up entry view\n");
	cEntryViewSize = m_pcEntryView->GetPreferredSize(false);
	cEntryViewBounds.left = cWindowBounds.left + fSpacing;
	cEntryViewBounds.right = cWindowBounds.right - fSpacing;
	cEntryViewBounds.bottom = cEntryButtonBounds.top - fSpacing;
	cEntryViewBounds.top = cEntryViewBounds.bottom - cEntryViewSize.y;
	m_pcEntryView->SetFrame(cEntryViewBounds);
	
	FN_PRINTF("Seting up display view\n");
	cDisplayViewSize = m_pcDisplayView->GetPreferredSize(false);
	cDisplayViewBounds.left = cWindowBounds.left + fSpacing;
	cDisplayViewBounds.right = cWindowBounds.right - fSpacing;
	cDisplayViewBounds.bottom = cEntryViewBounds.top - fSpacing;
	cDisplayViewBounds.top = cWindowBounds.top + fSpacing;
	m_pcDisplayView->SetFrame(cDisplayViewBounds);
	
	FN_END();
}






