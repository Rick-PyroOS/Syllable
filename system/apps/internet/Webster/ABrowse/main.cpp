#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <iostream>

#include <atheos/time.h>

#include <util/application.h>
#include <util/message.h>
#include <util/messenger.h>
#include <util/clipboard.h>
#include <util/resources.h>
#include <util/settings.h>
#include <gui/window.h>
#include <gui/view.h>
#include <gui/frameview.h>
#include <gui/button.h>
#include <gui/separator.h>
#include <gui/textview.h>
#include <gui/imagebutton.h>
#include <gui/tabview.h>

#include <wtf/Platform.h>
#include <Document.h>
#include <FrameView.h>
#include <ContextMenuClientSyllable.h>
#include <FrameLoadRequest.h>
#include <FrameTree.h>
#include <SharedTimer.h>
#include <ResourceRequest.h>
#include <ResourceHandle.h>
#include <ResourceHandleClient.h>
#include <KURL.h>
#include <Cache.h>
#include <PageCache.h>
#include <SelectionController.h>
#include <FocusController.h>
#include <CString.h>

#include <Frame.h>
#include <FrameLoader.h>
#include <page/Page.h>
#include <string>
#include <list>

#include <gui/menu.h>
#include <gui/requesters.h>
#include <storage/path.h>
#include <storage/file.h>
#include <storage/directory.h>
#include <storage/tempfile.h>
#include <storage/registrar.h>
#include <ResourceHandleManager.h>

#include "toolbar.h"
#include "statusbar.h"
#include "searchdialog.h"
#include "downloaddialog.h"

#include "bookmarks.h"
#include "main.h"
#include "version.h"

#include "HTMLView.h"
#include "ChromeClientSyllable.h"


using namespace os;

class PrefsWindow : public os::Window
{
public:
	PrefsWindow( const os::Rect& cFrame, os::Looper* pcTarget, os::String zHome, os::String zProxy ) : os::Window( cFrame, "prefs", "Settings" )
	{
		os::LayoutView* pcView = new os::LayoutView( GetBounds(), "layout" );
		#include "PrefsLayout.cpp"
		SetDefaultButton( m_pcOk );
		m_pcRoot->SameWidth( "Home", "Proxy", NULL );
		m_pcHomeURL->Set( zHome.c_str() );
		m_pcProxyURL->Set( zProxy.c_str() );
		m_pcHomeURL->SetTabOrder( 0 );
		m_pcProxyURL->SetTabOrder( 1 );
		m_pcOk->SetTabOrder( 2 );
		m_pcCancel->SetTabOrder( 3 );
		pcView->SetRoot( m_pcRoot );
		AddChild( pcView );
		m_pcTarget = pcTarget;
	}
	void HandleMessage( os::Message* pcMsg )
	{
		switch( pcMsg->GetCode() )
		{
			case ID_SET_SETTINGS:
			{
				os::Message cMsg( ID_SET_SETTINGS );
				cMsg.AddString( "home", m_pcHomeURL->GetBuffer()[0] );
				cMsg.AddString( "proxy", m_pcProxyURL->GetBuffer()[0] );
				m_pcTarget->PostMessage( &cMsg, m_pcTarget );
				PostMessage( os::M_QUIT );
				break;
			}
			default:
				os::Window::HandleMessage( pcMsg );
		}
	}

private:
	os::Looper* m_pcTarget;
	#include "PrefsLayout.h"
};

/* WebCore::ChromeClient implementation (TODO: still incomplete) */
namespace os
{
	
class BrowserChromeClient : public WebCore::ChromeClientSyllable
{
public:
	BrowserChromeClient( BrowserWindow* pcWindow )
	{
		m_pcWindow = pcWindow;
	}
	void setWindowRect(const WebCore::FloatRect& cRect)
	{
		/* We do not want to do this if we use tabs */
		//m_pcWindow->SetFrame( cRect );
	}
	WebCore::FloatRect windowRect()
	{
		return( m_pcWindow->GetFrame() );
	}
	WebCore::FloatRect pageRect()
	{
		return( m_pcWindow->GetTabs()->GetTabView( m_pcWindow->GetTabs()->GetSelection() )->GetFrame() );
	}
	float scaleFactor()
	{
		return 1;
	}
	void focus()
	{
		m_pcWindow->MakeFocus();
	}
	void unfocus()
	{
		m_pcWindow->MakeFocus( false );
	}
	void show()
	{
	}
	virtual WebCore::Page* createWindow(WebCore::Frame*, const WebCore::FrameLoadRequest& cRequest, const WebCore::WindowFeatures& )
	{
		/* TODO: Add the possibility to open a real window */
		WebCore::Frame* pcFrame = m_pcWindow->CreateNewTab( cRequest );
		return( pcFrame->page() );
	}
#if 0
	virtual WebCore::Page* createModalDialog(WebCore::Frame* pcFrame, const WebCore::FrameLoadRequest& cRequest)
	{
		/* No modal windows on syllable */
		return( createWindow( pcFrame, cRequest ) );
	}
#endif
	void setStatusbarText(const WebCore::String& cString)
	{
		m_pcWindow->SetStatusBarText( cString );
	}
private:
	BrowserWindow* m_pcWindow;
};

}

//----------------------------------------------------------------------------
// NAME:
// DESC:
// NOTE:
// SEE ALSO:
//----------------------------------------------------------------------------

std::vector<BrowserWindow*> BrowserWindow::s_cWindows;

TextView *BrowserWindow::GetURLView()
{
	return ( m_pcURLView );
}

WebCore::Frame *BrowserWindow::GetWebCoreFrame()
{
	uint i = m_pcTabs->GetSelection();
	HTMLView *hview = ( HTMLView * ) m_pcTabs->GetTabView( i );

	return ( hview->GetWebCoreFrame() );
}

void BrowserWindow::SwitchTab()
{
	uint i = m_pcTabs->GetSelection();
	HTMLView *hview = ( HTMLView * ) m_pcTabs->GetTabView( i );

	m_pcURLView->Set( hview->GetCurrentURL().c_str(  ), false );
	SetTitle( m_pcTabs->GetTabTitle( i ) + " - ABrowse" );
	std::string cURL = m_pcURLView->GetBuffer()[0];

	os::View *pcView = hview->GetWebCoreFrame()->view()->syllableWidget();

	pcView->SetResizeMask( CF_FOLLOW_ALL );

	os::font_height sFontHeight;
	m_pcTabs->GetFontHeight( &sFontHeight );
	float vGlyphHeight = sFontHeight.ascender + sFontHeight.descender + sFontHeight.line_gap;
	float vTabHeight = ceil( vGlyphHeight * 1.2f + 6.0f );

	os::Rect cClientBounds = m_pcTabs->GetBounds();
	cClientBounds.Resize( 0.0f, 0.0f, -4.0f, -2.0f - vTabHeight );
	cClientBounds.Floor();

	pcView->SetFrame( cClientBounds );

	pcView->MakeFocus();

//	pcView->SetMsgTarget( Messenger( this ) );

	
	GoHistory( 0, true, true );

}

void BrowserWindow::SetStatusBarText( const os::String& cText )
{
	m_pcStatusBar->SetStatus( cText );
}

os::Point BrowserWindow::GetNewWinPosition()
{
	static Point cLastPos( 100, 100 );

	cLastPos.x += 30;
	cLastPos.y += 30;
	if ( cLastPos.x > 500 )
	{
		cLastPos.x = 100;
	}
	if ( cLastPos.y > 500 )
	{
		cLastPos.y = 100;
	}
	return ( cLastPos );
}

void BrowserWindow::CreateNewTab()
{
	//WebCore::Page* pcPage = new WebCore::Page(new WebCore::ChromeClientSyllable(), new WebCore::ContextMenuClientSyllable());
	
	HTMLView *hview = new HTMLView( m_pcTabs->GetBounds(), "html_tab", m_pcChromeClient, os::CF_FOLLOW_ALL );
	m_pcTabs->AppendTab( "  (Untitled)  ", hview );

}

extern os::Locker g_cGlobalMutex;

WebCore::Frame* BrowserWindow::CreateNewTab( const WebCore::FrameLoadRequest& cRequest )
{
	
	HTMLView *hview = new HTMLView( m_pcTabs->GetBounds(), "html_tab", m_pcChromeClient, os::CF_FOLLOW_ALL );

	m_pcTabs->AppendTab( "  (Untitled)  ", hview );
	hview->OpenURL( cRequest.resourceRequest() );

	return( hview->GetWebCoreFrame() );
}

void BrowserWindow::DeleteTab()
{
	uint i = m_pcTabs->GetSelection();
	int nt = m_pcTabs->GetTabCount();

	if ( nt > 1 )
	{
		delete( m_pcTabs->DeleteTab( i ) );
		GoHistory( 0, true, true );
	}
}

BrowserWindow::BrowserWindow( const Rect & cFrame, bool bSaveSize ):Window( cFrame, "abrowse_wnd", "ABrowse" )
{
	m_bSaveSize = bSaveSize;
	m_pcSearchDialog = NULL;
	m_bSearchCaseSensitive = false;
	m_bFindActive = false;
	char *pzHome = getenv( "HOME" );
	
	m_pcChromeClient = new BrowserChromeClient( this );

	s_cWindows.push_back( this );

	g_cGlobalMutex.Lock();
	SetMutex( &g_cGlobalMutex );

	m_pcFrame = new LayoutView( GetBounds(), "frame_view" );

	m_pcTabs = new os::TabView( Rect( 0, 0, 0, 0 ), "tabs", CF_FOLLOW_LEFT | CF_FOLLOW_TOP, WID_WILL_DRAW | WID_CLEAR_BACKGROUND );
	m_pcTabs->SetMessage( new Message( ID_TAB_CHANGED ) );

	VLayoutNode *pcRoot = new VLayoutNode( "root" );
	m_pcMenuBar = new Menu( Rect( 0, 0, 0, 0 ), "menu_bar", ITEMS_IN_ROW );
	Menu *pcAppMenu = new Menu( Rect( 0, 0, 0, 0 ), "Application", ITEMS_IN_COLUMN );

	pcAppMenu->AddItem( "About...", new os::Message( ID_ABOUT ) );
	pcAppMenu->AddItem( new os::MenuSeparator() );
	pcAppMenu->AddItem( "Settings...", new os::Message( ID_SETTINGS ) );	
	pcAppMenu->AddItem( new os::MenuSeparator() );	
	pcAppMenu->AddItem( "Quit", new os::Message( M_QUIT ), "Ctrl+Q" );
	m_pcMenuBar->AddItem( pcAppMenu );

	Menu *pcTabMenu = new Menu( Rect( 0, 0, 0, 0 ), "Tab", ITEMS_IN_COLUMN );

	pcTabMenu->AddItem( "New Tab", new os::Message( ID_NEW_TAB ), "Ctrl+T" );
	pcTabMenu->AddItem( "Close Tab", new os::Message( ID_DELETE_TAB ), "Ctrl+W" );

	m_pcMenuBar->AddItem( pcTabMenu );
	Menu *pcEditMenu = new Menu( Rect( 0, 0, 0, 0 ), "Edit", ITEMS_IN_COLUMN );

	pcEditMenu->AddItem( "Copy", new os::Message( ID_COPY ), "Ctrl+C" );
	pcEditMenu->AddItem( "Paste", new os::Message( ID_PASTE ), "Ctrl+V" );
	m_pcMenuBar->AddItem( pcEditMenu );
	


	m_pcBookmarkMenu = new BookmarksMenu( this, os::Path( os::String ( pzHome ) + "/Bookmarks" ), os::Rect(), "Bookmarks", os::ITEMS_IN_COLUMN );

	m_pcBookmarkMenu->SetTargetForItems( this );
	m_pcMenuBar->AddItem( m_pcBookmarkMenu );
	
	m_pcMenuBar->SetTargetForItems( this );

	m_pcURLView = new TextView( Rect( 0, 0, 0, 0 ), "url_view", "" /*g_pzDefaultURL */  );

	m_pcURLView->SetEventMask( os::TextView::EI_ENTER_PRESSED | os::TextView::EI_ESC_PRESSED );
	m_pcURLView->SetMultiLine( false );
	m_pcURLView->SetMessage( new Message( ID_URL_CHANGED ) );

	m_pcToolBar = new ToolBar( Rect( 0, 0, 0, 0 ), "tool_bar" );
	m_pcToolBar->AddButton( "breaker.png", "", NULL );		
	m_pcToolBar->AddButton( "back.png", "Back", new os::Message( ID_PREV_URL ) );	
	m_pcToolBar->AddButton( "forward.png", "Forward", new os::Message( ID_NEXT_URL ) );
	m_pcToolBar->AddButton( "reload.png", "Reload", new os::Message( ID_RELOAD ) );
	m_pcStopButton = m_pcToolBar->AddButton( "stop.png", "Stop", new os::Message( ID_STOP ) );	
	m_pcToolBar->AddButton( "home.png", "Home", new os::Message( ID_GOHOME ) );
	m_pcFindButton = m_pcToolBar->AddButton( "find2.png", "Find", new os::Message( ID_FIND ) );
	m_pcFindView = new os::StringView( os::Rect(), "find", "" );
	m_pcToolBar->GetNode()->AddChild( m_pcFindView, 0.0f );		
	m_pcToolBar->GetNode()->AddChild( m_pcURLView );
	m_pcToolBar->Finish();

	m_pcStatusBar = new StatusBar( Rect(), "status_bar" );

	pcRoot->AddChild( m_pcMenuBar );
	pcRoot->AddChild( m_pcToolBar, 0.0f );

	CreateNewTab();

	pcRoot->AddChild( m_pcTabs );
	pcRoot->AddChild( m_pcStatusBar );
	m_pcFrame->SetRoot( pcRoot );

	AddChild( m_pcFrame );




	uint i = m_pcTabs->GetSelection();
	HTMLView *hview = ( HTMLView * ) m_pcTabs->GetTabView( i );
	os::View *pcView = hview->GetWebCoreFrame()->view()->syllableWidget();


	pcView->SetResizeMask( CF_FOLLOW_ALL );

	os::font_height sFontHeight;
	m_pcTabs->GetFontHeight( &sFontHeight );
	float vGlyphHeight = sFontHeight.ascender + sFontHeight.descender + sFontHeight.line_gap;
	float vTabHeight = ceil( vGlyphHeight * 1.2f + 6.0f );

	os::Rect cClientBounds = m_pcTabs->GetBounds();
	cClientBounds.Resize( 0.0f, 0.0f, -4.0f, -2.0f - vTabHeight );
	cClientBounds.Floor();
	pcView->SetFrame( cClientBounds );

	pcView->MakeFocus();

//	pcView->SetMsgTarget( Messenger( this ) );

	m_pcURLView->SetTarget( this );

	m_pcToolBar->EnableButton( ID_PREV_URL, false );
	m_pcToolBar->EnableButton( ID_NEXT_URL, false );
	m_pcToolBar->EnableButton( ID_COPY, false );

	
	AddTimer( this, 0, 100000, false );

	/* Set Icon */
	os::Resources cCol( get_image_id() );
	os::ResStream * pcStream = cCol.GetResourceStream( "icon24x24.png" );
	os::BitmapImage * pcIcon = new os::BitmapImage( pcStream );
	delete( pcStream );
	SetIcon( pcIcon->LockBitmap() );
	delete( pcIcon );
}

//----------------------------------------------------------------------------
// NAME:
// DESC:
// NOTE:
// SEE ALSO:
//----------------------------------------------------------------------------

BrowserWindow::~BrowserWindow()
{
	RemoveTimer( this, 0 );
	
	/* Delete window from list */
	Lock();
	for( uint i = 0; i < s_cWindows.size(); i++ ) {
		if( s_cWindows[i] == this )
		{
			s_cWindows.erase( s_cWindows.begin() + i );
			break;
		}
	}
	Unlock();
}


void BrowserWindow::GoHistory( int nStep, bool bButtonsOnly, bool bUpdateURL )
{

	uint i = m_pcTabs->GetSelection();
	HTMLView *hview = ( HTMLView * ) m_pcTabs->GetTabView( i );

	if ( !bButtonsOnly )
	{
		if( nStep == 1 )
			hview->GoForward();
		else if( nStep == -1 )
			hview->GoBack();
	}

	if( bUpdateURL && !m_bFindActive )
		m_pcURLView->Set( hview->GetCurrentURL().c_str(), false );
	if ( hview->GetBackListCount() == 0 )
	{
		m_pcToolBar->EnableButton( ID_PREV_URL, false );
	}
	else
	{
		m_pcToolBar->EnableButton( ID_PREV_URL, true );
	}
	if ( hview->GetForwardListCount() == 0 )
	{
		m_pcToolBar->EnableButton( ID_NEXT_URL, false );
	}
	else
	{
		m_pcToolBar->EnableButton( ID_NEXT_URL, true );
	}
}

#if 0
void dump_message( os::Message * pcMsg )
{
	int nCount = pcMsg->CountNames();

	for ( int i = 0; i < nCount; ++i )
	{
		int nType;
		int nCount;

		pcMsg->GetNameInfo( pcMsg->GetName( i ).c_str(), &nType, &nCount );
		switch ( nType )
		{
		case T_STRING:
			{
				std::string cStr;
				pcMsg->FindString( pcMsg->GetName( i ).c_str(), &cStr );
				printf( "%s: '%s'\n", pcMsg->GetName( i ).c_str(), cStr.c_str(  ) );
				break;
			}
		default:
			printf( "%s: (Type=%d)\n", pcMsg->GetName( i ).c_str(), nType );
			break;

		}
	}
}
#endif

void BrowserWindow::OpenURL( const WebCore::FrameLoadRequest& cRequest )
{
	uint i = m_pcTabs->GetSelection();
	HTMLView *hview = ( HTMLView * ) m_pcTabs->GetTabView( i );
	os::String cURL = cRequest.resourceRequest().url().string().utf8().data();
	if( !m_bFindActive )
		m_pcURLView->Set( cRequest.resourceRequest().url().string().utf8().data(), false );
	if ( cURL.find( "#" ) == std::string::npos )
		m_pcTabs->SetTabTitle( i, "Loading..." );
	hview->OpenURL( cRequest.resourceRequest() );
}

void BrowserWindow::PopupMenu( os::String& cURL )
{
	m_pcPopupMenu = new os::Menu( Rect( 0, 0, 10, 10 ), "popup", ITEMS_IN_COLUMN );
	m_pcPopupMenu->SetCloseMessage( os::Message( ID_POPUP_CLOSED ) );
	m_pcPopupMenu->SetCloseMsgTarget( this );

	os::Message * pcMsg;
	m_pcPopupMenu->AddItem( new os::MenuItem( "New window", new os::Message( ID_NEW_WINDOW ) ) );
	m_pcPopupMenu->AddItem( new os::MenuItem( "Reload", new os::Message( ID_RELOAD ) ) );

	if ( cURL.empty() == false )
	{
		m_pcPopupMenu->AddItem( new os::MenuSeparator() );

		pcMsg = new os::Message( ID_OPEN_TAB );
		pcMsg->AddString( "url", cURL );
		m_pcPopupMenu->AddItem( new os::MenuItem( "Open link in new tab", pcMsg ) );

		pcMsg = new os::Message( ID_OPEN_LINK );
		pcMsg->AddString( "url", cURL );
		m_pcPopupMenu->AddItem( new os::MenuItem( "Open link in new window", pcMsg ) );

		pcMsg = new os::Message( ID_SAVE_LINK );
		pcMsg->AddString( "url", cURL );
		m_pcPopupMenu->AddItem( new os::MenuItem( "Save link as...", pcMsg ) );

		pcMsg = new os::Message( ID_COPY_LINK_LOCATION );
		pcMsg->AddString( "url", cURL );
		m_pcPopupMenu->AddItem( new os::MenuItem( "Copy link location", pcMsg ) );
	}

	m_pcPopupMenu->AddItem( new os::MenuSeparator() );
	m_pcPopupMenu->AddItem( new os::MenuItem( "Quit", new os::Message( M_QUIT ) ) );

	m_pcPopupMenu->SetTargetForItems( this );

	os::Point cMousePos;
	m_pcStatusBar->GetMouse( &cMousePos, NULL );
	m_pcPopupMenu->Open( m_pcStatusBar->ConvertToScreen( cMousePos ) );
}

static void SetButtonImageFromResource( os::ImageButton* pcButton, os::String zResource )
{
	os::File cSelf( open_image_file( get_image_id() ) );
	os::Resources cCol( &cSelf );		
	os::ResStream *pcStream = cCol.GetResourceStream( zResource );
	pcButton->SetImage( pcStream );
	delete( pcStream );
}

void BrowserWindow::HandleMessage( Message * pcMsg )
{
	switch ( pcMsg->GetCode() )
	{
	case ID_TAB_CHANGED:
		{
			SwitchTab();
			break;
		}
	case ID_NEW_TAB:
		{
			CreateNewTab();
			break;
		}
	case ID_DELETE_TAB:
		{
			DeleteTab();
			break;
		}
	case ID_URL_CHANGED:
		{
			uint32 nEvents = 0;

			pcMsg->FindInt( "events", &nEvents );
			uint i = m_pcTabs->GetSelection();
			HTMLView *hview = ( HTMLView * ) m_pcTabs->GetTabView( i );
			
			if( m_bFindActive )
			{
				WebCore::Frame* pcFrame = hview->GetWebCoreFrame()->page()->focusController()->focusedOrMainFrame();;
				if( pcFrame == NULL )
					break;
			
				printf("Find in %s\n", os::String( pcFrame->tree()->name().domString() ).c_str() );
				pcFrame->findString( m_pcURLView->GetBuffer()[0], true, false, true, true );
				break;
			}

			if ( nEvents & os::TextView::EI_ESC_PRESSED )
			{
				m_pcURLView->Set( hview->GetCurrentURL().c_str(  ) );
				hview->GetWebCoreFrame()->view()->syllableWidget()->MakeFocus(  );
				break;
			}
			if ( nEvents & os::TextView::EI_ENTER_PRESSED )
			{
				std::string cURL = m_pcURLView->GetBuffer()[0];

				for ( uint i = 0; i <= cURL.size(); ++i )
				{
					if ( i == cURL.size() || isalnum( cURL[i] ) == false )
					{
						if ( i == cURL.size() || cURL[i] != ':' )
						{
							std::string cTmp = cURL;
							cURL = "http://";
							for ( uint j = 0; j < cTmp.size(); ++j )
							{
								if ( cTmp[j] != '/' )
								{
									cURL.insert( cURL.end(), cTmp.begin(  ) + j, cTmp.end(  ) );
									break;
								}
							}
						}
						break;
					}
				}

				
				hview->GetWebCoreFrame()->view()->syllableWidget()->MakeFocus(  );
				WebCore::ResourceRequest cRequest;
				cRequest.setURL( WebCore::KURL( cURL.c_str() ) );
				WebCore::FrameLoadRequest cFrameRequest( cRequest );
				hview->OpenURL( cFrameRequest.resourceRequest() );

			}
			break;
		}
	case ID_ACTIVATE_URLEDIT:
		m_pcURLView->MakeFocus( true );
		m_pcURLView->SetCursor( -1, 0 );
		m_pcURLView->SelectAll();
		break;
	case ID_RELOAD:
		{
			uint i = m_pcTabs->GetSelection();
			HTMLView *hview = ( HTMLView * ) m_pcTabs->GetTabView( i );
			hview->Reload();
			break;
		}
	case ID_STOP:
		{
			uint i = m_pcTabs->GetSelection();
			HTMLView *hview = ( HTMLView * ) m_pcTabs->GetTabView( i );
			hview->Stop();
			break;
		}		
	case ID_NEW_WINDOW:
		{
			BrowserWindow *pcNewWindow = new BrowserWindow( GetFrame() + os::Point( 30.0f, 30.0f ), false );

			pcNewWindow->GoHistory( 0, false, true );
			pcNewWindow->Show( true );
			pcNewWindow->MakeFocus( true );
			break;
		}
	case ID_OPEN_TAB:
		{
			std::string cURL;
			pcMsg->FindString( "url", &cURL );
			WebCore::ResourceRequest cRequest;
			cRequest.setURL( WebCore::KURL( cURL.c_str() ) );
			WebCore::FrameLoadRequest cFrameRequest( cRequest );
			CreateNewTab( cFrameRequest );
			break;
		}
	case ID_OPEN_LINK:
		{
			std::string cURL;
			pcMsg->FindString( "url", &cURL );
			BrowserWindow *pcNewWindow = new BrowserWindow( GetFrame() + os::Point( 30.0f, 30.0f ), false );

			WebCore::ResourceRequest cRequest;
			cRequest.setURL( WebCore::KURL( cURL.c_str() ) );
			WebCore::FrameLoadRequest cFrameRequest( cRequest );
			pcNewWindow->OpenURL( cFrameRequest );
			pcNewWindow->Show( true );
			pcNewWindow->MakeFocus( true );
			break;
		}
	case ID_SAVE_LINK:
		{
			os::String cURL;
			pcMsg->FindString( "url", &cURL );

			WebCore::ResourceRequest cRequest;
			cRequest.setURL( WebCore::KURL( cURL.c_str() ) );
//			WebCore::ResourceHandleClient* pcHandler = new WebCore::ResourceHandleClient();
			DownloadHandler* pcHandler = new DownloadHandler();
			WTF::RefPtr<WebCore::ResourceHandle> cHandle = WebCore::ResourceHandle::create( cRequest, pcHandler, NULL, false, false);
			//pcHandler->Start( cHandle.get() );
			break;
		}
	case ID_COPY_LINK_LOCATION:
		{
			std::string cURL;
			pcMsg->FindString( "url", &cURL );

			os::Clipboard cClipboard;
			cClipboard.Lock();
			cClipboard.Clear();
			os::Message * pcData = cClipboard.GetData();
			pcData->AddString( "text/plain", cURL );
			cClipboard.Commit();
			cClipboard.Unlock();
			break;
		}
	case ID_PASTE:
		{
			GetURLView()->Delete(  );
			GetURLView()->Paste( false );

			break;
		}
	case ID_COPY:
		{
			
			uint i = m_pcTabs->GetSelection();
			HTMLView *hview = ( HTMLView * ) m_pcTabs->GetTabView( i );
			WebCore::Frame* pcFrame = hview->GetWebCoreFrame()->page()->focusController()->focusedOrMainFrame();;
			if( pcFrame == NULL )
				break;
			
			
			if ( pcFrame->selectedText().isEmpty() )
			{
				break;
			}
			os::Clipboard cClipboard;
			cClipboard.Lock();
			cClipboard.Clear();
			os::Message * pcData = cClipboard.GetData();
			pcData->AddString( "text/plain", pcFrame->selectedText() );
			cClipboard.Commit();
			cClipboard.Unlock();
			break;
		}
	case ID_FIND:
		{
			uint i = m_pcTabs->GetSelection();
			HTMLView *hview = ( HTMLView * ) m_pcTabs->GetTabView( i );
			
			if( m_bFindActive )
			{
				m_cLastSearchStr = m_pcURLView->GetBuffer()[0];
				m_pcURLView->Set( hview->GetCurrentURL().c_str(  ), false );			
				m_pcFindView->SetString( "" );
				SetButtonImageFromResource( m_pcFindButton, "find2.png" );
			}
			else
			{
				m_pcURLView->Set( m_cLastSearchStr.c_str(), false );
				m_pcFindView->SetString( "Find " );
				SetButtonImageFromResource( m_pcFindButton, "find.png" );
			}
			m_pcToolBar->InvalidateLayout();		
			m_pcURLView->MakeFocus();
			m_pcURLView->SetCursor( m_pcURLView->GetBuffer()[0].Length(), 0 );
			
			m_bFindActive = !m_bFindActive;
		}
		break;
	#if 0
	case ID_DO_SEARCH:
		{
			bool bFromTop = false;

			pcMsg->FindBool( "from_top", &bFromTop );
			pcMsg->FindBool( "case_sensitive", &m_bSearchCaseSensitive );
			pcMsg->FindString( "string", &m_cLastSearchStr );

			
			uint i = m_pcTabs->GetSelection();
			HTMLView *hview = ( HTMLView * ) m_pcTabs->GetTabView( i );
			WebCore::Frame* pcFrame = hview->GetWebCoreFrame()->page()->focusController()->focusedOrMainFrame();;
			if( pcFrame == NULL )
				break;
			
			
			if( bFromTop )
				pcFrame->selectionController()->clear();
			printf("Find in %s\n", os::String( pcFrame->tree()->name().domString() ).c_str() );
			pcFrame->findString( m_cLastSearchStr.c_str(), true, m_bSearchCaseSensitive, true, true );
			break;
		}
	
	case ID_SEARCH_CLOSED:
		m_pcSearchDialog = NULL;
		break;
	#endif
	case ID_GOHOME:
	{
		uint i = m_pcTabs->GetSelection();
		HTMLView *hview = ( HTMLView * ) m_pcTabs->GetTabView( i );
		WebCore::ResourceRequest cRequest;
		BrowserApp* pcApp = static_cast<BrowserApp*>(os::Application::GetInstance());
		cRequest.setURL( WebCore::KURL( pcApp->GetHome().c_str() ) );
		hview->OpenURL( cRequest );
		break;
	}
	case ID_PREV_URL:
		GoHistory( -1, false, false );
		break;
	case ID_NEXT_URL:
		GoHistory( 1, false, false );
		break;
	case ID_ABOUT:
		{
			os::Alert * pcAbout = new os::Alert( "About ABrowse", "ABrowse " ABROWSE_VERSION " (Alpha)\nA WebCore based webbrowser\nWritten by Kurt Skauen, 2001\nWritten by Arno Klenke, 2004-2007\nWritten by Kristian Van Der Vliet, 2008" "\n\nABrowse is released under the Gnu Public Licencse (GPL)\nPlease see the file COPYING, distributed with ABrowse, for\nmore information\n", os::Alert::ALERT_INFO, 0x00, "Ok", NULL );
			pcAbout->Go( NULL );
			break;
		}
	case ID_ADD_BOOKMARK:
		{
			os::String zPath;
			bool bDummy;
			
			if( pcMsg->FindBool( "slave", &bDummy ) == 0 )
			{
				/* Update menu */
				m_pcBookmarkMenu->Reload();
				m_pcBookmarkMenu->SetTargetForItems( this );
				break;
			}
			
			if( pcMsg->FindString( "path", &zPath ) != 0 )
				break;
			
			
			uint i = m_pcTabs->GetSelection();
			HTMLView *hview = ( HTMLView * ) m_pcTabs->GetTabView( i );

			if ( m_pcTabs->GetTabTitle( i ) == "Loading..." )
				break;
			os::String zFile = m_pcTabs->GetTabTitle( i );

			for ( i = 0; i < zFile.Length(); i++ )
				if ( zFile[i] == '/' )
					zFile[i] = '_';
			zFile = zPath + "/" + zFile;

			try
			{
				os::String zType = "text/x-bookmark";
				os::File cFile( zFile, O_RDWR | O_CREAT | O_TRUNC );
				cFile.Write( hview->GetCurrentURL().c_str(  ), hview->GetCurrentURL(  ).Length(  ) );
				cFile.WriteAttr( "os::MimeType", O_TRUNC, ATTR_TYPE_STRING, zType.c_str(), 0, zType.Length() );
				cFile.Flush();
			} catch( ... )
			{
			}
			/* Update menu */
			m_pcBookmarkMenu->Reload();
			m_pcBookmarkMenu->SetTargetForItems( this );
			
			/* Tell the other windows */
			
			for( uint i = 0; i < s_cWindows.size(); i++ )
			{
				if( s_cWindows[i] != this )
				{
					os::Message cMsg( *pcMsg );
					cMsg.AddBool( "slave", true );
					s_cWindows[i]->PostMessage( &cMsg, s_cWindows[i] );
				}
			}
			
			break;
		}
	case ID_MANAGE_BOOKMARKS:
		{
			os::String zPath;
			if( pcMsg->FindString( "path", &zPath ) != 0 )
				break;
			/* Open a filebrowser window */
			
			if ( fork() == 0 )
			{
				set_thread_priority( -1, 0 );
				execlp( "/system/bin/FileBrowser", "/system/bin/FileBrowser", zPath.c_str(), NULL );
			}
			break;
		}
	case ID_GO_BOOKMARK:
		{
			os::String zPath;
			if( pcMsg->FindString( "file/path", &zPath ) != 0 )
				break;
			char zBuffer[PATH_MAX];
			memset( zBuffer, 0, sizeof( zBuffer ) );
			try
			{
				os::File cFile( zPath );
				if( cFile.Read( zBuffer, PATH_MAX ) <= 0 )
					break;
				WebCore::ResourceRequest cRequest;
				cRequest.setURL( WebCore::KURL( zBuffer ) );
				OpenURL( cRequest );
				//OpenURLRequest( zBuffer, KParts::URLArgs() );
			} catch( ... ) {
			}
			break;
		}
	case os::M_NODE_MONITOR:
		{
			if( m_pcBookmarkMenu->GetWindow() == NULL )
			{
				m_pcBookmarkMenu->Reload();
				m_pcBookmarkMenu->SetTargetForItems( this );
			}	
			break;
		}
	case ID_POPUP_CLOSED:
		{
			break;
		}
	case ID_SET_TITLE:
		{
			HTMLView* view = NULL;
			os::String cTitle;
			pcMsg->FindPointer( "view", (void**)&view );
			pcMsg->FindString( "title", &cTitle );
			for ( int i = 0; i < m_pcTabs->GetTabCount(); ++i )
			{
				HTMLView *hview = ( HTMLView * ) m_pcTabs->GetTabView( i );

				if ( hview == view )
				{
					if ( ( int )m_pcTabs->GetSelection() == i )
						SetTitle( cTitle + " - ABrowse" );

					if( cTitle.Length() > 32 )
					{
						cTitle.Resize( 30 );
						cTitle += "...";
					}
					m_pcTabs->SetTabTitle( i, cTitle );
					break;
				}
			}
			break;
		}
	
	case ID_URL_OPENED:
		{
			HTMLView* view = NULL;
			os::String cURL;
			bool bScroll = false;
			pcMsg->FindPointer( "view", (void**)&view );
			pcMsg->FindString( "url", &cURL );
			if( pcMsg->FindBool( "scroll", &bScroll ) != 0 )
			{
				for ( int i = 0; i < m_pcTabs->GetTabCount(); ++i )
				{
					HTMLView *hview = ( HTMLView * ) m_pcTabs->GetTabView( i );

					if ( hview == view )
					{
						m_pcTabs->SetTabTitle( i, "Loading..." );
						break;
					}
				}
			}
			if( view == ( HTMLView * ) m_pcTabs->GetTabView( m_pcTabs->GetSelection() ) )
			{
				if( !m_bFindActive )
					m_pcURLView->Set( cURL.c_str(), false );
				GoHistory( 0, true, false );
			}
			m_pcStatusBar->SetStatus( "Loading..." );
			break;
		}
	case ID_LOAD_FINISHED:
		{
			HTMLView* view = NULL;
			pcMsg->FindPointer( "view", (void**)&view );
			for ( int i = 0; i < m_pcTabs->GetTabCount(); ++i )
			{
				HTMLView *hview = ( HTMLView * ) m_pcTabs->GetTabView( i );
				

				if ( hview == view )
				{
					if( hview->IsLoading() )
						break;
					if( m_pcTabs->GetTabTitle( i ) == "Loading..." )
						m_pcTabs->SetTabTitle( i, "Unknown" );
						
					if( i == m_pcTabs->GetSelection() )
						m_pcStatusBar->SetStatus( "Load finished" );
					break;
				}
			}

			break;
		}
	case ID_SET_STATUS_BAR_TEXT:
		{
			os::String cText;
			pcMsg->FindString( "text", &cText );
			m_pcStatusBar->SetStatus( cText );
			break;
		}
	case ID_OPEN_POPUP_MENU:
		{
			os::String cUrl;
			pcMsg->FindString( "url", &cUrl );
			PopupMenu( cUrl );
			break;
		}
	case ID_SETTINGS:
		{
			BrowserApp* pcApp = static_cast<BrowserApp*>(os::Application::GetInstance());
			PrefsWindow* pcWindow = new PrefsWindow( os::Rect( 0, 0, 350, 100 ), pcApp, pcApp->GetHome(), pcApp->GetProxy() );
			pcWindow->CenterInWindow( this );
			pcWindow->Show();
			pcWindow->MakeFocus();
			break;
		}
	default:
		Window::HandleMessage( pcMsg );
		break;
	}
}

void BrowserWindow::TimerTick( int )
{
	bool bLoading = false;
	int nSelection = (int)m_pcTabs->GetSelection();
	for ( int i = 0; i < m_pcTabs->GetTabCount(); ++i )
	{
		HTMLView *hview = ( HTMLView * ) m_pcTabs->GetTabView( i );
		bool bTabLoading = hview->IsLoading();
		
		if( nSelection == i )
		{
			if( m_pcStopButton->IsEnabled() != bTabLoading )
				m_pcStopButton->SetEnable( bTabLoading );
		}
		
		if( bTabLoading ) {
			bLoading = true;
			break;
		}
	}
	
	m_pcStatusBar->SetProgress( bLoading );
}

//----------------------------------------------------------------------------
// NAME:
// DESC:
// NOTE:
// SEE ALSO:
//----------------------------------------------------------------------------

bool BrowserWindow::OkToQuit( void )
{
	if ( m_pcSearchDialog != NULL )
	{
		if ( m_pcSearchDialog->SafeLock() >= 0 )
		{
			m_pcSearchDialog->PostMessage( M_QUIT );
			m_pcSearchDialog->Unlock();
			m_pcSearchDialog = NULL;
		}
	}
	if ( s_cWindows.size() == 1 )
	{
		if ( m_bSaveSize )
		{
			BrowserApp* pcApp = static_cast<BrowserApp*>(os::Application::GetInstance());
			pcApp->SetSize( GetFrame() );			
		}
		Application::GetInstance()->PostMessage( M_TERMINATE );
	}
	return ( true );
}




//----------------------------------------------------------------------------
// NAME:
// DESC:
// NOTE:
// SEE ALSO:
//----------------------------------------------------------------------------
#include <netdb.h>
BrowserApp::BrowserApp( const char *pzName, int argc, char *argv[] ):Application( pzName )
{
//    signal( SIGCHLD, SIG_IGN );

	sethostent( 1 );
	/* Register types */
	m_pcManager = NULL;
	try
	{
		char zPath[PATH_MAX];
		memset( zPath, 0, sizeof( zPath ) );
		m_pcManager = os::RegistrarManager::Get();
		
		m_pcManager->RegisterType( "text/x-bookmark", "Bookmark" );
		m_pcManager->RegisterTypeIconFromRes( "text/x-bookmark", "text_bookmark.png" );
		m_pcManager->RegisterAsTypeHandler( "text/x-bookmark" );
		
		m_pcManager->RegisterType( "text/html", "HTML Page" );
		m_pcManager->RegisterTypeExtension( "text/html", "htm" );
		m_pcManager->RegisterTypeExtension( "text/html", "html" );
		m_pcManager->RegisterTypeIconFromRes( "text/html", "text_html.png" );
		m_pcManager->RegisterAsTypeHandler( "text/html" );
	} catch( ... ) {}


	/* Create bookmarks folder */
	char *pzHome = getenv( "HOME" );

	if ( NULL != pzHome )
	{
		try
		{
			os::Directory cDir( pzHome );
			os::Directory cBookmarkDir;
			cDir.CreateDirectory( "Bookmarks", &cBookmarkDir, cDir.GetMode() );
		}
		catch( ... )
		{
		}
	}

	
	/* Load settings */
	os::Settings * pcSettings = new os::Settings();
	m_cSize = os::Rect( 0, 0, 500, 450 );
	bool bSettingsLoaded = false;
	os::Path cHome( pzHome );
	cHome.Append( "Desktop/Welcome" );
	os::String cDefaultPath;	
	cDefaultPath = "file://";
	cDefaultPath += cHome.GetPath();
	
	if ( pcSettings->Load() == 0 )
	{
		m_cSize = pcSettings->GetRect( "frame", m_cSize );
		cDefaultPath = pcSettings->GetString( "home", cDefaultPath.c_str() );
		m_zProxy = pcSettings->GetString( "proxy", "" );
		bSettingsLoaded = true;
	}
	delete( pcSettings );
	m_zHome = cDefaultPath;

	BrowserWindow *pcWindow = new BrowserWindow( m_cSize, true );
	if( !bSettingsLoaded )
		pcWindow->CenterInScreen();
	

	/* Open the given or the default page */

	try
	{
		bool bIsBookmark = false;
		if( argc > 1 )
		{
			/* Check if the parameter is a bookmark */
			os::String zPath( argv[1] );
			os::Path cPath( argv[1] );
			if( zPath.find( "://" ) != std::string::npos )
				cDefaultPath = zPath;
			else
				cDefaultPath = cPath.GetPath();
			try 
			{
				char zAttrib[255];
				memset( zAttrib, 0, sizeof( zAttrib ) );
				os::File cFile( cDefaultPath );
				if( cFile.ReadAttr( "os::MimeType", ATTR_TYPE_STRING, zAttrib, 0, 255 ) > 0 )
					if( strcmp( zAttrib, "text/x-bookmark" ) == 0 )
						bIsBookmark = true;
			} catch( ... )
			{
			}
		} else {
			printf("Home %s\n", cDefaultPath.c_str() );
		}
		if( bIsBookmark )
		{
			os::Message* pcMsg = new os::Message( ID_GO_BOOKMARK );
			pcMsg->AddString( "file/path", cDefaultPath );
			pcWindow->PostMessage( pcMsg, pcWindow );
		}
		else {
			//pcWindow->OpenURL( cDefaultPath.str(), cArgs );
			WebCore::ResourceRequest cRequest;
			cRequest.setURL( WebCore::KURL( cDefaultPath.c_str() ) );
			pcWindow->OpenURL( cRequest );
			
		}
	} catch( ... )
	{
	}


	pcWindow->Show();
	pcWindow->MakeFocus();
}

//----------------------------------------------------------------------------
// NAME:
// DESC:
// NOTE:
// SEE ALSO:
//----------------------------------------------------------------------------

BrowserApp::~BrowserApp()
{
	/* Wait until all windows have closed */
	Lock();
	while( BrowserWindow::s_cWindows.size() > 0 )
	{
		Unlock();
		snooze( 10000 );
		Lock();
	}
	Unlock();
	
	g_cGlobalMutex.Lock();

	WebCore::pageCache()->releaseAutoreleasedPagesNow();
	(WebCore::cache())->setDisabled(true);
	WebCore::stopSharedTimer();

	g_cGlobalMutex.Unlock();
	

	if( m_pcManager )
		m_pcManager->Put();

	/* Save settings */
	os::Settings * pcSettings = new os::Settings();
	pcSettings->SetRect( "frame", m_cSize );
	pcSettings->SetString( "home", m_zHome );
	pcSettings->SetString( "proxy", m_zProxy );	
	pcSettings->Save();
	delete( pcSettings );
}

bool BrowserApp::OkToQuit()
{
	if( BrowserWindow::s_cWindows.size() == 0 )
		return( true );
	
	/* Delete window from list */
	Lock();

	for( uint i = 0; i < BrowserWindow::s_cWindows.size(); i++ ) {
		BrowserWindow::s_cWindows[i]->PostMessage( os::M_QUIT );
	}
	Unlock();
	return( false );
}

void BrowserApp::HandleMessage( os::Message* pcMsg )
{
	switch( pcMsg->GetCode() )
	{
		case ID_SET_SETTINGS:
			pcMsg->FindString( "home", &m_zHome );
			pcMsg->FindString( "proxy", &m_zProxy );
			WebCore::ProxyManager::setProxy( m_zProxy.c_str(), "http" );
			break;
		default:
			os::Application::HandleMessage( pcMsg );
	}
}

int main( int argc, char *argv[] )
{
	BrowserApp *pcApp;
	pcApp = new BrowserApp( "application/x-vnd.Syllable-Abrowse", argc, argv );	

	pcApp->Run();

#if 0
	/* XXXKV: Method no longer available in the CURL class */
	if( WebCore::ResourceHandleManager::self() != NULL )
		delete( WebCore::ResourceHandleManager::self() );
#endif
	return ( 0 );
}
