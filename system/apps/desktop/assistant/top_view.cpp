#include "top_view.h"
#include <util/message.h>
#include "gui/layoutview.h"
#include "gui/listview.h"
#include "messages.h"
#include "./providers/provider.h"
#include "./gui/row.h"
#include "provider_thread.h"
#include "util/application.h"
#include "util/messenger.h"
#include <sys/debug.h>
using namespace assistant;

enum
{
	TIMER_QUERY = 1
};

/* Wait this long after the last keystroke before searching */
static const bigtime_t QUERY_DELAY = 250000;
static const uint MAX_RESULTS = 20;


TopView::TopView( os::Window* parent, const os::Rect& cFrame ) : os::View( cFrame, "assistant_topmost", os::CF_FOLLOW_ALL ), m_nGeneration( 0 )
{
	m_pcParent = parent;
	_Layout();
}

void TopView::_Layout()
{
	os::Rect frame = GetBounds();

	m_pcRoot = new os::VLayoutNode( "root" );
	m_pcLayoutView = new os::LayoutView( frame, "", m_pcRoot, os::CF_FOLLOW_ALL );

	os::HLayoutNode* node1 = new os::HLayoutNode( "text_node" );
	node1->AddChild( m_pcView = new os::TextView( frame, "", "", os::CF_FOLLOW_ALL ) );


	m_pcView->SetEventMask( os::TextView::EI_ESC_PRESSED | os::TextView::EI_CONTENT_CHANGED | os::TextView::EI_ENTER_PRESSED );
	m_pcView->SetTextColor( os::Color32_s( 255, 255, 255 ) );
	m_pcView->SetBackgroundColor( os::Color32_s( 0, 0, 0, 120 ) );
	m_pcView->SetSelectAll( true );
	m_pcView->SetSelectWord( true );
	m_pcView->SetDrawFrame( false );


	m_pcView->SetMessage( new os::Message( M_CHANGED ) );
	m_pcView->SetTarget( this );


	os::HLayoutNode* node2 = new os::HLayoutNode( "list_node" );

	m_pcListView = new os::ListView( frame, "_listview", os::ListView::F_NO_HEADER, os::CF_FOLLOW_ALL );
	m_pcListView->InsertColumn( "col", 200 );

	node2->AddChild( m_pcListView );
	
	m_pcRoot->AddChild( node1 );
	m_pcRoot->AddChild( node2 );

	m_pcView->SetTarget( this );
	m_pcListView->SetTarget( this );
	m_pcListView->SetInvokeMsg( new os::Message( M_ENTER ) );

	m_pcLayoutView->SetRoot( m_pcRoot );
	AddChild( m_pcLayoutView );
}

void TopView::AllAttached()
{
	View::AllAttached();
}

os::Point TopView::GetPreferredSize( bool max )
{
	return m_pcLayoutView->GetPreferredSize( max );
}

void TopView::AttachedToWindow()
{
	os::Font* font = GetFont();
	font->SetSize( 28 );
	SetFont( font );
}


void TopView::KeyDown( const char* pzKey, const char* pzRawString, uint32 nQualifiers )
{
	uint8 key = ( uint8 )pzRawString[0];
	if( key == os::VK_ESCAPE )
	{
		m_pcParent->PostMessage( new os::Message( M_CHANGED ), this );
	}
	else if( pzKey[0] == os::VK_DOWN_ARROW || pzKey[0] == os::VK_UP_ARROW )
	{
		os::Message cMessage( M_CYCLE );
		cMessage.AddString( "_raw", pzRawString );
		cMessage.AddString( "_key", pzKey );
		cMessage.AddInt32( "_qual", nQualifiers );
		m_pcListView->KeyDown( pzKey, pzRawString, nQualifiers );
	}

	else if( pzKey[0] == os::VK_ENTER )
	{
		m_pcListView->KeyDown( pzKey, pzRawString, nQualifiers );
	}
}

void TopView::HandleMessage( os::Message* pcMessage )
{
	switch( pcMessage->GetCode() )
	{
		case M_ENTER:
		{
			_LaunchSelected();
			break;
		}
		case M_RESULTS:
		{
			_ShowResults( pcMessage );
			break;
		}
		case M_CHANGED:
		{
			int32 events;
			if( pcMessage->FindInt32( "events", &events ) == 0 )
			{
				if( events & os::TextView::EI_ESC_PRESSED )
				{
					m_pcParent->Hide();
					return;
				}
				if( events & os::TextView::EI_ENTER_PRESSED )
				{
					_LaunchSelected();
					return;
				}
				if( events & os::TextView::EI_CONTENT_CHANGED )
				{
					/* Debounce: restart the timer on every edit, search when it fires */
					GetLooper()->RemoveTimer( this, TIMER_QUERY );
					GetLooper()->AddTimer( this, TIMER_QUERY, QUERY_DELAY, true );
				}
			}
			break;
		}
		default:
			os::View::HandleMessage( pcMessage );
			break;
	}
}

void TopView::TimerTick( int nID )
{
	if( nID == TIMER_QUERY )
	{
		_Query();
	}
}

/* Ask the provider thread for results. The reply comes back as M_RESULTS. */
void TopView::_Query()
{
	os::String input = m_pcView->GetValue().AsString();
	m_nGeneration++;

	if( input.empty() )
	{
		m_pcListView->Clear();
		return;
	}

	ProviderThread* pcProviders = ProviderThread::GetInstance();
	if( pcProviders == NULL )
	{
		return;
	}

	os::Message cMessage( M_QUERY );
	cMessage.AddString( "input", input );
	cMessage.AddInt32( "generation", m_nGeneration );
	pcProviders->PostMessage( &cMessage, pcProviders, this );
}

void TopView::_ShowResults( os::Message* pcMessage )
{
	int32 nGeneration = -1;
	std::vector<Result>* pcResults = NULL;
	pcMessage->FindInt32( "generation", &nGeneration );
	if( pcMessage->FindPointer( "results", ( void** )&pcResults ) != 0 || pcResults == NULL )
	{
		return;
	}

	/* Drop answers to queries the user has already typed past */
	if( nGeneration == m_nGeneration )
	{
		m_pcListView->Clear();
		for( uint i = 0; i < pcResults->size() && i < MAX_RESULTS; i++ )
		{
			m_pcListView->InsertRow( new ListViewDisplayRow( ( *pcResults )[i] ), false );
		}
		if( m_pcListView->GetRowCount() > 0 )
		{
			m_pcListView->Select( 0 );
		}
		m_pcListView->RefreshLayout();
	}
	delete pcResults;
}

void TopView::_LaunchSelected()
{
	int nSelected = m_pcListView->GetLastSelected();
	if( nSelected < 0 && m_pcListView->GetRowCount() > 0 )
	{
		nSelected = 0;
	}
	if( nSelected < 0 )
	{
		return;
	}

	/* Every row in the list is a ListViewDisplayRow */
	ListViewDisplayRow* node = static_cast<ListViewDisplayRow*>( m_pcListView->GetRow( nSelected ) );
	ProviderThread* pcProviders = ProviderThread::GetInstance();
	if( node == NULL || pcProviders == NULL )
	{
		return;
	}

	const Result& r = node->GetResult();
	os::Message cMessage( M_LAUNCH );
	cMessage.AddString( "provider", r.provider() );
	cMessage.AddString( "name", r.name() );
	cMessage.AddString( "description", r.description() );
	for( std::map<os::String, os::String>::const_iterator i = r.metaData().begin(); i != r.metaData().end(); ++i )
	{
		cMessage.AddString( "meta_key", i->first );
		cMessage.AddString( "meta_value", i->second );
	}
	pcProviders->PostMessage( &cMessage, pcProviders );
	m_pcParent->Hide();
}

os::String TopView::GetContent() const
{
	return m_pcView->GetValue().AsString();
}


void TopView::MakeFocus( bool bFocus )
{
	GetWindow()->Lock();
	os::View::MakeFocus( bFocus );
	GetWindow()->SetFocusChild( m_pcView );
	/* Forget any search still in flight */
	GetWindow()->RemoveTimer( this, TIMER_QUERY );
	m_nGeneration++;
	m_pcListView->Clear();
	m_pcRoot->Layout();
	GetWindow()->Unlock();
}


void TopView::Paint( const os::Rect& cFrame )
{
	View::Paint( cFrame );
}


void TopView::FontChanged( os::Font* font )
{
	m_pcListView->FontChanged( font );
	m_pcView->FontChanged( font );
	m_pcLayoutView->FontChanged( font );
}
