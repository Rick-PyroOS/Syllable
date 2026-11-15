/*
 *  GLUT 3.7 for Syllable (Pyro), on os::GLView (Mesa's OSMesa).
 *
 *  Threads.  GLUT programs call GL from the thread that runs glutMainLoop()
 *  (the main thread), and expect their callbacks there.  Syllable's
 *  Application and Windows have message loops of their own, so:
 *
 *    - glutInit() starts a thread that creates the os::Application and runs
 *      its loop;
 *    - each GLUT window is an os::Window with one GlutView (an os::GLView);
 *      the window's thread turns input, resizes and menu choices into events
 *      on a queue, and wakes the main thread (g_hWake);
 *    - glutMainLoop() takes the events off the queue and calls the program's
 *      callbacks, reshapes and redisplays the windows that need it, runs the
 *      timers and the idle function, and sleeps on g_hWake when there is
 *      nothing to do.
 *
 *  Only the main thread renders, so GlutView uses GLView::MakeCurrent()
 *  without its lock.  A single-buffered window is shown after each display
 *  callback; a double-buffered one by glutSwapBuffers().
 *
 *  Not supported: colour-index mode (glutSetColor...), overlays (the overlay
 *  functions do nothing and glutLayerGet() says there are none), cursors,
 *  pointer warping, spaceball/dials/tablet/joystick input, game mode and
 *  video resizing.  Menus are Syllable popup menus.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>

#include <deque>
#include <vector>
#include <string>

#include <atheos/semaphore.h>
#include <atheos/threads.h>
#include <atheos/time.h>
#include <util/application.h>
#include <util/locker.h>
#include <util/message.h>
#include <gui/window.h>
#include <gui/menu.h>
#include <gui/desktop.h>
#include <gui/GLView.h>

#include <GL/osmesa.h>
#define GLUT_NO_LIB_PRAGMA
#include <GL/glut.h>

using namespace os;

/* ------------------------------------------------------------------------- */
/* Errors                                                                    */

static std::string g_cProgramName = "glut";

extern "C" void __glutWarning( char* format, ... )
{
	va_list args;
	va_start( args, format );
	fprintf( stderr, "GLUT: Warning in %s: ", g_cProgramName.c_str() );
	vfprintf( stderr, format, args );
	va_end( args );
	putc( '\n', stderr );
}

extern "C" void __glutFatalError( char* format, ... )
{
	va_list args;
	va_start( args, format );
	fprintf( stderr, "GLUT: Fatal Error in %s: ", g_cProgramName.c_str() );
	vfprintf( stderr, format, args );
	va_end( args );
	putc( '\n', stderr );
	exit( 1 );
}

extern "C" void* __glutFont( void* font )
{
	return font;
}

#define WARN( msg ) __glutWarning( (char*)( msg ) )
#define FATAL( msg ) __glutFatalError( (char*)( msg ) )

/* ------------------------------------------------------------------------- */
/* State                                                                     */

enum EventType
{
	EV_KEY, EV_KEY_UP, EV_SPECIAL, EV_SPECIAL_UP, EV_MOUSE, EV_MOTION,
	EV_PASSIVE, EV_ENTRY, EV_RESHAPE, EV_MENU, EV_CLOSE, EV_VISIBILITY
};

struct Event
{
	EventType nType;
	int nWindow;
	int nKey;		// key, special key, button, menu, state
	int nState;		// GLUT_DOWN/GLUT_UP, menu value
	int x, y;
	int nModifiers;
};

struct MenuItem_s
{
	std::string cLabel;
	int nValue;
	int nSubMenu;		// 0: an entry
};

struct GlutMenu
{
	int nId;
	void ( *pfFunc )( int );
	std::vector<MenuItem_s> cItems;
};

class GlutView;
class GlutWindow;

struct GlutWin
{
	int nId;
	GlutWindow* pcWindow;	// the top-level os::Window (a sub-window's parent's)
	GlutView* pcView;
	int nParent;		// 0: top-level
	std::vector<int> cChildren;

	int nWidth, nHeight;	// as the program last saw them
	bool bNeedReshape;
	bool bNeedDisplay;
	bool bVisible;
	bool bVisSent;
	int nMenu[3];		// attached menu per button

	void ( *pfDisplay )( void );
	void ( *pfReshape )( int, int );
	void ( *pfKeyboard )( unsigned char, int, int );
	void ( *pfKeyboardUp )( unsigned char, int, int );
	void ( *pfMouse )( int, int, int, int );
	void ( *pfMotion )( int, int );
	void ( *pfPassive )( int, int );
	void ( *pfEntry )( int );
	void ( *pfVisibility )( int );
	void ( *pfWindowStatus )( int );
	void ( *pfSpecial )( int, int, int );
	void ( *pfSpecialUp )( int, int, int );
};

struct Timer
{
	bigtime_t nDue;
	void ( *pfFunc )( int );
	int nValue;
};

static Application* g_pcApp = NULL;
static sem_id g_hAppReady = -1;
static sem_id g_hWake = -1;
static Locker g_cQueueLock( "glut_queue" );
static Locker g_cMenuLock( "glut_menus" );	// menus are read in the window threads
static std::deque<Event> g_cQueue;

static std::vector<GlutWin*> g_cWindows;	// index = id - 1; NULL once destroyed
static std::vector<GlutMenu*> g_cMenus;
static GlutWin* g_pcCurrent = NULL;
static int g_nCurrentMenu = 0;
static std::vector<Timer> g_cTimers;
static void ( *g_pfIdle )( void ) = NULL;
static void ( *g_pfMenuStatus )( int, int, int ) = NULL;

static unsigned int g_nDisplayMode = GLUT_RGBA | GLUT_SINGLE;
static int g_nInitX = -1, g_nInitY = -1;
static int g_nInitWidth = 300, g_nInitHeight = 300;
static bigtime_t g_nStartTime = 0;
static int g_nModifiers = 0;		// of the event being handled
static bool g_bInCallback = false;
static bool g_bIgnoreKeyRepeat = false;
static bool g_bInitialized = false;

static const int MSG_MENU = 0x474c4d4e;	// 'GLMN'

// Room a top-level window needs around its frame: the Dock and the title bar
// above it, the border at the sides and the bottom
static const int GLUT_DECOR_TOP = 56;
static const int GLUT_DECOR_SIDE = 6;

static void push_event( const Event& sEvent, bool bCoalesce = false )
{
	g_cQueueLock.Lock();
	if( bCoalesce && !g_cQueue.empty() )
	{
		Event& sLast = g_cQueue.back();
		if( sLast.nType == sEvent.nType && sLast.nWindow == sEvent.nWindow )
		{
			sLast = sEvent;
			g_cQueueLock.Unlock();
			unlock_semaphore( g_hWake );
			return;
		}
	}
	g_cQueue.push_back( sEvent );
	g_cQueueLock.Unlock();
	unlock_semaphore( g_hWake );
}

static int glut_modifiers( uint32 nQualifiers )
{
	int n = 0;
	if( nQualifiers & QUAL_SHIFT )
		n |= GLUT_ACTIVE_SHIFT;
	if( nQualifiers & QUAL_CTRL )
		n |= GLUT_ACTIVE_CTRL;
	if( nQualifiers & QUAL_ALT )
		n |= GLUT_ACTIVE_ALT;
	return n;
}

static GlutWin* get_window( int nId )
{
	if( nId < 1 || nId > (int)g_cWindows.size() )
		return NULL;
	return g_cWindows[nId - 1];
}

static GlutMenu* get_menu( int nId )
{
	if( nId < 1 || nId > (int)g_cMenus.size() )
		return NULL;
	return g_cMenus[nId - 1];
}

/* ------------------------------------------------------------------------- */
/* The view and the window                                                   */

class GlutView : public GLView
{
public:
	GlutView( const Rect& cFrame, int nId, const GLParams& sParams, uint32 nResize )
		: GLView( cFrame, "glut_view", sParams, nResize, WID_WILL_DRAW ),
		  m_nId( nId ), m_nButtons( 0 ), m_pcMenu( NULL )
	{
	}

	~GlutView()
	{
		delete m_pcMenu;
	}

	void FrameSized( const Point& cDelta )
	{
		GLView::FrameSized( cDelta );
		Event sEvent;
		memset( &sEvent, 0, sizeof( sEvent ) );
		sEvent.nType = EV_RESHAPE;
		sEvent.nWindow = m_nId;
		push_event( sEvent, true );
	}

	void KeyDown( const char* pzString, const char* pzRawString, uint32 nQualifiers )
	{
		Key( pzString, nQualifiers, true );
	}

	void KeyUp( const char* pzString, const char* pzRawString, uint32 nQualifiers )
	{
		Key( pzString, nQualifiers, false );
	}

	void MouseDown( const Point& cPos, uint32 nButton )
	{
		MakeFocus( true );
		int nGlutButton = glut_button( nButton );
		if( nGlutButton < 0 )
			return;
		m_nButtons |= 1 << nGlutButton;

		// a menu attached to the button opens instead
		GlutWin* pcWin = get_window( m_nId );
		int nMenu = pcWin != NULL ? pcWin->nMenu[nGlutButton] : 0;
		if( nMenu != 0 )
		{
			OpenMenu( nMenu, cPos );
			m_nButtons &= ~( 1 << nGlutButton );
			return;
		}
		Mouse( nGlutButton, GLUT_DOWN, cPos );
	}

	void MouseUp( const Point& cPos, uint32 nButton, Message* pcData )
	{
		int nGlutButton = glut_button( nButton );
		if( nGlutButton < 0 || !( m_nButtons & ( 1 << nGlutButton ) ) )
			return;
		m_nButtons &= ~( 1 << nGlutButton );
		Mouse( nGlutButton, GLUT_UP, cPos );
	}

	void MouseMove( const Point& cPos, int nCode, uint32 nButtons, Message* pcData )
	{
		Event sEvent;
		memset( &sEvent, 0, sizeof( sEvent ) );
		sEvent.nWindow = m_nId;
		sEvent.x = int( cPos.x );
		sEvent.y = int( cPos.y );
		if( nCode == MOUSE_ENTERED || nCode == MOUSE_EXITED )
		{
			sEvent.nType = EV_ENTRY;
			sEvent.nKey = nCode == MOUSE_ENTERED ? GLUT_ENTERED : GLUT_LEFT;
			push_event( sEvent );
			if( nCode == MOUSE_EXITED )
				return;
		}
		sEvent.nType = m_nButtons != 0 ? EV_MOTION : EV_PASSIVE;
		push_event( sEvent, true );
	}

	void HandleMessage( Message* pcMessage )
	{
		if( pcMessage->GetCode() == MSG_MENU )
		{
			int32 nMenu = 0, nValue = 0;
			pcMessage->FindInt32( "menu", &nMenu );
			pcMessage->FindInt32( "value", &nValue );
			Event sEvent;
			memset( &sEvent, 0, sizeof( sEvent ) );
			sEvent.nType = EV_MENU;
			sEvent.nWindow = m_nId;
			sEvent.nKey = nMenu;
			sEvent.nState = nValue;
			push_event( sEvent );
			return;
		}
		GLView::HandleMessage( pcMessage );
	}

	int m_nId;

private:
	static int glut_button( uint32 nButton )
	{
		switch( nButton )
		{
		case MBI_LEFT:
			return GLUT_LEFT_BUTTON;
		case MBI_MIDDLE:
			return GLUT_MIDDLE_BUTTON;
		case MBI_RIGHT:
			return GLUT_RIGHT_BUTTON;
		}
		return -1;
	}

	void Mouse( int nButton, int nState, const Point& cPos )
	{
		Event sEvent;
		memset( &sEvent, 0, sizeof( sEvent ) );
		sEvent.nType = EV_MOUSE;
		sEvent.nWindow = m_nId;
		sEvent.nKey = nButton;
		sEvent.nState = nState;
		sEvent.x = int( cPos.x );
		sEvent.y = int( cPos.y );
		sEvent.nModifiers = glut_modifiers( GetQualifiers() );
		push_event( sEvent );
	}

	void Key( const char* pzString, uint32 nQualifiers, bool bDown )
	{
		if( pzString == NULL || pzString[0] == '\0' )
			return;
		if( bDown && g_bIgnoreKeyRepeat && ( nQualifiers & QUAL_REPEAT ) )
			return;

		Event sEvent;
		memset( &sEvent, 0, sizeof( sEvent ) );
		sEvent.nWindow = m_nId;
		sEvent.nModifiers = glut_modifiers( nQualifiers );
		Point cMouse;
		uint32 nButtons;
		GetMouse( &cMouse, &nButtons );
		sEvent.x = int( cMouse.x );
		sEvent.y = int( cMouse.y );

		unsigned char c = (unsigned char)pzString[0];
		int nSpecial = 0;
		bool bCtrl = ( nQualifiers & QUAL_CTRL ) != 0;
		if( pzString[1] == '\0' )
		{
			switch( c )
			{
			case VK_LEFT_ARROW:	nSpecial = GLUT_KEY_LEFT; break;
			case VK_RIGHT_ARROW:	nSpecial = GLUT_KEY_RIGHT; break;
			case VK_UP_ARROW:	nSpecial = GLUT_KEY_UP; break;
			case VK_DOWN_ARROW:	nSpecial = GLUT_KEY_DOWN; break;
			// these share their codes with Ctrl+letter
			case VK_HOME:	 if( !bCtrl ) nSpecial = GLUT_KEY_HOME; break;
			case VK_END:	 if( !bCtrl ) nSpecial = GLUT_KEY_END; break;
			case VK_PAGE_UP: if( !bCtrl ) nSpecial = GLUT_KEY_PAGE_UP; break;
			case VK_PAGE_DOWN: if( !bCtrl ) nSpecial = GLUT_KEY_PAGE_DOWN; break;
			case VK_INSERT:	 if( !bCtrl ) nSpecial = GLUT_KEY_INSERT; break;
			case VK_FUNCTION_KEY:
				{
					Message* pcMsg = GetLooper() != NULL ? GetLooper()->GetCurrentMessage() : NULL;
					int32 nRaw = 0;
					if( pcMsg == NULL || pcMsg->FindInt32( "_raw_key", &nRaw ) != 0 )
						return;
					// F1..F12 are raw keys 2..13
					if( nRaw < 2 || nRaw > 13 )
						return;
					nSpecial = GLUT_KEY_F1 + nRaw - 2;
				}
				break;
			case VK_ENTER:
				c = '\r';	// GLUT programs look for 13
				break;
			}
		}
		else if( c >= 0x80 )
		{
			// UTF-8: GLUT has bytes; pass Latin-1 through, drop the rest
			unsigned char c2 = (unsigned char)pzString[1];
			if( c >= 0xc2 && c <= 0xc3 && pzString[2] == '\0' )
				c = (unsigned char)( ( ( c & 0x1f ) << 6 ) | ( c2 & 0x3f ) );
			else
				return;
		}

		if( nSpecial != 0 )
		{
			sEvent.nType = bDown ? EV_SPECIAL : EV_SPECIAL_UP;
			sEvent.nKey = nSpecial;
		}
		else
		{
			sEvent.nType = bDown ? EV_KEY : EV_KEY_UP;
			sEvent.nKey = c;
		}
		push_event( sEvent );
	}

	void AddItems( Menu* pcMenu, int nMenuId, int nDepth )
	{
		GlutMenu* pcGlutMenu = get_menu( nMenuId );
		if( pcGlutMenu == NULL || nDepth > 8 )
			return;
		for( size_t i = 0; i < pcGlutMenu->cItems.size(); i++ )
		{
			const MenuItem_s& sItem = pcGlutMenu->cItems[i];
			if( sItem.nSubMenu != 0 )
			{
				Menu* pcSub = new Menu( Rect(), sItem.cLabel.c_str(), ITEMS_IN_COLUMN );
				AddItems( pcSub, sItem.nSubMenu, nDepth + 1 );
				pcMenu->AddItem( pcSub );
			}
			else
			{
				Message* pcMsg = new Message( MSG_MENU );
				pcMsg->AddInt32( "menu", nMenuId );
				pcMsg->AddInt32( "value", sItem.nValue );
				pcMenu->AddItem( sItem.cLabel.c_str(), pcMsg );
			}
		}
	}

	void OpenMenu( int nMenuId, const Point& cPos )
	{
		// the last menu has closed by now: a new one replaces it
		delete m_pcMenu;
		m_pcMenu = new Menu( Rect(), "glut_menu", ITEMS_IN_COLUMN );
		g_cMenuLock.Lock();
		AddItems( m_pcMenu, nMenuId, 0 );
		g_cMenuLock.Unlock();
		m_pcMenu->SetTargetForItems( this );
		m_pcMenu->Open( ConvertToScreen( cPos ) );
	}

	uint32 m_nButtons;	// GLUT buttons down (bit per button)
	Menu* m_pcMenu;
};

class GlutWindow : public Window
{
public:
	GlutWindow( const Rect& cFrame, int nId, const char* pzTitle )
		: Window( cFrame, "glut_window", pzTitle, 0, CURRENT_DESKTOP ), m_nId( nId )
	{
	}

	// GLUT programs end when a window is closed: the main thread exits
	bool OkToQuit()
	{
		Event sEvent;
		memset( &sEvent, 0, sizeof( sEvent ) );
		sEvent.nType = EV_CLOSE;
		sEvent.nWindow = m_nId;
		push_event( sEvent );
		return false;
	}

	int m_nId;
};

/* ------------------------------------------------------------------------- */
/* Initialization                                                            */

static int32 app_thread( void* pData )
{
	std::string cSignature = "application/x-vnd.glut-" + g_cProgramName;
	for( size_t i = 0; i < cSignature.size(); i++ )
		if( cSignature[i] == ' ' )
			cSignature[i] = '_';
	g_pcApp = new Application( cSignature.c_str() );
	unlock_semaphore( g_hAppReady );
	g_pcApp->Run();
	return 0;
}

static void glut_init_once()
{
	if( g_bInitialized )
		return;
	g_bInitialized = true;
	g_nStartTime = get_system_time();
	g_hWake = create_semaphore( "glut_wake", 0, 0 );
	g_hAppReady = create_semaphore( "glut_app", 0, 0 );
	thread_id hThread = spawn_thread( "glut_application", (void*)app_thread,
		NORMAL_PRIORITY, 0, NULL );
	if( hThread < 0 )
		FATAL( "cannot start the application thread" );
	resume_thread( hThread );
	lock_semaphore( g_hAppReady );
}

void glutInit( int* argcp, char** argv )
{
	if( argv != NULL && argcp != NULL && *argcp > 0 && argv[0] != NULL )
	{
		const char* pzName = strrchr( argv[0], '/' );
		g_cProgramName = pzName != NULL ? pzName + 1 : argv[0];
	}

	// the X11 options GLUT knows that mean something here
	if( argcp != NULL && argv != NULL )
	{
		int nOut = 1;
		for( int i = 1; i < *argcp; i++ )
		{
			if( strcmp( argv[i], "-geometry" ) == 0 && i + 1 < *argcp )
			{
				int x, y;
				unsigned int w, h;
				const char* g = argv[++i];
				if( sscanf( g, "%ux%u+%d+%d", &w, &h, &x, &y ) == 4 )
				{
					g_nInitWidth = w; g_nInitHeight = h;
					g_nInitX = x; g_nInitY = y;
				}
				else if( sscanf( g, "%ux%u", &w, &h ) == 2 )
				{
					g_nInitWidth = w; g_nInitHeight = h;
				}
				continue;
			}
			if( strcmp( argv[i], "-display" ) == 0 && i + 1 < *argcp )
			{
				i++;
				continue;
			}
			if( strcmp( argv[i], "-direct" ) == 0 || strcmp( argv[i], "-indirect" ) == 0 ||
			    strcmp( argv[i], "-iconic" ) == 0 || strcmp( argv[i], "-gldebug" ) == 0 ||
			    strcmp( argv[i], "-sync" ) == 0 )
				continue;
			argv[nOut++] = argv[i];
		}
		*argcp = nOut;
		argv[nOut] = NULL;
	}
	glut_init_once();
}

void glutInitDisplayMode( unsigned int nMode )
{
	g_nDisplayMode = nMode;
}

void glutInitDisplayString( const char* pzString )
{
	// the common words only
	unsigned int nMode = GLUT_RGBA;
	if( strstr( pzString, "double" ) != NULL )
		nMode |= GLUT_DOUBLE;
	if( strstr( pzString, "depth" ) != NULL )
		nMode |= GLUT_DEPTH;
	if( strstr( pzString, "stencil" ) != NULL )
		nMode |= GLUT_STENCIL;
	if( strstr( pzString, "acc" ) != NULL )
		nMode |= GLUT_ACCUM;
	if( strstr( pzString, "alpha" ) != NULL )
		nMode |= GLUT_ALPHA;
	g_nDisplayMode = nMode;
}

void glutInitWindowPosition( int x, int y )
{
	g_nInitX = x;
	g_nInitY = y;
}

void glutInitWindowSize( int nWidth, int nHeight )
{
	g_nInitWidth = nWidth;
	g_nInitHeight = nHeight;
}

/* ------------------------------------------------------------------------- */
/* Windows                                                                   */

static GLParams display_mode_params()
{
	GLParams sParams;
	if( g_nDisplayMode & GLUT_DOUBLE )
		sParams.flags |= SYLGL_DOUBLE_BUFFER;
	if( g_nDisplayMode & GLUT_DEPTH )
		sParams.depth_bits = 16;
	if( g_nDisplayMode & GLUT_STENCIL )
		sParams.stencil_bits = 8;
	if( g_nDisplayMode & GLUT_ACCUM )
		sParams.accum_bits = 16;
	if( g_nDisplayMode & GLUT_INDEX )
		WARN( "colour index mode is not supported: using RGBA" );
	return sParams;
}

static GlutWin* new_window_state()
{
	GlutWin* pcWin = new GlutWin;
	pcWin->nId = 0;
	pcWin->pcWindow = NULL;
	pcWin->pcView = NULL;
	pcWin->nParent = 0;
	pcWin->nWidth = pcWin->nHeight = 0;
	pcWin->bNeedReshape = true;
	pcWin->bNeedDisplay = true;
	pcWin->bVisible = true;
	pcWin->bVisSent = false;
	pcWin->nMenu[0] = pcWin->nMenu[1] = pcWin->nMenu[2] = 0;
	pcWin->pfDisplay = NULL;
	pcWin->pfReshape = NULL;
	pcWin->pfKeyboard = NULL;
	pcWin->pfKeyboardUp = NULL;
	pcWin->pfMouse = NULL;
	pcWin->pfMotion = NULL;
	pcWin->pfPassive = NULL;
	pcWin->pfEntry = NULL;
	pcWin->pfVisibility = NULL;
	pcWin->pfWindowStatus = NULL;
	pcWin->pfSpecial = NULL;
	pcWin->pfSpecialUp = NULL;
	g_cWindows.push_back( pcWin );
	pcWin->nId = g_cWindows.size();
	return pcWin;
}

static void make_current( GlutWin* pcWin )
{
	g_pcCurrent = pcWin;
	if( pcWin != NULL && pcWin->pcView != NULL )
		pcWin->pcView->MakeCurrent();
}

int glutCreateWindow( const char* pzTitle )
{
	glut_init_once();
	GlutWin* pcWin = new_window_state();

	int w = g_nInitWidth > 0 ? g_nInitWidth : 300;
	int h = g_nInitHeight > 0 ? g_nInitHeight : 300;

	// Where the program asked for, if the window and its decoration fit on
	// the screen there, below the Dock.  Otherwise centred, later windows
	// cascading from there.  Programs often ask for (0, 0), like gears: on
	// X11 the window manager moves such a window to have its title bar on the
	// screen, but a Syllable window's frame is its client area, so there the
	// title bar would be above the screen and the window under the Dock.
	IPoint cRes = Desktop().GetResolution();
	int x = g_nInitX, y = g_nInitY;
	bool bCenter = x < GLUT_DECOR_SIDE || y < GLUT_DECOR_TOP ||
		x + w + GLUT_DECOR_SIDE > cRes.x || y + h + GLUT_DECOR_SIDE > cRes.y;

	pcWin->pcWindow = new GlutWindow( Rect( 0, 0, w - 1, h - 1 ), pcWin->nId,
		pzTitle != NULL ? pzTitle : "" );
	if( bCenter )
	{
		pcWin->pcWindow->CenterInScreen();
		int n = ( pcWin->nId - 1 ) % 10;
		Rect cFrame = pcWin->pcWindow->GetFrame();
		x = int( cFrame.left ) + 30 * n;
		y = int( cFrame.top ) + 30 * n;
		// a window too big for the screen: its top left corner on it
		if( x < GLUT_DECOR_SIDE )
			x = GLUT_DECOR_SIDE;
		if( y < GLUT_DECOR_TOP )
			y = GLUT_DECOR_TOP;
	}
	pcWin->pcWindow->MoveTo( x, y );
	pcWin->pcView = new GlutView( pcWin->pcWindow->GetBounds(), pcWin->nId,
		display_mode_params(), CF_FOLLOW_ALL );
	pcWin->pcWindow->AddChild( pcWin->pcView );
	pcWin->pcWindow->SetFocusChild( pcWin->pcView );
	pcWin->pcWindow->Show();
	pcWin->pcWindow->MakeFocus( true );

	make_current( pcWin );
	return pcWin->nId;
}

int glutCreateSubWindow( int nParent, int x, int y, int w, int h )
{
	GlutWin* pcParent = get_window( nParent );
	if( pcParent == NULL )
	{
		WARN( "glutCreateSubWindow: no such window" );
		return 0;
	}
	GlutWin* pcWin = new_window_state();
	pcWin->nParent = nParent;
	pcWin->pcWindow = pcParent->pcWindow;
	pcParent->cChildren.push_back( pcWin->nId );

	pcWin->pcView = new GlutView( Rect( x, y, x + w - 1, y + h - 1 ), pcWin->nId,
		display_mode_params(), CF_FOLLOW_LEFT | CF_FOLLOW_TOP );
	if( pcWin->pcWindow->Lock() == 0 )
	{
		pcParent->pcView->AddChild( pcWin->pcView );
		pcWin->pcWindow->Unlock();
	}
	make_current( pcWin );
	return pcWin->nId;
}

void glutDestroyWindow( int nId )
{
	GlutWin* pcWin = get_window( nId );
	if( pcWin == NULL )
		return;
	while( !pcWin->cChildren.empty() )
		glutDestroyWindow( pcWin->cChildren.back() );

	if( g_pcCurrent == pcWin )
	{
		g_pcCurrent = NULL;
		OSMesaMakeCurrent( NULL, NULL, GL_UNSIGNED_BYTE, 0, 0 );
	}
	g_cWindows[nId - 1] = NULL;

	if( pcWin->nParent != 0 )
	{
		GlutWin* pcParent = get_window( pcWin->nParent );
		if( pcParent != NULL )
		{
			std::vector<int>& c = pcParent->cChildren;
			for( size_t i = 0; i < c.size(); i++ )
				if( c[i] == nId )
				{
					c.erase( c.begin() + i );
					break;
				}
		}
		if( pcWin->pcWindow->Lock() == 0 )
		{
			pcWin->pcView->RemoveThis();
			pcWin->pcWindow->Unlock();
		}
		delete pcWin->pcView;
	}
	else
	{
		// the window deletes its views
		pcWin->pcWindow->Terminate();
	}
	delete pcWin;
}

int glutGetWindow( void )
{
	return g_pcCurrent != NULL ? g_pcCurrent->nId : 0;
}

void glutSetWindow( int nId )
{
	GlutWin* pcWin = get_window( nId );
	if( pcWin == NULL )
	{
		WARN( "glutSetWindow: no such window" );
		return;
	}
	if( pcWin != g_pcCurrent )
		make_current( pcWin );
}

void glutPostRedisplay( void )
{
	if( g_pcCurrent != NULL )
		g_pcCurrent->bNeedDisplay = true;
}

void glutPostWindowRedisplay( int nId )
{
	GlutWin* pcWin = get_window( nId );
	if( pcWin != NULL )
		pcWin->bNeedDisplay = true;
}

void glutSwapBuffers( void )
{
	if( g_pcCurrent != NULL && g_pcCurrent->pcView != NULL )
		g_pcCurrent->pcView->SwapBuffers();
}

void glutSetWindowTitle( const char* pzTitle )
{
	if( g_pcCurrent == NULL || g_pcCurrent->nParent != 0 )
		return;
	if( g_pcCurrent->pcWindow->Lock() == 0 )
	{
		g_pcCurrent->pcWindow->SetTitle( pzTitle );
		g_pcCurrent->pcWindow->Unlock();
	}
}

void glutSetIconTitle( const char* pzTitle )
{
}

void glutPositionWindow( int x, int y )
{
	if( g_pcCurrent == NULL )
		return;
	if( g_pcCurrent->pcWindow->Lock() != 0 )
		return;
	if( g_pcCurrent->nParent != 0 )
		g_pcCurrent->pcView->MoveTo( x, y );
	else
		g_pcCurrent->pcWindow->MoveTo( x, y );
	g_pcCurrent->pcWindow->Unlock();
}

void glutReshapeWindow( int w, int h )
{
	if( g_pcCurrent == NULL || w < 1 || h < 1 )
		return;
	if( g_pcCurrent->pcWindow->Lock() != 0 )
		return;
	if( g_pcCurrent->nParent != 0 )
		g_pcCurrent->pcView->ResizeTo( w - 1, h - 1 );
	else
		g_pcCurrent->pcWindow->ResizeTo( w - 1, h - 1 );
	g_pcCurrent->pcWindow->Unlock();
	g_pcCurrent->bNeedReshape = true;
}

void glutFullScreen( void )
{
	if( g_pcCurrent == NULL || g_pcCurrent->nParent != 0 )
		return;
	IPoint cRes = Desktop().GetResolution();
	if( g_pcCurrent->pcWindow->Lock() != 0 )
		return;
	g_pcCurrent->pcWindow->MoveTo( 0, 0 );
	g_pcCurrent->pcWindow->ResizeTo( cRes.x - 1, cRes.y - 1 );
	g_pcCurrent->pcWindow->Unlock();
	g_pcCurrent->bNeedReshape = true;
}

static void show_window( bool bShow )
{
	if( g_pcCurrent == NULL )
		return;
	if( g_pcCurrent->pcWindow->Lock() != 0 )
		return;
	if( g_pcCurrent->nParent != 0 )
		g_pcCurrent->pcView->Show( bShow );
	else
		g_pcCurrent->pcWindow->Show( bShow );
	g_pcCurrent->pcWindow->Unlock();
	g_pcCurrent->bVisible = bShow;
	g_pcCurrent->bVisSent = false;
	if( bShow )
		g_pcCurrent->bNeedDisplay = true;
}

void glutShowWindow( void )
{
	show_window( true );
}

void glutHideWindow( void )
{
	show_window( false );
}

void glutIconifyWindow( void )
{
	show_window( false );
}

void glutPopWindow( void )
{
	if( g_pcCurrent == NULL || g_pcCurrent->nParent != 0 )
		return;
	if( g_pcCurrent->pcWindow->Lock() == 0 )
	{
		g_pcCurrent->pcWindow->MakeFocus( true );
		g_pcCurrent->pcWindow->Unlock();
	}
}

void glutPushWindow( void )
{
}

void glutSetCursor( int nCursor )
{
}

void glutWarpPointer( int x, int y )
{
}

/* Overlays: none */
void glutEstablishOverlay( void )
{
	WARN( "overlays are not supported" );
}
void glutRemoveOverlay( void ) {}
void glutUseLayer( GLenum layer ) {}
void glutPostOverlayRedisplay( void ) {}
void glutPostWindowOverlayRedisplay( int win ) {}
void glutShowOverlay( void ) {}
void glutHideOverlay( void ) {}
void glutOverlayDisplayFunc( void ( *func )( void ) ) {}

/* Colour index: none */
void glutSetColor( int ndx, GLfloat red, GLfloat green, GLfloat blue )
{
	WARN( "glutSetColor: colour index mode is not supported" );
}
GLfloat glutGetColor( int ndx, int component )
{
	return -1.0f;
}
void glutCopyColormap( int win ) {}

/* ------------------------------------------------------------------------- */
/* Callbacks                                                                 */

#define SET_CALLBACK( name, member, type ) \
	void name type \
	{ \
		if( g_pcCurrent != NULL ) \
			g_pcCurrent->member = func; \
	}

void glutDisplayFunc( void ( *func )( void ) )
{
	if( g_pcCurrent == NULL )
		return;
	if( func == NULL )
		FATAL( "NULL display callback not allowed in GLUT 3.0; update your code." );
	g_pcCurrent->pfDisplay = func;
}

SET_CALLBACK( glutReshapeFunc, pfReshape, ( void ( *func )( int, int ) ) )
SET_CALLBACK( glutKeyboardFunc, pfKeyboard, ( void ( *func )( unsigned char, int, int ) ) )
SET_CALLBACK( glutKeyboardUpFunc, pfKeyboardUp, ( void ( *func )( unsigned char, int, int ) ) )
SET_CALLBACK( glutMouseFunc, pfMouse, ( void ( *func )( int, int, int, int ) ) )
SET_CALLBACK( glutMotionFunc, pfMotion, ( void ( *func )( int, int ) ) )
SET_CALLBACK( glutPassiveMotionFunc, pfPassive, ( void ( *func )( int, int ) ) )
SET_CALLBACK( glutEntryFunc, pfEntry, ( void ( *func )( int ) ) )
SET_CALLBACK( glutSpecialFunc, pfSpecial, ( void ( *func )( int, int, int ) ) )
SET_CALLBACK( glutSpecialUpFunc, pfSpecialUp, ( void ( *func )( int, int, int ) ) )

void glutVisibilityFunc( void ( *func )( int ) )
{
	if( g_pcCurrent == NULL )
		return;
	g_pcCurrent->pfVisibility = func;
	g_pcCurrent->bVisSent = false;	// tell the new callback where things stand
}

void glutWindowStatusFunc( void ( *func )( int ) )
{
	if( g_pcCurrent == NULL )
		return;
	g_pcCurrent->pfWindowStatus = func;
	g_pcCurrent->bVisSent = false;
}

void glutIdleFunc( void ( *func )( void ) )
{
	g_pfIdle = func;
}

void glutTimerFunc( unsigned int nMillis, void ( *func )( int ), int nValue )
{
	if( func == NULL )
		return;
	Timer sTimer;
	sTimer.nDue = get_system_time() + bigtime_t( nMillis ) * 1000;
	sTimer.pfFunc = func;
	sTimer.nValue = nValue;
	g_cTimers.push_back( sTimer );
}

void glutMenuStateFunc( void ( *func )( int ) ) {}
void glutMenuStatusFunc( void ( *func )( int, int, int ) )
{
	g_pfMenuStatus = func;
}

void glutSpaceballMotionFunc( void ( *func )( int, int, int ) ) {}
void glutSpaceballRotateFunc( void ( *func )( int, int, int ) ) {}
void glutSpaceballButtonFunc( void ( *func )( int, int ) ) {}
void glutButtonBoxFunc( void ( *func )( int, int ) ) {}
void glutDialsFunc( void ( *func )( int, int ) ) {}
void glutTabletMotionFunc( void ( *func )( int, int ) ) {}
void glutTabletButtonFunc( void ( *func )( int, int, int, int ) ) {}
void glutJoystickFunc( void ( *func )( unsigned int, int, int, int ), int nPoll ) {}
void glutForceJoystickFunc( void ) {}

void glutIgnoreKeyRepeat( int nIgnore )
{
	g_bIgnoreKeyRepeat = nIgnore != 0;
}

void glutSetKeyRepeat( int nMode ) {}

/* ------------------------------------------------------------------------- */
/* Menus                                                                     */

int glutCreateMenu( void ( *func )( int ) )
{
	GlutMenu* pcMenu = new GlutMenu;
	pcMenu->pfFunc = func;
	g_cMenuLock.Lock();
	g_cMenus.push_back( pcMenu );
	pcMenu->nId = g_cMenus.size();
	g_cMenuLock.Unlock();
	g_nCurrentMenu = pcMenu->nId;
	return pcMenu->nId;
}

void glutDestroyMenu( int nMenu )
{
	g_cMenuLock.Lock();
	GlutMenu* pcMenu = get_menu( nMenu );
	if( pcMenu != NULL )
	{
		g_cMenus[nMenu - 1] = NULL;
		delete pcMenu;
	}
	g_cMenuLock.Unlock();
	if( g_nCurrentMenu == nMenu )
		g_nCurrentMenu = 0;
}

int glutGetMenu( void )
{
	return g_nCurrentMenu;
}

void glutSetMenu( int nMenu )
{
	if( get_menu( nMenu ) != NULL )
		g_nCurrentMenu = nMenu;
}

static void menu_set_item( int nItem, const char* pzLabel, int nValue, int nSubMenu, bool bAdd )
{
	g_cMenuLock.Lock();
	GlutMenu* pcMenu = get_menu( g_nCurrentMenu );
	if( pcMenu != NULL )
	{
		MenuItem_s sItem;
		sItem.cLabel = pzLabel != NULL ? pzLabel : "";
		sItem.nValue = nValue;
		sItem.nSubMenu = nSubMenu;
		if( bAdd )
			pcMenu->cItems.push_back( sItem );
		else if( nItem >= 1 && nItem <= (int)pcMenu->cItems.size() )
			pcMenu->cItems[nItem - 1] = sItem;
	}
	g_cMenuLock.Unlock();
}

void glutAddMenuEntry( const char* pzLabel, int nValue )
{
	menu_set_item( 0, pzLabel, nValue, 0, true );
}

void glutAddSubMenu( const char* pzLabel, int nSubMenu )
{
	menu_set_item( 0, pzLabel, 0, nSubMenu, true );
}

void glutChangeToMenuEntry( int nItem, const char* pzLabel, int nValue )
{
	menu_set_item( nItem, pzLabel, nValue, 0, false );
}

void glutChangeToSubMenu( int nItem, const char* pzLabel, int nSubMenu )
{
	menu_set_item( nItem, pzLabel, 0, nSubMenu, false );
}

void glutRemoveMenuItem( int nItem )
{
	g_cMenuLock.Lock();
	GlutMenu* pcMenu = get_menu( g_nCurrentMenu );
	if( pcMenu != NULL && nItem >= 1 && nItem <= (int)pcMenu->cItems.size() )
		pcMenu->cItems.erase( pcMenu->cItems.begin() + ( nItem - 1 ) );
	g_cMenuLock.Unlock();
}

void glutAttachMenu( int nButton )
{
	if( g_pcCurrent != NULL && nButton >= 0 && nButton < 3 )
		g_pcCurrent->nMenu[nButton] = g_nCurrentMenu;
}

void glutDetachMenu( int nButton )
{
	if( g_pcCurrent != NULL && nButton >= 0 && nButton < 3 )
		g_pcCurrent->nMenu[nButton] = 0;
}

/* ------------------------------------------------------------------------- */
/* State queries                                                             */

int glutGet( GLenum eType )
{
	GlutWin* w = g_pcCurrent;
	GLint n = 0;
	switch( eType )
	{
	case GLUT_WINDOW_X:
	case GLUT_WINDOW_Y:
		if( w != NULL && w->pcWindow->Lock() == 0 )
		{
			Point cPos = w->pcView->ConvertToScreen( Point( 0, 0 ) );
			w->pcWindow->Unlock();
			return eType == GLUT_WINDOW_X ? int( cPos.x ) : int( cPos.y );
		}
		return 0;
	case GLUT_WINDOW_WIDTH:
	case GLUT_WINDOW_HEIGHT:
		if( w != NULL )
		{
			Rect cBounds = w->pcView->GetBounds();
			return eType == GLUT_WINDOW_WIDTH ? int( cBounds.Width() ) + 1 : int( cBounds.Height() ) + 1;
		}
		return 0;
	case GLUT_WINDOW_BUFFER_SIZE:
		return 32;
	case GLUT_WINDOW_STENCIL_SIZE:
		glGetIntegerv( GL_STENCIL_BITS, &n );
		return n;
	case GLUT_WINDOW_DEPTH_SIZE:
		glGetIntegerv( GL_DEPTH_BITS, &n );
		return n;
	case GLUT_WINDOW_RED_SIZE:
		glGetIntegerv( GL_RED_BITS, &n );
		return n;
	case GLUT_WINDOW_GREEN_SIZE:
		glGetIntegerv( GL_GREEN_BITS, &n );
		return n;
	case GLUT_WINDOW_BLUE_SIZE:
		glGetIntegerv( GL_BLUE_BITS, &n );
		return n;
	case GLUT_WINDOW_ALPHA_SIZE:
		glGetIntegerv( GL_ALPHA_BITS, &n );
		return n;
	case GLUT_WINDOW_ACCUM_RED_SIZE:
	case GLUT_WINDOW_ACCUM_GREEN_SIZE:
	case GLUT_WINDOW_ACCUM_BLUE_SIZE:
	case GLUT_WINDOW_ACCUM_ALPHA_SIZE:
		glGetIntegerv( GL_ACCUM_RED_BITS, &n );
		return n;
	case GLUT_WINDOW_DOUBLEBUFFER:
		return w != NULL && w->pcView->IsDoubleBuffered() ? 1 : 0;
	case GLUT_WINDOW_RGBA:
		return 1;
	case GLUT_WINDOW_COLORMAP_SIZE:
		return 0;
	case GLUT_WINDOW_PARENT:
		return w != NULL ? w->nParent : 0;
	case GLUT_WINDOW_NUM_CHILDREN:
		return w != NULL ? (int)w->cChildren.size() : 0;
	case GLUT_WINDOW_NUM_SAMPLES:
	case GLUT_WINDOW_STEREO:
		return 0;
	case GLUT_WINDOW_CURSOR:
		return GLUT_CURSOR_INHERIT;
	case GLUT_SCREEN_WIDTH:
		return Desktop().GetResolution().x;
	case GLUT_SCREEN_HEIGHT:
		return Desktop().GetResolution().y;
	case GLUT_SCREEN_WIDTH_MM:
		return Desktop().GetResolution().x * 254 / 960;	// 96 dpi
	case GLUT_SCREEN_HEIGHT_MM:
		return Desktop().GetResolution().y * 254 / 960;
	case GLUT_MENU_NUM_ITEMS:
		{
			GlutMenu* pcMenu = get_menu( g_nCurrentMenu );
			return pcMenu != NULL ? (int)pcMenu->cItems.size() : 0;
		}
	case GLUT_DISPLAY_MODE_POSSIBLE:
		return ( g_nDisplayMode & ( GLUT_INDEX | GLUT_STEREO | GLUT_MULTISAMPLE ) ) == 0;
	case GLUT_INIT_WINDOW_X:
		return g_nInitX;
	case GLUT_INIT_WINDOW_Y:
		return g_nInitY;
	case GLUT_INIT_WINDOW_WIDTH:
		return g_nInitWidth;
	case GLUT_INIT_WINDOW_HEIGHT:
		return g_nInitHeight;
	case GLUT_INIT_DISPLAY_MODE:
		return g_nDisplayMode;
	case GLUT_ELAPSED_TIME:
		return int( ( get_system_time() - g_nStartTime ) / 1000 );
	case GLUT_WINDOW_FORMAT_ID:
		return 0;
	}
	return -1;
}

int glutDeviceGet( GLenum eType )
{
	switch( eType )
	{
	case GLUT_HAS_KEYBOARD:
	case GLUT_HAS_MOUSE:
		return 1;
	case GLUT_NUM_MOUSE_BUTTONS:
		return 3;
	case GLUT_DEVICE_IGNORE_KEY_REPEAT:
		return g_bIgnoreKeyRepeat ? 1 : 0;
	case GLUT_DEVICE_KEY_REPEAT:
		return GLUT_KEY_REPEAT_DEFAULT;
	}
	return 0;
}

int glutLayerGet( GLenum eType )
{
	switch( eType )
	{
	case GLUT_OVERLAY_POSSIBLE:
	case GLUT_HAS_OVERLAY:
	case GLUT_NORMAL_DAMAGED:
		return 0;
	case GLUT_LAYER_IN_USE:
		return GLUT_NORMAL;
	case GLUT_TRANSPARENT_INDEX:
	case GLUT_OVERLAY_DAMAGED:
		return -1;
	}
	return -1;
}

int glutGetModifiers( void )
{
	if( !g_bInCallback )
	{
		WARN( "glutGetModifiers: called outside an input callback" );
		return 0;
	}
	return g_nModifiers;
}

int glutExtensionSupported( const char* pzName )
{
	const char* pzExtensions = (const char*)glGetString( GL_EXTENSIONS );
	if( pzName == NULL || pzExtensions == NULL || strchr( pzName, ' ' ) != NULL || *pzName == '\0' )
		return 0;
	size_t nLen = strlen( pzName );
	const char* p = pzExtensions;
	while( ( p = strstr( p, pzName ) ) != NULL )
	{
		bool bStart = p == pzExtensions || p[-1] == ' ';
		bool bEnd = p[nLen] == ' ' || p[nLen] == '\0';
		if( bStart && bEnd )
			return 1;
		p += nLen;
	}
	return 0;
}

GLUTproc glutGetProcAddress( const char* pzName )
{
	return (GLUTproc)OSMesaGetProcAddress( pzName );
}

void glutReportErrors( void )
{
	GLenum e;
	while( ( e = glGetError() ) != GL_NO_ERROR )
		__glutWarning( (char*)"GL error: %s", gluErrorString( e ) );
}

/* Game mode and video resizing: none */
void glutGameModeString( const char* pzString ) {}
int glutEnterGameMode( void ) { return 0; }
void glutLeaveGameMode( void ) {}
int glutGameModeGet( GLenum eMode ) { return 0; }
int glutVideoResizeGet( GLenum eParam ) { return 0; }
void glutSetupVideoResizing( void ) {}
void glutStopVideoResizing( void ) {}
void glutVideoResize( int x, int y, int width, int height ) {}
void glutVideoPan( int x, int y, int width, int height ) {}

/* ------------------------------------------------------------------------- */
/* The main loop                                                             */

static void dispatch( const Event& sEvent )
{
	GlutWin* pcWin = get_window( sEvent.nWindow );
	if( pcWin == NULL )
		return;

	if( sEvent.nType == EV_CLOSE )
		exit( 0 );

	if( sEvent.nType == EV_RESHAPE )
	{
		pcWin->bNeedReshape = true;
		return;
	}

	g_nModifiers = sEvent.nModifiers;
	if( pcWin != g_pcCurrent )
		make_current( pcWin );

	switch( sEvent.nType )
	{
	case EV_KEY:
		if( pcWin->pfKeyboard != NULL )
			pcWin->pfKeyboard( (unsigned char)sEvent.nKey, sEvent.x, sEvent.y );
		break;
	case EV_KEY_UP:
		if( pcWin->pfKeyboardUp != NULL )
			pcWin->pfKeyboardUp( (unsigned char)sEvent.nKey, sEvent.x, sEvent.y );
		break;
	case EV_SPECIAL:
		if( pcWin->pfSpecial != NULL )
			pcWin->pfSpecial( sEvent.nKey, sEvent.x, sEvent.y );
		break;
	case EV_SPECIAL_UP:
		if( pcWin->pfSpecialUp != NULL )
			pcWin->pfSpecialUp( sEvent.nKey, sEvent.x, sEvent.y );
		break;
	case EV_MOUSE:
		if( pcWin->pfMouse != NULL )
			pcWin->pfMouse( sEvent.nKey, sEvent.nState, sEvent.x, sEvent.y );
		break;
	case EV_MOTION:
		if( pcWin->pfMotion != NULL )
			pcWin->pfMotion( sEvent.x, sEvent.y );
		break;
	case EV_PASSIVE:
		if( pcWin->pfPassive != NULL )
			pcWin->pfPassive( sEvent.x, sEvent.y );
		break;
	case EV_ENTRY:
		if( pcWin->pfEntry != NULL )
			pcWin->pfEntry( sEvent.nKey );
		break;
	case EV_MENU:
		{
			GlutMenu* pcMenu = get_menu( sEvent.nKey );
			if( pcMenu != NULL && pcMenu->pfFunc != NULL )
			{
				int nOld = g_nCurrentMenu;
				g_nCurrentMenu = sEvent.nKey;
				pcMenu->pfFunc( sEvent.nState );
				if( get_menu( nOld ) != NULL )
					g_nCurrentMenu = nOld;
			}
		}
		break;
	default:
		break;
	}
}

static bool update_windows()
{
	bool bDisplayed = false;
	for( size_t i = 0; i < g_cWindows.size(); i++ )
	{
		GlutWin* pcWin = g_cWindows[i];
		if( pcWin == NULL )
			continue;

		if( !pcWin->bVisSent )
		{
			pcWin->bVisSent = true;
			make_current( pcWin );
			if( pcWin->pfWindowStatus != NULL )
				pcWin->pfWindowStatus( pcWin->bVisible ? GLUT_FULLY_RETAINED : GLUT_HIDDEN );
			else if( pcWin->pfVisibility != NULL )
				pcWin->pfVisibility( pcWin->bVisible ? GLUT_VISIBLE : GLUT_NOT_VISIBLE );
			if( g_cWindows[i] == NULL )
				continue;
		}

		if( pcWin->bNeedReshape )
		{
			pcWin->bNeedReshape = false;
			make_current( pcWin );	// resizes the buffer
			Rect cBounds = pcWin->pcView->GetBounds();
			int w = int( cBounds.Width() ) + 1, h = int( cBounds.Height() ) + 1;
			if( w != pcWin->nWidth || h != pcWin->nHeight )
			{
				pcWin->nWidth = w;
				pcWin->nHeight = h;
				if( pcWin->pfReshape != NULL )
					pcWin->pfReshape( w, h );
				else
					glViewport( 0, 0, w, h );
				if( g_cWindows[i] == NULL )
					continue;
			}
			pcWin->bNeedDisplay = true;
		}

		if( pcWin->bNeedDisplay && pcWin->bVisible && pcWin->pfDisplay != NULL )
		{
			pcWin->bNeedDisplay = false;
			make_current( pcWin );
			pcWin->pfDisplay();
			bDisplayed = true;
			if( g_cWindows[i] == NULL )
				continue;
			if( !pcWin->pcView->IsDoubleBuffered() )
				pcWin->pcView->SwapBuffers();
		}
	}
	return bDisplayed;
}

static bool any_display_pending()
{
	for( size_t i = 0; i < g_cWindows.size(); i++ )
	{
		GlutWin* pcWin = g_cWindows[i];
		if( pcWin != NULL && ( pcWin->bNeedReshape || ( pcWin->bNeedDisplay && pcWin->bVisible ) ) )
			return true;
	}
	return false;
}

static void run_timers()
{
	bigtime_t nNow = get_system_time();
	// timers may add timers: take the due ones out first
	std::vector<Timer> cDue;
	for( size_t i = 0; i < g_cTimers.size(); )
	{
		if( g_cTimers[i].nDue <= nNow )
		{
			cDue.push_back( g_cTimers[i] );
			g_cTimers.erase( g_cTimers.begin() + i );
		}
		else
			i++;
	}
	for( size_t i = 0; i < cDue.size(); i++ )
		cDue[i].pfFunc( cDue[i].nValue );
}

void glutMainLoop( void )
{
	if( g_cWindows.empty() )
		FATAL( "main loop entered with no windows created." );

	g_bInCallback = true;
	for( ;; )
	{
		// input first, in order
		for( ;; )
		{
			g_cQueueLock.Lock();
			if( g_cQueue.empty() )
			{
				g_cQueueLock.Unlock();
				break;
			}
			Event sEvent = g_cQueue.front();
			g_cQueue.pop_front();
			g_cQueueLock.Unlock();
			dispatch( sEvent );
		}

		run_timers();
		update_windows();

		if( g_pfIdle != NULL )
		{
			g_pfIdle();
			continue;
		}
		if( any_display_pending() )
			continue;

		// sleep until an event, or the next timer
		if( g_cTimers.empty() )
			lock_semaphore( g_hWake );
		else
		{
			bigtime_t nNext = g_cTimers[0].nDue;
			for( size_t i = 1; i < g_cTimers.size(); i++ )
				if( g_cTimers[i].nDue < nNext )
					nNext = g_cTimers[i].nDue;
			bigtime_t nWait = nNext - get_system_time();
			if( nWait > 0 )
				lock_semaphore_x( g_hWake, 1, 0, nWait );
		}
	}
}
