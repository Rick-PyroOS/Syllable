/*  os::ConcurrentQueue and os::ConcurrentRingBuffer: the same as os::Queue
 *  and os::RingBuffer from one thread, then many threads at once: nothing
 *  lost or doubled, waits that wake, timeouts that end, Close() that lets
 *  everyone go.
 */

#include <gtest/gtest.h>

#include <util/concurrent.h>
#include <atheos/threads.h>
#include <atheos/time.h>

#include <vector>

using namespace os;

namespace
{

typedef ConcurrentQueue<int> IntQueue;
typedef ConcurrentRingBuffer<int> IntRing;

/* A thread: Run() is what it does.  Start() it, then Join() it. */
class Job
{
public:
	Job() : m_hThread( -1 ) {}
	virtual ~Job() {}
	virtual void Run() = 0;
	void Start()
	{
		m_hThread = spawn_thread( "concurrent_test", (void*)Entry, NORMAL_PRIORITY, 0, this );
		resume_thread( m_hThread );
	}
	void Join() { wait_for_thread( m_hThread ); }
private:
	static int32 Entry( void* pData )
	{
		static_cast<Job*>( pData )->Run();
		return 0;
	}
	thread_id m_hThread;
};

/* Pushes nCount numbers: nFirst, nFirst + 1, ... */
class Producer : public Job
{
public:
	Producer( IntQueue* pcQueue, int nFirst, int nCount ) : m_pcQueue( pcQueue ), m_nFirst( nFirst ), m_nCount( nCount ) {}
	void Run()
	{
		for( int i = 0; i < m_nCount; i++ )
			m_pcQueue->Push( m_nFirst + i );
	}
private:
	IntQueue* m_pcQueue;
	int m_nFirst, m_nCount;
};

/* Pops until the queue is closed and empty, keeping what it got */
class Consumer : public Job
{
public:
	explicit Consumer( IntQueue* pcQueue ) : m_pcQueue( pcQueue ) {}
	void Run()
	{
		int nValue;
		while( m_pcQueue->Pop( &nValue ) )
			m_cGot.push_back( nValue );
	}
	std::vector<int> m_cGot;
private:
	IntQueue* m_pcQueue;
};

/* Waits once in Pop(), for as long as nTimeout */
class Waiter : public Job
{
public:
	Waiter( IntQueue* pcQueue, bigtime_t nTimeout ) : m_pcQueue( pcQueue ), m_nTimeout( nTimeout ), m_bGot( false ), m_nValue( -1 ), m_nTook( 0 ) {}
	void Run()
	{
		bigtime_t nStart = get_system_time();
		m_bGot = m_pcQueue->Pop( &m_nValue, m_nTimeout );
		m_nTook = get_system_time() - nStart;
	}
	IntQueue* m_pcQueue;
	bigtime_t m_nTimeout;
	bool m_bGot;
	int m_nValue;
	bigtime_t m_nTook;
};

/* Pushes nCount numbers into a ring, keeping no count of its own */
class RingProducer : public Job
{
public:
	RingProducer( IntRing* pcRing, int nCount, bool bRetry ) : m_pcRing( pcRing ), m_nCount( nCount ), m_bRetry( bRetry ) {}
	void Run()
	{
		for( int i = 0; i < m_nCount; i++ )
		{
			while( !m_pcRing->Push( i ) && m_bRetry )
				snooze( 100 );			// full (REJECT): try again soon
		}
		m_pcRing->Close();
	}
private:
	IntRing* m_pcRing;
	int m_nCount;
	bool m_bRetry;
};

/* Reads from a ring in blocks until it is closed and empty */
class RingConsumer : public Job
{
public:
	RingConsumer( IntRing* pcRing, int nBlock ) : m_pcRing( pcRing ), m_nBlock( nBlock ) {}
	void Run()
	{
		std::vector<int> cBlock( m_nBlock );
		int nGot;
		while( ( nGot = m_pcRing->Read( &cBlock[0], m_nBlock ) ) > 0 )
			m_cGot.insert( m_cGot.end(), cBlock.begin(), cBlock.begin() + nGot );
	}
	std::vector<int> m_cGot;
private:
	IntRing* m_pcRing;
	int m_nBlock;
};

struct Sum
{
	explicit Sum( int* pnTotal ) : m_pnTotal( pnTotal ) {}
	void operator()( int n ) const { *m_pnTotal += n; }
	int* m_pnTotal;
};

} // namespace

// --- os::ConcurrentQueue, one thread ------------------------------------

TEST( ConcurrentQueue, FirstInFirstOut )
{
	IntQueue cQueue;
	EXPECT_TRUE( cQueue.IsEmpty() );
	EXPECT_TRUE( cQueue.Push( 1 ) );
	EXPECT_TRUE( cQueue.Push( 2 ) );
	EXPECT_TRUE( cQueue.Push( 3 ) );
	EXPECT_EQ( 3, cQueue.Size() );
	EXPECT_EQ( 3, cQueue.Count() );
	int nValue = 0;
	ASSERT_TRUE( cQueue.Peek( &nValue ) );
	EXPECT_EQ( 1, nValue );
	ASSERT_TRUE( cQueue.Pop( &nValue ) );
	EXPECT_EQ( 1, nValue );
	ASSERT_TRUE( cQueue.TryPop( &nValue ) );
	EXPECT_EQ( 2, nValue );
	EXPECT_TRUE( cQueue.Pop() );
	EXPECT_FALSE( cQueue.TryPop( &nValue ) );
	EXPECT_FALSE( cQueue.Peek( &nValue ) );
	EXPECT_TRUE( cQueue.IsEmpty() );
}

TEST( ConcurrentQueue, PopTimesOut )
{
	IntQueue cQueue;
	bigtime_t nStart = get_system_time();
	int nValue = 42;
	EXPECT_FALSE( cQueue.Pop( &nValue, 50000 ) );
	bigtime_t nTook = get_system_time() - nStart;
	EXPECT_EQ( 42, nValue );
	EXPECT_GE( nTook, 40000 );
	EXPECT_LT( nTook, 2000000 );
	EXPECT_FALSE( cQueue.Pop( &nValue, -5 ) );		// a negative timeout is 0
}

TEST( ConcurrentQueue, TakeAllClearAndForEach )
{
	IntQueue cQueue;
	for( int i = 1; i <= 5; i++ )
		cQueue.Push( i );
	int nTotal = 0;
	cQueue.ForEach( Sum( &nTotal ) );
	EXPECT_EQ( 15, nTotal );
	std::vector<int> cAll;
	cAll.push_back( 0 );
	EXPECT_EQ( 5, cQueue.TakeAll( &cAll ) );
	ASSERT_EQ( 6u, cAll.size() );
	EXPECT_EQ( 0, cAll[0] );
	EXPECT_EQ( 5, cAll[5] );
	EXPECT_TRUE( cQueue.IsEmpty() );
	EXPECT_FALSE( cQueue.TryPop() );			// no tokens left over

	cQueue.Push( 7 );
	cQueue.Push( 8 );
	cQueue.Clear();
	EXPECT_FALSE( cQueue.TryPop() );
	cQueue.Push( 9 );
	int nValue = 0;
	ASSERT_TRUE( cQueue.TryPop( &nValue ) );
	EXPECT_EQ( 9, nValue );
}

TEST( ConcurrentQueue, CloseDrainsThenEnds )
{
	IntQueue cQueue;
	cQueue.Push( 1 );
	cQueue.Push( 2 );
	cQueue.Close();
	cQueue.Close();						// twice is fine
	EXPECT_TRUE( cQueue.IsClosed() );
	EXPECT_FALSE( cQueue.Push( 3 ) );
	int nValue = 0;
	ASSERT_TRUE( cQueue.Pop( &nValue ) );
	EXPECT_EQ( 1, nValue );
	ASSERT_TRUE( cQueue.Pop( &nValue ) );
	EXPECT_EQ( 2, nValue );
	bigtime_t nStart = get_system_time();
	EXPECT_FALSE( cQueue.Pop( &nValue ) );		// no timeout, yet it returns
	EXPECT_FALSE( cQueue.Pop( &nValue ) );
	EXPECT_LT( get_system_time() - nStart, 1000000 );
}

TEST( ConcurrentQueue, LockMakesStepsOne )
{
	IntQueue cQueue;
	cQueue.Lock();
	cQueue.Push( 1 );					// the lock is recursive
	cQueue.Push( 2 );
	EXPECT_EQ( 2, cQueue.Size() );
	cQueue.Unlock();
	EXPECT_EQ( 2, cQueue.Size() );
}

// --- os::ConcurrentQueue, many threads ----------------------------------

TEST( ConcurrentQueue, PopWakesWhenSomethingComes )
{
	IntQueue cQueue;
	Waiter cWaiter( &cQueue, INFINITE_TIMEOUT );
	cWaiter.Start();
	snooze( 30000 );					// it is waiting by now
	cQueue.Push( 77 );
	cWaiter.Join();
	EXPECT_TRUE( cWaiter.m_bGot );
	EXPECT_EQ( 77, cWaiter.m_nValue );
	EXPECT_GE( cWaiter.m_nTook, 20000 );
}

TEST( ConcurrentQueue, CloseWakesEveryWaiter )
{
	IntQueue cQueue;
	std::vector<Waiter*> cWaiters;
	for( int i = 0; i < 6; i++ )
	{
		cWaiters.push_back( new Waiter( &cQueue, INFINITE_TIMEOUT ) );
		cWaiters.back()->Start();
	}
	snooze( 30000 );
	cQueue.Push( 5 );					// one of them gets it
	cQueue.Close();						// the others go home empty
	int nGot = 0;
	for( size_t i = 0; i < cWaiters.size(); i++ )
	{
		cWaiters[i]->Join();
		if( cWaiters[i]->m_bGot )
		{
			nGot++;
			EXPECT_EQ( 5, cWaiters[i]->m_nValue );
		}
		delete cWaiters[i];
	}
	EXPECT_EQ( 1, nGot );
}

TEST( ConcurrentQueue, ManyProducersManyConsumers )
{
	const int nProducers = 4, nConsumers = 4, nEach = 20000;
	IntQueue cQueue;
	std::vector<Producer*> cProducers;
	std::vector<Consumer*> cConsumers;
	for( int i = 0; i < nConsumers; i++ )
	{
		cConsumers.push_back( new Consumer( &cQueue ) );
		cConsumers.back()->Start();
	}
	for( int i = 0; i < nProducers; i++ )
	{
		cProducers.push_back( new Producer( &cQueue, i * nEach, nEach ) );
		cProducers.back()->Start();
	}
	for( int i = 0; i < nProducers; i++ )
	{
		cProducers[i]->Join();
		delete cProducers[i];
	}
	cQueue.Close();

	std::vector<int> cSeen( nProducers * nEach, 0 );
	bool bInOrder = true;
	int nTotal = 0;
	for( int i = 0; i < nConsumers; i++ )
	{
		cConsumers[i]->Join();
		// each consumer sees each producer's numbers in the order pushed
		std::vector<int> cLast( nProducers, -1 );
		const std::vector<int>& cGot = cConsumers[i]->m_cGot;
		for( size_t j = 0; j < cGot.size(); j++ )
		{
			int nProducer = cGot[j] / nEach;
			bInOrder = bInOrder && cGot[j] > cLast[nProducer];
			cLast[nProducer] = cGot[j];
			cSeen[cGot[j]]++;
		}
		nTotal += int( cGot.size() );
		delete cConsumers[i];
	}
	EXPECT_TRUE( bInOrder );
	EXPECT_EQ( nProducers * nEach, nTotal );
	bool bEachOnce = true;
	for( size_t i = 0; i < cSeen.size(); i++ )
		bEachOnce = bEachOnce && cSeen[i] == 1;
	EXPECT_TRUE( bEachOnce );
	EXPECT_TRUE( cQueue.IsEmpty() );
}

TEST( ConcurrentQueue, ClearWhileOthersWait )
{
	// waiters that wake for an item Clear() took must wait again, not
	// return something or fail
	IntQueue cQueue;
	std::vector<Waiter*> cWaiters;
	for( int i = 0; i < 3; i++ )
	{
		cWaiters.push_back( new Waiter( &cQueue, 300000 ) );
		cWaiters.back()->Start();
	}
	snooze( 20000 );
	for( int nRound = 0; nRound < 50; nRound++ )
	{
		cQueue.Lock();
		cQueue.Push( nRound );
		cQueue.Clear();
		cQueue.Unlock();
	}
	cQueue.Push( 1000 );
	int nGot = 0;
	for( size_t i = 0; i < cWaiters.size(); i++ )
	{
		cWaiters[i]->Join();
		if( cWaiters[i]->m_bGot )
		{
			nGot++;
			EXPECT_EQ( 1000, cWaiters[i]->m_nValue );
		}
		else
		{
			EXPECT_GE( cWaiters[i]->m_nTook, 250000 );	// it timed out, and only then
		}
		delete cWaiters[i];
	}
	EXPECT_EQ( 1, nGot );
}

// --- os::ConcurrentRingBuffer, one thread -------------------------------

TEST( ConcurrentRingBuffer, BehavesAsARingBuffer )
{
	IntRing cRing( 3 );
	EXPECT_EQ( 3, cRing.GetCapacity() );
	EXPECT_EQ( IntRing::WhenFull( RingBuffer<int>::OVERWRITE ), cRing.GetWhenFull() );
	for( int i = 1; i <= 5; i++ )
		EXPECT_TRUE( cRing.Push( i ) );
	EXPECT_TRUE( cRing.IsFull() );
	EXPECT_EQ( 0, cRing.GetFree() );
	EXPECT_EQ( 2, cRing.GetDropped() );
	std::vector<int> cItems = cRing.Items();
	ASSERT_EQ( 3u, cItems.size() );
	EXPECT_EQ( 3, cItems[0] );
	int nValue = 0;
	ASSERT_TRUE( cRing.Peek( &nValue ) );
	EXPECT_EQ( 3, nValue );
	ASSERT_TRUE( cRing.Pop( &nValue ) );
	EXPECT_EQ( 3, nValue );
	EXPECT_TRUE( cRing.Pop() );
	EXPECT_TRUE( cRing.TryPop( &nValue ) );
	EXPECT_EQ( 5, nValue );
	EXPECT_FALSE( cRing.TryPop() );			// three pushes overwrote: no stray tokens
	cRing.ResetDropped();
	EXPECT_EQ( 0, cRing.GetDropped() );
}

TEST( ConcurrentRingBuffer, RejectWriteReadDiscard )
{
	IntRing cRing( 4, RingBuffer<int>::REJECT );
	int anIn[6] = { 1, 2, 3, 4, 5, 6 }, anOut[6] = { 0 };
	EXPECT_EQ( 4, cRing.Write( anIn, 6 ) );
	EXPECT_EQ( 2, cRing.GetDropped() );
	EXPECT_FALSE( cRing.Push( 7 ) );
	EXPECT_EQ( 3, cRing.GetDropped() );
	EXPECT_EQ( 3, cRing.Read( anOut, 3, 0 ) );
	EXPECT_EQ( 1, anOut[0] );
	EXPECT_EQ( 3, anOut[2] );
	EXPECT_EQ( 1, cRing.Read( anOut, 6, 0 ) );		// what's there, not what was asked
	EXPECT_EQ( 4, anOut[0] );
	EXPECT_EQ( 0, cRing.Read( anOut, 6, 0 ) );
	EXPECT_EQ( 0, cRing.Read( anOut, 0 ) );

	cRing.Write( anIn, 4 );
	EXPECT_EQ( 2, cRing.Discard( 2 ) );
	EXPECT_EQ( 2, cRing.Size() );
	cRing.SetCapacity( 1 );					// keeps the newest
	EXPECT_EQ( 1, cRing.Count() );
	ASSERT_TRUE( cRing.TryPop( &anOut[0] ) );
	EXPECT_EQ( 4, anOut[0] );
	EXPECT_FALSE( cRing.TryPop() );
	cRing.SetCapacity( 4 );
	cRing.Push( 1 );
	cRing.Push( 2 );
	cRing.Clear();
	EXPECT_TRUE( cRing.IsEmpty() );
	EXPECT_FALSE( cRing.TryPop() );
	cRing.SetWhenFull( RingBuffer<int>::OVERWRITE );
	EXPECT_EQ( IntRing::WhenFull( RingBuffer<int>::OVERWRITE ), cRing.GetWhenFull() );
}

TEST( ConcurrentRingBuffer, ReadTimesOutAndCloseEnds )
{
	IntRing cRing( 8 );
	int anOut[4];
	bigtime_t nStart = get_system_time();
	EXPECT_EQ( 0, cRing.Read( anOut, 4, 30000 ) );
	EXPECT_GE( get_system_time() - nStart, 20000 );
	cRing.Push( 1 );
	cRing.Close();
	EXPECT_TRUE( cRing.IsClosed() );
	EXPECT_FALSE( cRing.Push( 2 ) );
	EXPECT_EQ( 0, cRing.Write( anOut, 2 ) );
	EXPECT_EQ( 1, cRing.Read( anOut, 4 ) );
	nStart = get_system_time();
	EXPECT_EQ( 0, cRing.Read( anOut, 4 ) );		// no timeout, yet it returns
	EXPECT_FALSE( cRing.Pop() );
	EXPECT_LT( get_system_time() - nStart, 1000000 );
}

// --- os::ConcurrentRingBuffer, two threads ------------------------------

TEST( ConcurrentRingBuffer, NothingLostWhenTheProducerRetries )
{
	// REJECT, and the producer tries again when full: every number comes
	// through, in order, though the ring is much smaller than the stream
	const int nCount = 100000;
	IntRing cRing( 64, RingBuffer<int>::REJECT );
	RingConsumer cConsumer( &cRing, 37 );
	RingProducer cProducer( &cRing, nCount, true );
	cConsumer.Start();
	cProducer.Start();
	cProducer.Join();
	cConsumer.Join();
	ASSERT_EQ( size_t( nCount ), cConsumer.m_cGot.size() );
	bool bInOrder = true;
	for( int i = 0; i < nCount; i++ )
		bInOrder = bInOrder && cConsumer.m_cGot[i] == i;
	EXPECT_TRUE( bInOrder );
}

TEST( ConcurrentRingBuffer, OverwriteLosesOnlyWhatItCounts )
{
	// OVERWRITE never makes the producer wait: what the consumer gets and
	// what was dropped add up to what was pushed, and what it gets is in
	// order
	const int nCount = 100000;
	IntRing cRing( 16 );
	RingConsumer cConsumer( &cRing, 5 );
	RingProducer cProducer( &cRing, nCount, false );
	cConsumer.Start();
	cProducer.Start();
	cProducer.Join();
	cConsumer.Join();
	EXPECT_EQ( nCount, int( cConsumer.m_cGot.size() ) + cRing.GetDropped() );
	bool bInOrder = true;
	for( size_t i = 1; i < cConsumer.m_cGot.size(); i++ )
		bInOrder = bInOrder && cConsumer.m_cGot[i] > cConsumer.m_cGot[i - 1];
	EXPECT_TRUE( bInOrder );
	EXPECT_EQ( nCount - 1, cConsumer.m_cGot.back() );	// the newest always gets through
}
