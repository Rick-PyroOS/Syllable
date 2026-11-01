#include <gui/window.h>
#include <gui/rect.h>
#include <gui/textview.h>
#include <gui/requesters.h>
#include <util/application.h>
#include <util/message.h>
#include <util/string.h>
#include <util/locker.h>

#include <webview/webview.h>
#include <webview/chromeclient.h>
#include <webview/websettings.h>

using namespace os;

class TestWebView : public WebView
{
	public:
		TestWebView( const Rect& cFrame, const String& cTitle, SimpleChromeClient* pcClient, WebSettings *pcSettings, uint32 nResizeMask = CF_FOLLOW_LEFT | CF_FOLLOW_TOP, uint32 nFlags = WID_WILL_DRAW | WID_CLEAR_BACKGROUND );

		virtual void SaveLink( WebView *pcWebView, String &cURL );
		virtual void SetTitle( WebView *pcWebView, String &cTitle );
		virtual void LoadStarted( WebView *pcWebView, String &cURL );
		virtual void LoadFinished( WebView *pcWebView );
};

class TestWindow : public Window
{
	public:
		TestWindow( const Rect & cFrame );
		~TestWindow();

		void HandleMessage( Message * pcMsg );
		bool OkToQuit( void );

		void OpenURL( String cURL );

	private:
		SimpleChromeClient *m_pcChromeClient;
		WebSettings *m_pcWebSettings;
		TestWebView *m_pcWebView;
		TextView *m_pcTextView;
};
#define ID_URL_CHANGED	(ID_LOAD_FINISHED + 100)

TestWebView::TestWebView( const Rect& cFrame, const String& cTitle, SimpleChromeClient* pcClient, WebSettings *pcSettings, uint32 nResizeMask, uint32 nFlags )
	: WebView( cFrame, cTitle, pcClient, pcSettings, nResizeMask, nFlags )
{
}

void TestWebView::SaveLink( WebView *pcWebView, String &cURL )
{
	printf("TestWebView::SaveLink( %s )\n", cURL.c_str());
	WebView::SaveLink( pcWebView, cURL );
}

void TestWebView::SetTitle( WebView *pcWebView, String &cTitle )
{
	GetWindow()->SetTitle( cTitle );
}

void TestWebView::LoadStarted( WebView *pcWebView, String &cURL )
{
	printf("TestWebView::LoadStarted()\n");
}

void TestWebView::LoadFinished( WebView *pcWebView )
{
	printf("TestWebView::LoadFinished()\n");
}

/* The global mutex is supplied by WebCore */
extern Locker g_cGlobalMutex;

TestWindow::TestWindow( const Rect &cFrame ) : Window( cFrame, "webview_wnd", "WebView Test Shell" )
{
	Rect cTextBounds, cWebBounds;

	cTextBounds = cWebBounds = GetBounds();

	m_pcChromeClient = new SimpleChromeClient( this );
	m_pcWebSettings = new WebSettings();
	m_pcWebSettings->Load();

	/* DANGER WILL ROBINSON! See the note in the TestApp constructor about this mutex! */
	g_cGlobalMutex.Lock();
	SetMutex( &g_cGlobalMutex );

	m_pcTextView = new TextView( Rect(), "textview", "", CF_FOLLOW_LEFT | CF_FOLLOW_RIGHT | CF_FOLLOW_TOP );
	m_pcTextView->SetMultiLine( false );
	m_pcTextView->SetEventMask( TextView::EI_ENTER_PRESSED );
	m_pcTextView->SetMessage( new Message( ID_URL_CHANGED ) );

	cTextBounds.bottom = m_pcTextView->GetPreferredSize(true).y;
	m_pcTextView->SetFrame( cTextBounds );
	AddChild( m_pcTextView );

	cWebBounds.top = cTextBounds.bottom + 1.0f;

	m_pcWebView = new TestWebView( cWebBounds, "webview", m_pcChromeClient, m_pcWebSettings, CF_FOLLOW_ALL );
	AddChild( m_pcWebView );

	m_pcWebView->MakeFocus();
}

TestWindow::~TestWindow()
{
	delete m_pcChromeClient;

	RemoveChild( m_pcTextView );
	delete m_pcTextView;
	RemoveChild( m_pcWebView );
	delete m_pcWebView;

	m_pcWebSettings->Save();
	delete m_pcWebSettings;
}

void TestWindow::HandleMessage( Message * pcMsg )
{
	switch( pcMsg->GetCode() )
	{
		case ID_URL_CHANGED:
		{
			uint32 nEvents = 0;
			pcMsg->FindInt( "events", &nEvents );
			if( nEvents & os::TextView::EI_ENTER_PRESSED )
			{
				String cURL = m_pcTextView->GetBuffer()[0];
				OpenURL( cURL );
			}
			break;
		}
		default:
		{
			printf( "Unknown message with code #%d\n", pcMsg->GetCode() );
			Window::HandleMessage( pcMsg );
			break;
		}
	}
}

bool TestWindow::OkToQuit( void )
{
	Application::GetInstance()->PostMessage( M_TERMINATE );
	return true;
}

void TestWindow::OpenURL( String cURL )
{
	m_pcWebView->OpenURL( cURL );
	m_pcTextView->Set( cURL.c_str(), false );
}

class TestApp : public Application
{
	public:
		TestApp( const char *pzName, String cURL );
		~TestApp();

	private:
		TestWindow *m_pcWindow;
};

TestApp::TestApp( const char *pzName, String cURL ) : Application( pzName )
{
	m_pcWindow = new TestWindow( Rect( 0, 0, 600, 400 ) );
	m_pcWindow->CenterInScreen();

	/* DANGER WILL ROBINSON! The renderer MUST be started and running before we
	   call Window::Show(), because everything shares the same mutex and it is
	   VERY VERY PICKY about the order in which the locks are taken. If Show() is
	   called before the renderer has started it will lead to a deadlock on
	   g_cGlobalMutex. THIS IS A VERY BAD DESIGN, and I shall try to find a
	   sensible fix.

	   Yes, it did take me quite some time before I realised this was the case. */

	m_pcWindow->OpenURL( cURL );

	m_pcWindow->Show();
	m_pcWindow->MakeFocus();
}

TestApp::~TestApp()
{
	WebView::Shutdown();
}

int main( int argc, char *argv[] )
{
	String cURL = "";
	if( argc > 1 )
		cURL = argv[1];

	TestApp *pcApp = new TestApp( "webview_shell", cURL );
	pcApp->Run();

	return 0;
}

