/*  os::PriorityQueue: push, pop and peek in order, ties in push order,
 *  PopIf() for timers, Remove() and RemoveIf(), other orders, and a random
 *  run against a plain list.
 */

#include <gtest/gtest.h>

#include <util/priorityqueue.h>

#include <functional>
#include <string>
#include <vector>
#include <cstdlib>

namespace os
{
/* reaches into the queue to make its push counter run out */
class PriorityQueueTest
{
public:
	template<class Q> static void SetNextOrder( Q& cQueue, size_t nOrder ) { cQueue.m_nNextOrder = nOrder; }
};
} // namespace os

using namespace os;

namespace
{

std::vector<int> Popped( PriorityQueue<int>& cQueue )
{
	std::vector<int> cOut;
	int nValue;
	while( cQueue.Pop( &nValue ) )
		cOut.push_back( nValue );
	return cOut;
}

std::vector<int> Ints( const char* pzList )
{
	std::vector<int> cOut;
	for( const char* p = pzList; *p != '\0'; )
	{
		char* pzEnd;
		cOut.push_back( int( strtol( p, &pzEnd, 10 ) ) );
		p = *pzEnd == ',' ? pzEnd + 1 : pzEnd;
	}
	return cOut;
}

/* A timer: due when, and which one.  Equal by its ID, ordered by when. */
struct Timer
{
	int m_nDue;
	int m_nID;
	bool operator<( const Timer& cOther ) const { return m_nDue < cOther.m_nDue; }
	bool operator==( const Timer& cOther ) const { return m_nID == cOther.m_nID; }
};
Timer MakeTimer( int nDue, int nID )
{
	Timer cTimer = { nDue, nID };
	return cTimer;
}

struct Due
{
	explicit Due( int nNow ) : m_nNow( nNow ) {}
	bool operator()( const Timer& cTimer ) const { return cTimer.m_nDue <= m_nNow; }
	int m_nNow;
};

struct IsOdd
{
	bool operator()( int n ) const { return n % 2 != 0; }
};

struct Sum
{
	explicit Sum( int* pnTotal ) : m_pnTotal( pnTotal ) {}
	void operator()( int n ) const { *m_pnTotal += n; }
	int* m_pnTotal;
};

bool ByLength( const std::string& cA, const std::string& cB )
{
	return cA.size() < cB.size();
}

/* Orders by distance from a point it carries: a comparison with state */
struct NearestTo
{
	explicit NearestTo( int nPoint = 0 ) : m_nPoint( nPoint ) {}
	bool operator()( int a, int b ) const { return abs( a - m_nPoint ) < abs( b - m_nPoint ); }
	int m_nPoint;
};

struct Counted
{
	static int s_nLive;
	int m_nValue;
	explicit Counted( int nValue = 0 ) : m_nValue( nValue ) { s_nLive++; }
	Counted( const Counted& cOther ) : m_nValue( cOther.m_nValue ) { s_nLive++; }
	Counted& operator=( const Counted& cOther ) { m_nValue = cOther.m_nValue; return *this; }
	~Counted() { s_nLive--; }
	bool operator<( const Counted& cOther ) const { return m_nValue < cOther.m_nValue; }
};
int Counted::s_nLive = 0;

} // namespace

TEST( PriorityQueue, StartsEmpty )
{
	PriorityQueue<int> cQueue;
	EXPECT_TRUE( cQueue.IsEmpty() );
	EXPECT_EQ( 0, cQueue.Size() );
	EXPECT_EQ( 0, cQueue.Count() );
	EXPECT_TRUE( cQueue.empty() );
	int nValue = 42;
	EXPECT_FALSE( cQueue.Pop( &nValue ) );
	EXPECT_FALSE( cQueue.Pop() );
	EXPECT_FALSE( cQueue.Peek( &nValue ) );
	EXPECT_FALSE( cQueue.PopIf( IsOdd(), &nValue ) );
	EXPECT_EQ( 42, nValue );
	EXPECT_FALSE( cQueue.Remove( 1 ) );
	EXPECT_EQ( 0, cQueue.RemoveIf( IsOdd() ) );
}

TEST( PriorityQueue, PopsSmallestFirst )
{
	PriorityQueue<int> cQueue;
	cQueue.Push( 5 ).Push( 1 ).Push( 4 ).Push( 1 ).Push( 3 ).Push( 9 ).Push( 2 ).Push( 6 );
	EXPECT_EQ( 8, cQueue.Size() );
	EXPECT_EQ( 1, cQueue.Peek() );
	EXPECT_EQ( Ints( "1,1,2,3,4,5,6,9" ), Popped( cQueue ) );
	EXPECT_TRUE( cQueue.IsEmpty() );
}

TEST( PriorityQueue, PeekLeavesItIn )
{
	PriorityQueue<int> cQueue;
	cQueue.Push( 3 ).Push( 2 );
	int nValue = 0;
	ASSERT_TRUE( cQueue.Peek( &nValue ) );
	EXPECT_EQ( 2, nValue );
	EXPECT_EQ( 2, cQueue.Size() );
	cQueue.Push( 1 );
	EXPECT_EQ( 1, cQueue.Peek() );
	EXPECT_TRUE( cQueue.Pop() );
	EXPECT_EQ( 2, cQueue.Peek() );
}

TEST( PriorityQueue, LargestFirstWithGreater )
{
	PriorityQueue<int, std::greater<int> > cQueue;
	cQueue.Push( 2 ).Push( 7 ).Push( 1 ).Push( 7 ).Push( 5 );
	std::vector<int> cOut;
	int nValue;
	while( cQueue.Pop( &nValue ) )
		cOut.push_back( nValue );
	EXPECT_EQ( Ints( "7,7,5,2,1" ), cOut );
}

TEST( PriorityQueue, FunctionAsOrder )
{
	typedef bool ( *Order )( const std::string&, const std::string& );
	PriorityQueue<std::string, Order> cQueue( ByLength );
	cQueue.Push( "three" ).Push( "a" ).Push( "of" ).Push( "b" ).Push( "seventeen" );
	std::string cWord;
	std::vector<std::string> cOut;
	while( cQueue.Pop( &cWord ) )
		cOut.push_back( cWord );
	ASSERT_EQ( 5u, cOut.size() );
	EXPECT_EQ( std::string( "a" ), cOut[0] );	// a and b are as long: pushed order
	EXPECT_EQ( std::string( "b" ), cOut[1] );
	EXPECT_EQ( std::string( "of" ), cOut[2] );
	EXPECT_EQ( std::string( "three" ), cOut[3] );
	EXPECT_EQ( std::string( "seventeen" ), cOut[4] );
}

TEST( PriorityQueue, OrderWithState )
{
	PriorityQueue<int, NearestTo> cQueue( NearestTo( 10 ) );
	cQueue.Push( 0 ).Push( 12 ).Push( 30 ).Push( 9 ).Push( 8 );
	EXPECT_EQ( 10, cQueue.GetCompare().m_nPoint );
	std::vector<int> cOut;
	int nValue;
	while( cQueue.Pop( &nValue ) )
		cOut.push_back( nValue );
	EXPECT_EQ( Ints( "9,12,8,0,30" ), cOut );	// 12 and 8 tie at 2: pushed order
}

TEST( PriorityQueue, EqualItemsKeepPushOrder )
{
	PriorityQueue<Timer> cTimers;
	for( int nID = 0; nID < 200; nID++ )
		cTimers.Push( MakeTimer( nID % 3, nID ) );	// due 0, 1, 2, 0, 1, 2, ...
	Timer cTimer;
	int nLastDue = -1, nLastID = -1;
	bool bInOrder = true;
	while( cTimers.Pop( &cTimer ) )
	{
		if( cTimer.m_nDue == nLastDue )
			bInOrder = bInOrder && cTimer.m_nID > nLastID;
		else
			bInOrder = bInOrder && cTimer.m_nDue > nLastDue;
		nLastDue = cTimer.m_nDue;
		nLastID = cTimer.m_nID;
	}
	EXPECT_TRUE( bInOrder );
}

TEST( PriorityQueue, PopIfTakesWhatIsDue )
{
	PriorityQueue<Timer> cTimers;
	cTimers.Push( MakeTimer( 30, 1 ) ).Push( MakeTimer( 10, 2 ) ).Push( MakeTimer( 20, 3 ) ).Push( MakeTimer( 10, 4 ) );

	std::vector<int> cFired;
	Timer cTimer;
	while( cTimers.PopIf( Due( 20 ), &cTimer ) )
		cFired.push_back( cTimer.m_nID );
	EXPECT_EQ( Ints( "2,4,3" ), cFired );
	EXPECT_EQ( 1, cTimers.Size() );
	EXPECT_EQ( 30, cTimers.Peek().m_nDue );

	EXPECT_FALSE( cTimers.PopIf( Due( 29 ), &cTimer ) );
	EXPECT_EQ( 1, cTimers.Size() );
	EXPECT_TRUE( cTimers.PopIf( Due( 30 ) ) );
	EXPECT_TRUE( cTimers.IsEmpty() );
}

TEST( PriorityQueue, RemoveFromAnywhere )
{
	PriorityQueue<Timer> cTimers;
	for( int nID = 0; nID < 50; nID++ )
		cTimers.Push( MakeTimer( ( nID * 37 ) % 50, nID ) );
	EXPECT_TRUE( cTimers.Has( MakeTimer( 0, 17 ) ) );
	EXPECT_TRUE( cTimers.Remove( MakeTimer( 0, 17 ) ) );	// found by ID
	EXPECT_FALSE( cTimers.Has( MakeTimer( 0, 17 ) ) );
	EXPECT_FALSE( cTimers.Remove( MakeTimer( 0, 17 ) ) );
	EXPECT_FALSE( cTimers.Remove( MakeTimer( 0, 99 ) ) );
	EXPECT_EQ( 49, cTimers.Size() );

	// take out the first one by Remove() too
	int nFirstID = cTimers.Peek().m_nID;
	EXPECT_TRUE( cTimers.Remove( cTimers.Peek() ) );
	EXPECT_TRUE( nFirstID != cTimers.Peek().m_nID );

	Timer cTimer;
	int nLastDue = -1;
	bool bInOrder = true;
	int nPopped = 0;
	while( cTimers.Pop( &cTimer ) )
	{
		bInOrder = bInOrder && cTimer.m_nDue >= nLastDue && cTimer.m_nID != 17 && cTimer.m_nID != nFirstID;
		nLastDue = cTimer.m_nDue;
		nPopped++;
	}
	EXPECT_TRUE( bInOrder );
	EXPECT_EQ( 48, nPopped );
}

TEST( PriorityQueue, RemoveTakesTheFirstPushedOfEquals )
{
	PriorityQueue<Timer> cTimers;
	cTimers.Push( MakeTimer( 5, 1 ) ).Push( MakeTimer( 9, 1 ) ).Push( MakeTimer( 1, 2 ) ).Push( MakeTimer( 7, 1 ) );
	EXPECT_TRUE( cTimers.Remove( MakeTimer( 0, 1 ) ) );	// the one due at 5
	std::vector<Timer> cLeft = cTimers.Sorted();
	ASSERT_EQ( 3u, cLeft.size() );
	EXPECT_EQ( 1, cLeft[0].m_nDue );
	EXPECT_EQ( 7, cLeft[1].m_nDue );
	EXPECT_EQ( 9, cLeft[2].m_nDue );
}

TEST( PriorityQueue, RemoveIfKeepsTheHeap )
{
	PriorityQueue<int> cQueue;
	for( int i = 0; i < 100; i++ )
		cQueue.Push( ( i * 61 ) % 100 );
	EXPECT_EQ( 50, cQueue.RemoveIf( IsOdd() ) );
	EXPECT_EQ( 50, cQueue.Size() );
	std::vector<int> cOut = Popped( cQueue );
	bool bInOrder = true;
	for( size_t i = 0; i < cOut.size(); i++ )
		bInOrder = bInOrder && cOut[i] == int( 2 * i );
	EXPECT_TRUE( bInOrder );
}

TEST( PriorityQueue, SortedAndForEachLeaveItAlone )
{
	PriorityQueue<int> cQueue;
	cQueue.Push( 4 ).Push( 2 ).Push( 8 ).Push( 6 );
	EXPECT_EQ( Ints( "2,4,6,8" ), cQueue.Sorted() );
	int nTotal = 0;
	cQueue.ForEach( Sum( &nTotal ) );
	EXPECT_EQ( 20, nTotal );
	EXPECT_EQ( 4, cQueue.Size() );
	EXPECT_EQ( Ints( "2,4,6,8" ), Popped( cQueue ) );
}

TEST( PriorityQueue, ClearSwapAndCopy )
{
	PriorityQueue<int> cA, cB;
	cA.Push( 3 ).Push( 1 );
	cB.Push( 9 );
	cA.Swap( cB );
	EXPECT_EQ( 1, cA.Size() );
	EXPECT_EQ( 9, cA.Peek() );
	EXPECT_EQ( 1, cB.Peek() );

	PriorityQueue<int> cC = cB;
	cC.Push( 0 );
	EXPECT_EQ( 1, cB.Peek() );
	EXPECT_EQ( 0, cC.Peek() );

	cC.Clear();
	EXPECT_TRUE( cC.IsEmpty() );
	cC.Reserve( 1000 );
	cC.Push( 5 );
	EXPECT_EQ( 5, cC.Peek() );
}

TEST( PriorityQueue, SwapTakesTheOrderAlong )
{
	PriorityQueue<int, NearestTo> cA( NearestTo( 0 ) ), cB( NearestTo( 100 ) );
	cA.Push( 10 ).Push( 90 );
	cB.Push( 10 ).Push( 90 );
	cA.Swap( cB );
	EXPECT_EQ( 90, cA.Peek() );
	EXPECT_EQ( 10, cB.Peek() );
	cA.Push( 99 );
	EXPECT_EQ( 99, cA.Peek() );
}

TEST( PriorityQueue, PopDestroysWhatItTakes )
{
	Counted::s_nLive = 0;
	{
		PriorityQueue<Counted> cQueue;
		for( int i = 0; i < 100; i++ )
			cQueue.Push( Counted( 100 - i ) );
		EXPECT_EQ( 100, Counted::s_nLive );
		for( int i = 0; i < 30; i++ )
			cQueue.Pop();
		EXPECT_EQ( 70, Counted::s_nLive );
		EXPECT_EQ( 31, cQueue.Peek().m_nValue );
		cQueue.Clear();
		EXPECT_EQ( 0, Counted::s_nLive );
		cQueue.Push( Counted( 1 ) );
	}
	EXPECT_EQ( 0, Counted::s_nLive );
}

TEST( PriorityQueue, PushCounterRunningOutKeepsTies )
{
	PriorityQueue<Timer> cTimers;
	cTimers.Push( MakeTimer( 1, 0 ) ).Push( MakeTimer( 2, 1 ) ).Push( MakeTimer( 1, 2 ) );
	PriorityQueueTest::SetNextOrder( cTimers, size_t( -3 ) );
	for( int nID = 3; nID < 10; nID++ )		// the counter runs out on the way
		cTimers.Push( MakeTimer( nID % 2 + 1, nID ) );
	std::vector<int> cIDs;
	Timer cTimer;
	while( cTimers.Pop( &cTimer ) )
		cIDs.push_back( cTimer.m_nID );
	// due 1: 0, 2, then 4, 6, 8 as pushed; due 2: 1, then 3, 5, 7, 9
	EXPECT_EQ( Ints( "0,2,4,6,8,1,3,5,7,9" ), cIDs );
}

TEST( PriorityQueue, ManyItems )
{
	PriorityQueue<int> cQueue;
	for( int i = 0; i < 100000; i++ )
		cQueue.Push( ( i * 7919 ) % 100000 );	// each of 0..99999 once
	int nValue, nExpected = 0;
	bool bInOrder = true;
	while( cQueue.Pop( &nValue ) )
		bInOrder = bInOrder && nValue == nExpected++;
	EXPECT_TRUE( bInOrder );
	EXPECT_EQ( 100000, nExpected );
}

/* Against a plain list of (value, ID), searched for the smallest value
 * and the lowest ID among equals: few values, so there are many ties. */
TEST( PriorityQueue, MatchesListOverRandomSteps )
{
	srand( 2718 );
	PriorityQueue<Timer> cQueue;
	std::vector<Timer> cModel;
	int nNextID = 0;
	for( int nStep = 0; nStep < 20000; nStep++ )
	{
		size_t nBest = 0;
		for( size_t i = 1; i < cModel.size(); i++ )
			if( cModel[i].m_nDue < cModel[nBest].m_nDue || ( cModel[i].m_nDue == cModel[nBest].m_nDue && cModel[i].m_nID < cModel[nBest].m_nID ) )
				nBest = i;
		int nOp = rand() % 10;
		if( nOp < 5 )
		{
			Timer cTimer = MakeTimer( rand() % 20, nNextID++ );
			cQueue.Push( cTimer );
			cModel.push_back( cTimer );
		}
		else if( nOp < 8 )
		{
			Timer cGot = MakeTimer( -1, -1 );
			bool bGot = cQueue.Pop( &cGot );
			ASSERT_EQ( !cModel.empty(), bGot ) << "step " << nStep;
			if( bGot )
			{
				ASSERT_EQ( cModel[nBest].m_nID, cGot.m_nID ) << "step " << nStep;
				cModel.erase( cModel.begin() + nBest );
			}
		}
		else if( nOp < 9 )
		{
			// cancel one, picked at random (or one that is not in)
			if( !cModel.empty() && rand() % 4 != 0 )
			{
				size_t nPick = size_t( rand() ) % cModel.size();
				ASSERT_TRUE( cQueue.Remove( cModel[nPick] ) ) << "step " << nStep;
				cModel.erase( cModel.begin() + nPick );
			}
			else
			{
				ASSERT_FALSE( cQueue.Remove( MakeTimer( 0, -5 ) ) ) << "step " << nStep;
			}
		}
		else
		{
			Timer cGot = MakeTimer( -1, -1 );
			ASSERT_EQ( !cModel.empty(), cQueue.Peek( &cGot ) ) << "step " << nStep;
			if( !cModel.empty() )
			{
				ASSERT_EQ( cModel[nBest].m_nID, cGot.m_nID ) << "step " << nStep;
			}
		}
		ASSERT_EQ( int( cModel.size() ), cQueue.Size() ) << "step " << nStep;
	}
	// what is left comes out in the model's order
	std::vector<Timer> cLeft = cQueue.Sorted();
	ASSERT_EQ( cModel.size(), cLeft.size() );
	for( size_t i = 1; i < cLeft.size(); i++ )
	{
		ASSERT_TRUE( cLeft[i - 1].m_nDue < cLeft[i].m_nDue || ( cLeft[i - 1].m_nDue == cLeft[i].m_nDue && cLeft[i - 1].m_nID < cLeft[i].m_nID ) );
	}
}
