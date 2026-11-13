/*  An example of GUI tests with libguitest: a window with a button that
 *  counts clicks into a string view, and a view that handles a message of
 *  its own.  Run it in a Syllable session (it needs the app server):
 *
 *      /system/development/tests/GuiTestExample
 *      /system/development/tests/GuiTestExample --gtest_filter=CounterTest.*
 */

#include <guitest.h>

#include <gui/button.h>
#include <gui/stringview.h>
#include <util/message.h>
#include <util/string.h>

using namespace os;

namespace
{

enum { ID_INCREMENT = 1, ID_SET_LABEL };

// What's being tested: a window with a button and a label
class CounterWindow : public Window
{
public:
	CounterWindow() : Window( Rect( 100, 100, 300, 160 ), "counter", "Counter" ), m_nCount( 0 )
	{
		m_pcLabel = new StringView( Rect( 10, 10, 190, 25 ), "label", "0" );
		AddChild( m_pcLabel );
		AddChild( new Button( Rect( 10, 30, 90, 50 ), "increment", "Add one",
				      new Message( ID_INCREMENT ) ) );
	}
	virtual void HandleMessage( Message* pcMsg )
	{
		if( pcMsg->GetCode() == ID_INCREMENT )
		{
			++m_nCount;
			String cText;
			cText.Format( "%d", m_nCount );
			m_pcLabel->SetString( cText );
		}
		else
			Window::HandleMessage( pcMsg );
	}
	int GetCount() const { return m_nCount; }
	String GetLabel() const { return m_pcLabel->GetString(); }
private:
	int m_nCount;
	StringView* m_pcLabel;
};

// A view that handles a message of its own: ID_SET_LABEL with a "text" string
class LabelView : public StringView
{
public:
	LabelView() : StringView( Rect( 10, 10, 190, 25 ), "labelview", "" ) {}
	virtual void HandleMessage( Message* pcMsg )
	{
		String cText;
		if( pcMsg->GetCode() == ID_SET_LABEL && pcMsg->FindString( "text", &cText ) == 0 )
			SetString( cText );
		else
			StringView::HandleMessage( pcMsg );
	}
};

class CounterTest : public guitest::WindowTest {};

TEST_F( CounterTest, StartsAtZero )
{
	CounterWindow* pcWin = Open( new CounterWindow() );
	guitest::Locked cLock( pcWin );
	EXPECT_EQ( 0, pcWin->GetCount() );
	EXPECT_STREQ( "0", pcWin->GetLabel().c_str() );
}

TEST_F( CounterTest, ButtonClicksAreCounted )
{
	CounterWindow* pcWin = Open( new CounterWindow() );
	View* pcButton;
	{
		guitest::Locked cLock( pcWin );
		pcButton = pcWin->FindView( "increment" );
	}
	ASSERT_TRUE( pcButton != NULL );
	for( int i = 0; i < 3; ++i )
		ASSERT_TRUE( guitest::Click( pcButton ) );

	guitest::Locked cLock( pcWin );
	EXPECT_EQ( 3, pcWin->GetCount() );
	EXPECT_STREQ( "3", pcWin->GetLabel().c_str() );
}

TEST_F( CounterTest, MessagesPostedToTheWindow )
{
	CounterWindow* pcWin = Open( new CounterWindow() );
	ASSERT_TRUE( guitest::PostAndSync( pcWin, ID_INCREMENT ) );
	ASSERT_TRUE( guitest::PostAndSync( pcWin, ID_INCREMENT ) );
	guitest::Locked cLock( pcWin );
	EXPECT_EQ( 2, pcWin->GetCount() );
}

TEST_F( CounterTest, MessagesPostedToAView )
{
	Window* pcWin = new Window( Rect( 100, 100, 300, 160 ), "labels", "Labels" );
	LabelView* pcView = new LabelView();
	pcWin->AddChild( pcView );
	Open( pcWin );

	Message cMsg( ID_SET_LABEL );
	cMsg.AddString( "text", "hello" );
	ASSERT_TRUE( guitest::PostToView( pcView, &cMsg ) );

	guitest::Locked cLock( pcWin );
	EXPECT_STREQ( "hello", pcView->GetString().c_str() );
}

// Something the window does in its own time: wait for it, don't sleep
struct CountIs
{
	CountIs( CounterWindow* pcWin, int nCount ) : m_pcWin( pcWin ), m_nCount( nCount ) {}
	bool operator()() const { guitest::Locked cLock( m_pcWin ); return m_pcWin->GetCount() == m_nCount; }
	CounterWindow* m_pcWin;
	int m_nCount;
};

TEST_F( CounterTest, WaitForACondition )
{
	CounterWindow* pcWin = Open( new CounterWindow() );
	pcWin->PostMessage( ID_INCREMENT );		// no Sync: WaitFor polls instead
	EXPECT_TRUE( guitest::WaitFor( CountIs( pcWin, 1 ) ) );
}

}	// namespace
