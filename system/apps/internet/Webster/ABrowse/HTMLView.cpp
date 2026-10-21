#include <wtf/Platform.h>
#include <Document.h>
#include <ScrollView.h>
#include <FrameView.h>
#include <ContextMenuClientSyllable.h>
#include <FrameLoadRequest.h>
#include <ResourceRequest.h>
#include <ResourceHandle.h>
#include <ResourceHandleInternal.h>
#include <ResourceResponse.h>
#include <HTMLFormElement.h>
#include <EditorClientSyllable.h>
#include <DragClientSyllable.h>
#include <FrameLoaderClientSyllable.h>
#include <Settings.h>
#include <CachedPage.h>
#include <HistoryItem.h>
#include <DocumentLoader.h>
#include <KURL.h>
#include <HTMLFrameOwnerElement.h>
#include <CString.h>
#include <wtf/HashMap.h>

#include <Frame.h>
#include <FrameLoader.h>
#include <FrameTree.h>
#include <page/EventHandler.h>
#include <page/Page.h>
#include <history/PageCache.h>
#include <InspectorClientSyllable.h>
#include <FocusController.h>
#include <HitTestResult.h>
#include <WindowFeatures.h>

#include <gui/scrollbar.h>
#include <gui/requesters.h>

#include "HTMLView.h"
#include "downloaddialog.h"
#include "ChromeClientSyllable.h"

using namespace os;


class HTMLViewFrame : public WebCore::FrameView
{
public:
	HTMLViewFrame( HTMLView* pcView, WebCore::Frame* pcFrame)
			: WebCore::FrameView( pcFrame )
	{
		m_pcView = pcView;
		m_bStatusTextSet = false;
	}
	bool handleMouseMoveEvent(const WebCore::PlatformMouseEvent& cEvent)
	{
		WebCore::HitTestResult result = frame()->eventHandler()->hitTestResultAtPoint( cEvent.pos(), true );
	    os::String cUrl = os::String( result.absoluteLinkURL().string().utf8().data() );
	    if( !cUrl.empty() )
	    {
    		os::Message cMsg( ID_SET_STATUS_BAR_TEXT );
			cMsg.AddString( "text", cUrl );
			m_pcView->GetWindow()->PostMessage( &cMsg, m_pcView->GetWindow() );
			m_bStatusTextSet = true;
	    }
	    else if( m_bStatusTextSet )
    	{
	    	os::Message cMsg( ID_SET_STATUS_BAR_TEXT );
			cMsg.AddString( "text", "" );
			m_pcView->GetWindow()->PostMessage( &cMsg, m_pcView->GetWindow() );
    		m_bStatusTextSet = false;
	    }
//	    return( WebCore::FrameView::handleMouseMoveEvent( cEvent ) );
	}
private:
	bool m_bStatusTextSet;
	HTMLView* m_pcView;
};

/* WebCore::ContextMenuClientSyllable implementation */
class HTMLViewContextMenuClient : public WebCore::ContextMenuClientSyllable
{
public:
	HTMLViewContextMenuClient( HTMLView* pcView )
	{
		m_pcView = pcView;
	}
	void downloadURL(const WebCore::KURL& url)
	{
		printf( "downloadURL %s\n", url.string().utf8().data() );

		if( m_pcView->GetWindow() != NULL )
		{
			os::Message cMsg( ID_SAVE_LINK );
			cMsg.AddPointer( "view", m_pcView );
			cMsg.AddString( "url", url.string().utf8().data() );
			m_pcView->GetWindow()->PostMessage( &cMsg, m_pcView->GetWindow() );
		}
	}
private:
	HTMLView* m_pcView;
};

	#include <util/locker.h>
	extern os::Locker g_cGlobalMutex;


/* WebCore::FrameLoaderClientSyllable implementation */
class HTMLViewFrameLoaderClient : public WebCore::FrameLoaderClientSyllable
{
public:
	HTMLViewFrameLoaderClient( HTMLView* pcView )
	{
		m_pcView = pcView;
		m_pcFrameView = NULL;
		m_pcFrame = NULL;
	}
	~HTMLViewFrameLoaderClient()
	{
	}
	bool hasWebView() const
	{
		return( true );
	}
	bool hasFrameView() const
	{
		return( true );
	}
	bool privateBrowsingEnabled() const
	{
		return( false );
	}
	bool shouldFallBack(const WebCore::ResourceError&)
	{
		return( true );
	}

	void download(WebCore::ResourceHandle* handle, const WebCore::ResourceRequest& request, const WebCore::ResourceRequest&, const WebCore::ResourceResponse& response)
	{
		printf( "HTMLViewFrameLoaderClient::download()\n" );

		DownloadHandler* pcHandler = new DownloadHandler();
		pcHandler->Start( handle );
		pcHandler->didReceiveResponse( handle, response );
	}
	
	void dispatchDecidePolicyForMIMEType(WebCore::FramePolicyFunction policyFunction, const WebCore::String& mimeType, const WebCore::ResourceRequest&)
	{
	    // FIXME: we need to call directly here (comment copied from Qt version)
	    ASSERT(frame() && policyFunction);
    	if (!frame() || !policyFunction)
	        return;
	    if( strncasecmp( os::String( mimeType ).c_str(), "text/", 5 ) == 0 ||
	    	strncasecmp( os::String( mimeType ).c_str(), "image/", 6 ) == 0 ||
	    	strcmp( os::String( mimeType ).c_str(), "application/xhtml+xml" ) == 0 )
    		(frame()->loader()->*policyFunction)(WebCore::PolicyUse);
    	else
    		(frame()->loader()->*policyFunction)(WebCore::PolicyDownload);
	}
	WebCore::Frame* dispatchCreatePage()
	{
		WebCore::ResourceRequest cRequest;
		cRequest.setURL( "about:blank" );

		WebCore::WindowFeatures cFeatures;

		WebCore::Page* pcPage = frame()->page()->chrome()->createWindow( frame(), WebCore::FrameLoadRequest( cRequest ), cFeatures );
		return( pcPage->mainFrame() );
	}
	void dispatchDidStartProvisionalLoad()
	{
		if( m_pcView->GetWindow() != NULL && m_pcView->GetWebCoreFrame() == frame() )
		{
			os::Message cMsg( ID_URL_OPENED );
			cMsg.AddPointer( "view", m_pcView );
			cMsg.AddString( "url", frame()->loader()->provisionalDocumentLoader()->url().string().utf8().data() );
			m_pcView->GetWindow()->PostMessage( &cMsg, m_pcView->GetWindow() );
		}
	}
	void dispatchDidFailProvisionalLoad(const WebCore::ResourceError& error)
	{
		if( m_pcView->GetWindow() != NULL && m_pcView->GetWebCoreFrame() == frame() )
		{
			//frame()->loader()->goToItem( frame()->loader()->currentHistoryItem(), WebCore::FrameLoadTypeBack );
			os::Alert * pcAlert = new os::Alert( "Error", os::String( "Failed to load " ) + os::String( error.failingURL() ), os::Alert::ALERT_WARNING, 0x00, "Ok", NULL );
			pcAlert->Go( NULL );
			os::Message cMsg( ID_LOAD_FINISHED );
			cMsg.AddPointer( "view", m_pcView );
			m_pcView->GetWindow()->PostMessage( &cMsg, m_pcView->GetWindow() );

		}
	}
	void updateGlobalHistoryForStandardLoad(const WebCore::KURL& url)
	{
		if( m_pcView->GetWindow() != NULL && m_pcView->GetWebCoreFrame() == frame() )
		{
			os::Message cMsg( ID_URL_OPENED );
			printf( "Start %s\n", url.string().utf8().data() );
			cMsg.AddPointer( "view", m_pcView );
			cMsg.AddString( "url", url.string().utf8().data() );
			m_pcView->GetWindow()->PostMessage( &cMsg, m_pcView->GetWindow() );
		}
	}
	void updateGlobalHistoryForReload(const WebCore::KURL& url)
	{
		updateGlobalHistoryForStandardLoad( url );
	}
	void dispatchDidReceiveContentLength( WebCore::DocumentLoader*, unsigned long identifier, int lengthReceived)
	{
	}
	void dispatchDidFinishLoading( WebCore::DocumentLoader*, unsigned long identifier )
	{
	}
	void dispatchDidHandleOnloadEvents()
	{
		printf("Load finished %s!\n",os::String( frame()->tree()->name().domString() ).c_str() );
		printf( "%x %x\n", frame()->tree()->firstChild(), frame()->document() );
		
		if( m_pcView->GetWindow() != NULL )
		{
			os::Message cMsg( ID_LOAD_FINISHED );
			cMsg.AddPointer( "view", m_pcView );
			m_pcView->GetWindow()->PostMessage( &cMsg, m_pcView->GetWindow() );
		}
	}
	WebCore::String overrideMediaType() const
	{
		return( WebCore::String() );
	}
	bool canCachePage() const
	{
		return( false );
	}
	void setDocumentViewFromCachedPage(WebCore::CachedPage* cachedPage)
	{
	    WebCore::DocumentLoader* cachedDocumentLoader = cachedPage->documentLoader();
    	cachedDocumentLoader->setFrame(frame());
	}
	void loadedFromCachedPage()
	{
		//frame()->loader()->currentHistoryItem()->setCachedPage(0);
		frame()->forceLayout();
	}
	void forceLayout()
	{
		frame()->forceLayout();
	}


	void dispatchDidReceiveTitle(const WebCore::String& title)
	{
		if( m_pcView->GetWindow() != NULL && m_pcView->GetWebCoreFrame() == frame()  )
		{
			os::Message cMsg( ID_SET_TITLE );
			cMsg.AddPointer( "view", m_pcView );
			cMsg.AddString( "title", os::String( title ) );
			m_pcView->GetWindow()->PostMessage( &cMsg, m_pcView->GetWindow() );
		}

	}

	PassRefPtr<WebCore::Frame> createFrame(const WebCore::KURL& url, const WebCore::String& name, WebCore::HTMLFrameOwnerElement* ownerElement, const WebCore::String& referrer,
									bool allowsScrolling, int marginWidth, int marginHeight)
	{
		if( m_pcView->GetWindow() != NULL )
			m_pcView->GetWindow()->Lock();
			
		HTMLViewFrameLoaderClient* pcLoaderClient = new HTMLViewFrameLoaderClient( m_pcView );
		pcLoaderClient->m_pcFrame = new WebCore::Frame( m_pcView->GetWebCoreFrame()->page(), ownerElement, pcLoaderClient );
	    pcLoaderClient->setFrame( pcLoaderClient->m_pcFrame.get() );
	    pcLoaderClient->m_pcFrameView = new HTMLViewFrame( m_pcView, pcLoaderClient->m_pcFrame.get() );
	    
	    printf("CREATE Frame %x %s %s %x %i!\n", pcLoaderClient->m_pcFrame.get(), url.string().utf8().data(), os::String( name ).c_str(), ownerElement->document()->frame(), g_cGlobalMutex.GetLockCount());
    
	    pcLoaderClient->m_pcFrame->setView(pcLoaderClient->m_pcFrameView.get());
	    pcLoaderClient->m_pcFrame->init();
		pcLoaderClient->m_pcFrame->deref();	    
	    pcLoaderClient->m_pcFrameView->deref();
	    pcLoaderClient->m_pcFrameView->setAllowsScrolling( allowsScrolling );

		pcLoaderClient->m_pcFrame->tree()->setName( name );
		frame()->tree()->appendChild(pcLoaderClient->m_pcFrame);
		pcLoaderClient->m_pcFrameView->hide();
			
		WebCore::FrameLoadType loadType = frame()->loader()->loadType();
    	WebCore::FrameLoadType childLoadType = WebCore::FrameLoadTypeRedirectWithLockedHistory;
		
		if( url == "about:blank" )
		{
			pcLoaderClient->m_pcFrameView->setStaticBackground( true );
		}
		
		pcLoaderClient->m_pcFrame->loader()->load( url, referrer, childLoadType,
                             WebCore::String(), NULL, 0);
                             
   		if( m_pcView->GetWindow() != NULL )
			m_pcView->GetWindow()->Unlock();

		 // The frame's onload handler may have removed it from the document.
    	if (!pcLoaderClient->m_pcFrame->tree()->parent())
        	return 0;

		RefPtr<WebCore::Frame> cChildFrame = pcLoaderClient->m_pcFrame;
	    return cChildFrame.get();
	}

	void detachedFromParent3()
	{
	}

	void detachedFromParent4()
	{
    	m_pcFrameView = NULL;
		m_pcFrame = NULL;
	}
private:
	HTMLView* m_pcView;
	WTF::RefPtr<WebCore::Frame> m_pcFrame;
	WTF::RefPtr<WebCore::FrameView> m_pcFrameView;
};


class HTMLView::Private
{
public:
	WebCore::Page* m_pcPage;
	WTF::RefPtr<WebCore::Frame> m_pcFrame;
	WTF::RefPtr<HTMLViewFrame> m_pcFrameView;
	bool m_bStatusTextSet;
};

HTMLView::HTMLView( const Rect & cFrame, const String &cTitle, WebCore::ChromeClient* pcClient, uint32 nResizeMask, uint32 nFlags ):os::View( cFrame, cTitle, nResizeMask, nFlags )
{
	m = new Private;
	m->m_bStatusTextSet = false;
	
	WebCore::EditorClientSyllable* pcEditor = new WebCore::EditorClientSyllable();
	HTMLViewContextMenuClient* pcContextMenu = new HTMLViewContextMenuClient( this );
	m->m_pcPage = new WebCore::Page(pcClient, pcContextMenu, pcEditor,
		new WebCore::DragClientSyllable(), new WebCore::InspectorClientSyllable() );
	pcEditor->setPage( m->m_pcPage );
	
	WebCore::Settings *settings = m->m_pcPage->settings();
	settings->setLoadsImagesAutomatically(true);
	settings->setMinimumFontSize(5);
	settings->setMinimumLogicalFontSize(5);
	settings->setShouldPrintBackgrounds(true);
	settings->setJavaScriptEnabled(true);

	settings->setDefaultFixedFontSize(14);
	settings->setDefaultFontSize(14);
	settings->setSerifFontFamily("Times New Roman");
	settings->setSansSerifFontFamily("Arial");
	settings->setFixedFontFamily("Courier");
	settings->setStandardFontFamily("Arial");
	
	HTMLViewFrameLoaderClient* pcLoaderClient = new HTMLViewFrameLoaderClient( this );
	m->m_pcFrame = new WebCore::Frame( m->m_pcPage, 0, pcLoaderClient );
    pcLoaderClient->setFrame( m->m_pcFrame.get() );
    
    m->m_pcFrameView = new HTMLViewFrame( this, m->m_pcFrame.get() );
    
	m->m_pcFrameView->deref();
    m->m_pcFrame->setView(m->m_pcFrameView.get());
    m->m_pcFrame->init();

    
    m->m_pcFrameView->setParentWidget(this);
	m->m_pcFrame->view()->syllableWidget()->SetFrame( GetBounds() );
}

HTMLView::~HTMLView()
{
	g_cGlobalMutex.Lock();
	m->m_pcFrame->page()->focusController()->setActive( false );
	m->m_pcFrame->loader()->detachFromParent();
	#if 0

	m->m_pcFrame->loader()->stopForUserCancel();
	m->m_pcFrame->loader()->closeURL();
	m->m_pcFrame->loader()->cancelAndClear();		
	#endif
	WebCore::FrameView* pcFrameView = m->m_pcFrameView.get();
	WebCore::Frame* pcFrame = m->m_pcFrame.get();
	m->m_pcFrameView = NULL;
	m->m_pcFrame = NULL;
	delete( m->m_pcPage );	
	delete( m );
	g_cGlobalMutex.Unlock();	
}

void HTMLView::KeyDown( const char *pzString, const char *pzRawString, uint32 nQualifiers )
{
	if ( nQualifiers & QUAL_CTRL )
	{
		switch ( pzRawString[0] )
		{
		case VK_TAB:
			GetWindow()->PostMessage( ID_ACTIVATE_URLEDIT, GetWindow(  ) );
			break;
		case 'r':
			GetWindow()->PostMessage( ID_RELOAD, GetWindow(  ) );
			break;
		case 'f':
			GetWindow()->PostMessage( ID_FIND, GetWindow(  ) );
			break;
		case VK_INSERT:
		case 'c':
			GetWindow()->PostMessage( ID_COPY, GetWindow(  ) );
			break;
		case 'n':
			GetWindow()->PostMessage( ID_NEW_WINDOW, GetWindow(  ) );
			break;
		case 'q':
			GetWindow()->PostMessage( M_QUIT, GetWindow(  ) );
			break;
		case 't':
			GetWindow()->PostMessage( ID_NEW_TAB, GetWindow(  ) );
			break;
		case 'w':
			GetWindow()->PostMessage( ID_DELETE_TAB, GetWindow(  ) );
			break;
		default:
			View::KeyDown( pzString, pzRawString, nQualifiers );
			break;
		}
	}
	else
	{
		switch ( pzRawString[0] )
		{
		#if 0
		case VK_BACKSPACE:
			GetWindow()->PostMessage( ID_PREV_URL, GetWindow(  ) );
			break;
		#endif			
		case VK_FUNCTION_KEY:
			{
				os::Message * pcMsg = GetLooper()->GetCurrentMessage(  );
				ASSERT( pcMsg != NULL );
				int32 nKeyCode;

				if ( pcMsg->FindInt32( "_raw_key", &nKeyCode ) != 0 )
				{
					return;
				}

				switch ( nKeyCode )
				{
				case 4:	// F3
					GetWindow()->PostMessage( ID_REPEAT_FIND, GetWindow(  ) );
					break;
				default:
					View::KeyDown( pzString, pzRawString, nQualifiers );
					break;
				}
				break;
			}
		default:
			View::KeyDown( pzString, pzRawString, nQualifiers );
			break;

		}
	}
}

int HTMLView::GetBackListCount()
{
	return( m->m_pcPage->backForwardList()->backListCount() );
}

int HTMLView::GetForwardListCount()
{
	return( m->m_pcPage->backForwardList()->forwardListCount() );
}

WebCore::Frame* HTMLView::GetWebCoreFrame()
{
	return ( m->m_pcFrame.get() );
}


os::String HTMLView::GetCurrentURL()
{
	if ( m->m_pcFrame->loader() == NULL || m->m_pcFrame->loader()->url().isEmpty() )
		return( "" );
	return ( os::String ( m->m_pcFrame->loader()->url().string().utf8().data() ) );
}

void HTMLView::OpenURL( const WebCore::ResourceRequest& cRequest )
{
	if( cRequest.url().isEmpty() )
		return;
	printf( "Open URL %s\n", cRequest.url().string().utf8().data() );
	m->m_pcFrame->loader()->load( cRequest );
}

void HTMLView::GoBack()
{
	m->m_pcPage->goBack();
}

void HTMLView::GoForward()
{
	m->m_pcPage->goForward();
}

void HTMLView::Reload()
{
	m->m_pcFrame->loader()->reload();
}


void HTMLView::Stop()
{
	m->m_pcFrame->loader()->stopAllLoaders();
}


bool HTMLView::IsLoading()
{
	return( m->m_pcFrame->loader()->isLoading() );
}

























































































































