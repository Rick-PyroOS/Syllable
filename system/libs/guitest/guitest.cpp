/*  libguitest: Google Test for Syllable GUI code (see guitest.h) */

#include "guitest.h"

#include <cstdio>

#include <atheos/threads.h>
#include <util/application.h>
#include <util/handler.h>
#include <util/messenger.h>
#include <gui/control.h>

using namespace os;

namespace guitest
{

namespace
{

// Message codes of our own, in the users' range, unlikely to be anyone's
enum { GUITEST_SYNC = 9999901, GUITEST_SYNC_DONE };

// Answers GUITEST_SYNC.  Added to a looper for one Sync(): it handles the
// message only after everything that was in the looper's port before it.
class SyncHandler : public Handler
{
public:
	SyncHandler() : Handler( "guitest_sync" ) {}
	virtual void HandleMessage( Message* pcMsg )
	{
		if( pcMsg->GetCode() == GUITEST_SYNC && pcMsg->IsSourceWaiting() )
			pcMsg->SendReply( GUITEST_SYNC_DONE );
		else
			Handler::HandleMessage( pcMsg );
	}
};

bool HeldByMe( Looper* pcLooper )
{
	return pcLooper->IsLocked() && pcLooper->GetLockingThread() == get_thread_id( NULL );
}

}	// namespace

bool Sync( Looper* pcLooper, int nRounds, bigtime_t nTimeout )
{
	if( pcLooper == NULL )
		return false;
	if( HeldByMe( pcLooper ) )
	{
		ADD_FAILURE() << "guitest::Sync() called with the looper \"" << pcLooper->GetName().c_str()
			      << "\" locked by the test: it can't handle anything until it's unlocked";
		return false;
	}
	SyncHandler* pcHandler = new SyncHandler();
	pcLooper->Lock();
	pcLooper->AddHandler( pcHandler );
	pcLooper->Unlock();

	bool bOk = true;
	for( int i = 0; i < nRounds && bOk; ++i )
	{
		Message cMsg( GUITEST_SYNC );
		Message cReply;
		bOk = Messenger( pcHandler, pcLooper ).SendMessage( &cMsg, &cReply, nTimeout, nTimeout ) >= 0
		      && cReply.GetCode() == GUITEST_SYNC_DONE;
	}

	pcLooper->Lock();
	pcLooper->RemoveHandler( pcHandler );
	pcLooper->Unlock();
	delete pcHandler;
	return bOk;
}

bool PostAndSync( Looper* pcLooper, Message* pcMsg, Handler* pcTarget, bigtime_t nTimeout )
{
	if( pcLooper == NULL || pcMsg == NULL )
		return false;
	status_t nError = pcTarget ? pcLooper->PostMessage( pcMsg, pcTarget ) : pcLooper->PostMessage( pcMsg );
	return nError >= 0 && Sync( pcLooper, 1, nTimeout );
}

bool PostAndSync( Looper* pcLooper, uint32 nCode, Handler* pcTarget, bigtime_t nTimeout )
{
	Message cMsg( nCode );
	return PostAndSync( pcLooper, &cMsg, pcTarget, nTimeout );
}

bool PostToView( View* pcView, Message* pcMsg, bigtime_t nTimeout )
{
	if( pcView == NULL || pcView->GetWindow() == NULL )
		return false;
	return PostAndSync( pcView->GetWindow(), pcMsg, pcView, nTimeout );
}

bool Click( View* pcView, bigtime_t nTimeout )
{
	Control* pcControl = dynamic_cast<Control*>( pcView );
	if( pcControl == NULL || pcControl->GetWindow() == NULL )
	{
		ADD_FAILURE() << "guitest::Click(): not a control in a window";
		return false;
	}
	Window* pcWindow = pcControl->GetWindow();
	Looper* pcTargetLooper = NULL;
	status_t nError;
	{
		Locked cLock( pcWindow );
		pcControl->GetTarget( &pcTargetLooper );
		nError = pcControl->Invoke();
	}
	if( nError < 0 )
		return false;
	// The window first (the click may change the control), then whoever
	// the control's message went to, if that's another looper
	bool bOk = Sync( pcWindow, 1, nTimeout );
	if( bOk && pcTargetLooper != NULL && pcTargetLooper != pcWindow )
		bOk = Sync( pcTargetLooper, 1, nTimeout );
	return bOk;
}

void WindowTest::TearDown()
{
	// Terminate() is safe on a window that has already gone
	for( size_t i = 0; i < m_cWindows.size(); ++i )
		m_cWindows[i]->Terminate();
	m_cWindows.clear();
}

namespace
{

struct TestRun
{
	Application* pcApp;
	int nResult;
};

uint32 RunTestsThread( void* pData )
{
	TestRun* psRun = static_cast<TestRun*>( pData );
	psRun->nResult = RUN_ALL_TESTS();
	// The application quits (closing any windows left) and its Run() returns
	psRun->pcApp->PostMessage( M_QUIT );
	return 0;
}

}	// namespace

int RunAllTests( const char* pzMimeType )
{
	TestRun sRun;
	sRun.pcApp = new Application( pzMimeType );
	sRun.nResult = 1;

	// The tests on a thread of their own, the application's loop on this
	// one (Run() is where a program normally ends up)
	thread_id hTests = spawn_thread( "guitest", (void*)RunTestsThread, NORMAL_PRIORITY, 0, &sRun );
	if( hTests < 0 )
	{
		delete sRun.pcApp;
		fprintf( stderr, "guitest: can't start the test thread\n" );
		return 1;
	}
	resume_thread( hTests );
	sRun.pcApp->Run();		// returns once the tests are done; deletes the application
	wait_for_thread( hTests );
	return sRun.nResult;
}

}	// namespace guitest
