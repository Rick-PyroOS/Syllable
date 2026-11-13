/*  libguitest: Google Test for Syllable GUI code
 *
 *  Runs Google Test tests inside an os::Application, so they can create
 *  windows and views, post messages to them and check what they did.
 *
 *  Link libguitest (CMake: LIBS libguitest) and write tests as usual; its
 *  main() creates the application, runs the tests on a thread of their own
 *  while the application's loop runs on the main thread, as in a real
 *  program, and quits the application when they are done.  Programs with a
 *  main() of their own call guitest::RunAllTests() from it instead.
 *
 *  Threads.  Each os::Window is a looper with its own thread, so what a test
 *  posts to a window is handled later, on that thread.  Sync() waits until
 *  it has been; Locked holds a looper's lock while the test reads or calls
 *  into its views.  Google Test is not thread-safe on Syllable: make
 *  assertions (EXPECT_*, ASSERT_*) in the test only, never in a window's
 *  HandleMessage().  Don't Sync() a looper while holding its lock: the
 *  looper can't handle anything until it is released (Sync() fails instead
 *  of hanging).
 *
 *      TEST_F( MyWindowTest, ButtonCountsClicks )
 *      {
 *          MyWindow* pcWin = Open( new MyWindow() );
 *          ASSERT_TRUE( guitest::Click( pcWin->FindView( "inc" ) ) );
 *          guitest::Locked cLock( pcWin );
 *          EXPECT_EQ( 1, pcWin->GetCount() );
 *      }
 */

#ifndef __F_GUITEST_GUITEST_H__
#define __F_GUITEST_GUITEST_H__

#include <gtest/gtest.h>

#include <atheos/types.h>
#include <atheos/time.h>
#include <util/looper.h>
#include <util/message.h>
#include <gui/window.h>
#include <gui/view.h>

#include <vector>

namespace guitest
{

/** How long Sync() and the others wait by default: five seconds */
static const bigtime_t DEFAULT_TIMEOUT = 5000000LL;

/** Create an os::Application with this MIME type, run the tests in it and
 *  return RUN_ALL_TESTS()'s result.  Call it once, from main(), after
 *  ::testing::InitGoogleTest().  libguitest's own main() does both. */
int RunAllTests( const char* pzMimeType = "application/x-vnd.syllable-guitest" );

/** Wait until pcLooper has handled every message posted to it before this
 *  call.  nRounds > 1 also waits for what those messages posted to it in
 *  turn (an Invoke() from HandleMessage(), say), one round each.  Returns
 *  false if the looper didn't answer in time (or the caller holds its lock). */
bool Sync( os::Looper* pcLooper, int nRounds = 1, bigtime_t nTimeout = DEFAULT_TIMEOUT );

/** Post pcMsg to pcLooper (to pcTarget, one of its handlers or views, if
 *  given) and wait until it has been handled.  The message is copied:
 *  pcMsg stays the caller's. */
bool PostAndSync( os::Looper* pcLooper, os::Message* pcMsg, os::Handler* pcTarget = NULL,
		  bigtime_t nTimeout = DEFAULT_TIMEOUT );
bool PostAndSync( os::Looper* pcLooper, uint32 nCode, os::Handler* pcTarget = NULL,
		  bigtime_t nTimeout = DEFAULT_TIMEOUT );

/** Send a message to a view the way the app server would, and wait for it:
 *  posts to the view's window with the view as the target. */
bool PostToView( os::View* pcView, os::Message* pcMsg, bigtime_t nTimeout = DEFAULT_TIMEOUT );

/** "Click" a control (an os::Button, os::CheckBox...): Invoke() it under its
 *  window's lock, as a mouse click ends up doing, then Sync() its window and
 *  the looper its message goes to.  pcView must be an os::Control. */
bool Click( os::View* pcView, bigtime_t nTimeout = DEFAULT_TIMEOUT );

/** Poll cCond() every 10 ms until it is true or nTimeout passes; for work a
 *  window does in its own time (timers, other threads).  Returns the last
 *  cCond() result.  cCond is any function or functor returning bool. */
template<class Cond>
bool WaitFor( Cond cCond, bigtime_t nTimeout = DEFAULT_TIMEOUT )
{
	bigtime_t nEnd = get_system_time() + nTimeout;
	for( ;; )
	{
		if( cCond() )
			return true;
		if( get_system_time() >= nEnd )
			return false;
		snooze( 10000 );
	}
}

/** Holds a looper's (window's) lock for its lifetime, to read or call into
 *  the looper's views safely from the test:
 *      { guitest::Locked cLock( pcWin ); EXPECT_EQ( ..., pcView->GetValue() ); } */
class Locked
{
public:
	explicit Locked( os::Looper* pcLooper ) : m_pcLooper( pcLooper ) { m_pcLooper->Lock(); }
	~Locked() { m_pcLooper->Unlock(); }
private:
	Locked( const Locked& );
	Locked& operator=( const Locked& );
	os::Looper* m_pcLooper;
};

/** A test fixture for window tests: Open() shows a window and waits until
 *  it is running, and every window opened is closed (Terminate()) after the
 *  test, even when it fails.  A window the test closes itself is fine too. */
class WindowTest : public ::testing::Test
{
protected:
	template<class W>
	W* Open( W* pcWindow )
	{
		m_cWindows.push_back( pcWindow );
		pcWindow->Show();
		EXPECT_TRUE( Sync( pcWindow ) ) << "the window didn't start";
		return pcWindow;
	}
	virtual void TearDown();
private:
	std::vector<os::Window*> m_cWindows;
};

}	// namespace guitest

#endif	// __F_GUITEST_GUITEST_H__
