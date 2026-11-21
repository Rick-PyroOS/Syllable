/*  os::Stack (last in, first out) and os::Queue (first in, first out):
 *  push, pop, peek, size, iterating, and a random run against std::deque.
 */

#include <gtest/gtest.h>

#include <util/stack.h>
#include <util/queue.h>

#include <deque>
#include <string>
#include <vector>
#include <cstdlib>

using namespace os;

namespace
{

struct Collect
{
	explicit Collect( std::vector<int>* pcOut ) : m_pcOut( pcOut ) {}
	void operator()( int nValue ) const { m_pcOut->push_back( nValue ); }
	std::vector<int>* m_pcOut;
};

struct Double
{
	void operator()( int& nValue ) const { nValue *= 2; }
};

std::vector<int> Seq( int a, int b, int c )
{
	std::vector<int> cOut;
	cOut.push_back( a );
	cOut.push_back( b );
	cOut.push_back( c );
	return cOut;
}

/* A value that counts its live copies, to see that Pop() and Clear()
 * destroy what they take out. */
struct Counted
{
	static int s_nLive;
	int m_nValue;
	explicit Counted( int nValue = 0 ) : m_nValue( nValue ) { s_nLive++; }
	Counted( const Counted& cOther ) : m_nValue( cOther.m_nValue ) { s_nLive++; }
	Counted& operator=( const Counted& cOther ) { m_nValue = cOther.m_nValue; return *this; }
	~Counted() { s_nLive--; }
	bool operator==( const Counted& cOther ) const { return m_nValue == cOther.m_nValue; }
};
int Counted::s_nLive = 0;

} // namespace

// --- os::Stack ---------------------------------------------------------

TEST( Stack, StartsEmpty )
{
	Stack<int> cStack;
	EXPECT_TRUE( cStack.IsEmpty() );
	EXPECT_EQ( 0, cStack.Size() );
	EXPECT_EQ( 0, cStack.Count() );
	EXPECT_TRUE( cStack.empty() );
	EXPECT_TRUE( cStack.begin() == cStack.end() );
}

TEST( Stack, PushPopIsLastInFirstOut )
{
	Stack<int> cStack;
	cStack.Push( 1 ).Push( 2 ).Push( 3 );
	EXPECT_EQ( 3, cStack.Size() );
	EXPECT_FALSE( cStack.IsEmpty() );

	int nValue = 0;
	ASSERT_TRUE( cStack.Pop( &nValue ) );
	EXPECT_EQ( 3, nValue );
	ASSERT_TRUE( cStack.Pop( &nValue ) );
	EXPECT_EQ( 2, nValue );
	cStack.Push( 4 );
	ASSERT_TRUE( cStack.Pop( &nValue ) );
	EXPECT_EQ( 4, nValue );
	ASSERT_TRUE( cStack.Pop( &nValue ) );
	EXPECT_EQ( 1, nValue );
	EXPECT_TRUE( cStack.IsEmpty() );
}

TEST( Stack, PopOnEmptyFailsAndLeavesValue )
{
	Stack<int> cStack;
	int nValue = 42;
	EXPECT_FALSE( cStack.Pop( &nValue ) );
	EXPECT_EQ( 42, nValue );
	EXPECT_FALSE( cStack.Pop() );
	EXPECT_FALSE( cStack.Peek( &nValue ) );
	EXPECT_EQ( 42, nValue );
}

TEST( Stack, PopWithoutValueDropsTop )
{
	Stack<int> cStack;
	cStack.Push( 1 ).Push( 2 );
	EXPECT_TRUE( cStack.Pop() );
	EXPECT_EQ( 1, cStack.Peek() );
	EXPECT_EQ( 1, cStack.Size() );
}

TEST( Stack, PeekLeavesTopOn )
{
	Stack<std::string> cStack;
	cStack.Push( "type a" ).Push( "type b" ).Push( "delete" );
	EXPECT_EQ( std::string( "delete" ), cStack.Peek() );
	std::string cTop;
	ASSERT_TRUE( cStack.Peek( &cTop ) );
	EXPECT_EQ( std::string( "delete" ), cTop );
	EXPECT_EQ( 3, cStack.Size() );

	const Stack<std::string>& cConst = cStack;
	EXPECT_EQ( std::string( "delete" ), cConst.Peek() );
}

TEST( Stack, PeekChangesTopInPlace )
{
	Stack<int> cStack;
	cStack.Push( 1 ).Push( 2 );
	cStack.Peek() = 20;
	int nValue = 0;
	cStack.Pop( &nValue );
	EXPECT_EQ( 20, nValue );
	EXPECT_EQ( 1, cStack.Peek() );
}

TEST( Stack, IteratesBottomToTopAndBack )
{
	Stack<int> cStack;
	cStack.Push( 1 ).Push( 2 ).Push( 3 );

	std::vector<int> cOrder( cStack.begin(), cStack.end() );
	EXPECT_EQ( Seq( 1, 2, 3 ), cOrder );
	std::vector<int> cReversed( cStack.rbegin(), cStack.rend() );
	EXPECT_EQ( Seq( 3, 2, 1 ), cReversed );

	std::vector<int> cSeen;
	cStack.ForEach( Collect( &cSeen ) );
	EXPECT_EQ( Seq( 1, 2, 3 ), cSeen );
	cSeen.clear();
	cStack.ForEachFromTop( Collect( &cSeen ) );
	EXPECT_EQ( Seq( 3, 2, 1 ), cSeen );

	cStack.ForEach( Double() );
	EXPECT_EQ( 6, cStack.Peek() );
}

TEST( Stack, ClearSwapAndCompare )
{
	Stack<int> cA, cB;
	cA.Push( 1 ).Push( 2 );
	cB.Push( 1 ).Push( 2 );
	EXPECT_TRUE( cA == cB );
	cB.Push( 3 );
	EXPECT_TRUE( cA != cB );

	cA.Swap( cB );
	EXPECT_EQ( 3, cA.Size() );
	EXPECT_EQ( 2, cB.Size() );
	EXPECT_EQ( 3, cA.Peek() );

	cA.Clear();
	EXPECT_TRUE( cA.IsEmpty() );
	EXPECT_FALSE( cA.Pop() );
	cA.Push( 7 );
	EXPECT_EQ( 7, cA.Peek() );
}

TEST( Stack, CopiesAreIndependent )
{
	Stack<int> cA;
	cA.Push( 1 ).Push( 2 );
	Stack<int> cB = cA;
	cB.Push( 3 );
	EXPECT_EQ( 2, cA.Size() );
	EXPECT_EQ( 3, cB.Size() );
	cA = cB;
	EXPECT_TRUE( cA == cB );
}

TEST( Stack, PopDestroysWhatItTakes )
{
	Counted::s_nLive = 0;
	{
		Stack<Counted> cStack;
		for( int i = 0; i < 100; i++ )
			cStack.Push( Counted( i ) );
		EXPECT_EQ( 100, Counted::s_nLive );
		for( int i = 0; i < 40; i++ )
			cStack.Pop();
		EXPECT_EQ( 60, Counted::s_nLive );
		cStack.Clear();
		EXPECT_EQ( 0, Counted::s_nLive );
		cStack.Push( Counted( 1 ) );
	}
	EXPECT_EQ( 0, Counted::s_nLive );
}

TEST( Stack, ManyItems )
{
	Stack<int> cStack;
	for( int i = 0; i < 100000; i++ )
		cStack.Push( i );
	EXPECT_EQ( 100000, cStack.Size() );
	int nValue = -1, nExpected = 99999;
	bool bInOrder = true;
	while( cStack.Pop( &nValue ) )
		bInOrder = bInOrder && nValue == nExpected--;
	EXPECT_TRUE( bInOrder );
	EXPECT_EQ( -1, nExpected );
}

TEST( Stack, MatchesDequeOverRandomSteps )
{
	srand( 1234 );
	Stack<int> cStack;
	std::deque<int> cModel;
	for( int nStep = 0; nStep < 20000; nStep++ )
	{
		int nValue = rand();
		switch( rand() % 4 )
		{
			case 0:
			case 1:
				cStack.Push( nValue );
				cModel.push_back( nValue );
				break;
			case 2:
			{
				int nGot = -1;
				bool bGot = cStack.Pop( &nGot );
				ASSERT_EQ( !cModel.empty(), bGot ) << "step " << nStep;
				if( bGot )
				{
					ASSERT_EQ( cModel.back(), nGot ) << "step " << nStep;
					cModel.pop_back();
				}
				break;
			}
			default:
			{
				int nGot = -1;
				ASSERT_EQ( !cModel.empty(), cStack.Peek( &nGot ) ) << "step " << nStep;
				if( !cModel.empty() )
				{
					ASSERT_EQ( cModel.back(), nGot ) << "step " << nStep;
				}
			}
		}
		ASSERT_EQ( int( cModel.size() ), cStack.Size() ) << "step " << nStep;
	}
	EXPECT_TRUE( cModel == cStack.GetContainer() );
}

// --- os::Queue ---------------------------------------------------------

TEST( Queue, StartsEmpty )
{
	Queue<int> cQueue;
	EXPECT_TRUE( cQueue.IsEmpty() );
	EXPECT_EQ( 0, cQueue.Size() );
	EXPECT_EQ( 0, cQueue.Count() );
	EXPECT_TRUE( cQueue.empty() );
	EXPECT_TRUE( cQueue.begin() == cQueue.end() );
}

TEST( Queue, PushPopIsFirstInFirstOut )
{
	Queue<int> cQueue;
	cQueue.Push( 1 ).Push( 2 ).Push( 3 );
	EXPECT_EQ( 3, cQueue.Size() );

	int nValue = 0;
	ASSERT_TRUE( cQueue.Pop( &nValue ) );
	EXPECT_EQ( 1, nValue );
	cQueue.Push( 4 );
	ASSERT_TRUE( cQueue.Pop( &nValue ) );
	EXPECT_EQ( 2, nValue );
	ASSERT_TRUE( cQueue.Pop( &nValue ) );
	EXPECT_EQ( 3, nValue );
	ASSERT_TRUE( cQueue.Pop( &nValue ) );
	EXPECT_EQ( 4, nValue );
	EXPECT_TRUE( cQueue.IsEmpty() );
}

TEST( Queue, PopOnEmptyFailsAndLeavesValue )
{
	Queue<int> cQueue;
	int nValue = 42;
	EXPECT_FALSE( cQueue.Pop( &nValue ) );
	EXPECT_EQ( 42, nValue );
	EXPECT_FALSE( cQueue.Pop() );
	EXPECT_FALSE( cQueue.Peek( &nValue ) );
	EXPECT_EQ( 42, nValue );
}

TEST( Queue, PeekFrontAndBack )
{
	Queue<std::string> cJobs;
	cJobs.Push( "print" ).Push( "scan" ).Push( "fax" );
	EXPECT_EQ( std::string( "print" ), cJobs.Peek() );
	EXPECT_EQ( std::string( "fax" ), cJobs.PeekBack() );
	std::string cNext;
	ASSERT_TRUE( cJobs.Peek( &cNext ) );
	EXPECT_EQ( std::string( "print" ), cNext );
	EXPECT_EQ( 3, cJobs.Size() );

	cJobs.Peek() = "copy";
	cJobs.PeekBack() = "mail";
	const Queue<std::string>& cConst = cJobs;
	EXPECT_EQ( std::string( "copy" ), cConst.Peek() );
	EXPECT_EQ( std::string( "mail" ), cConst.PeekBack() );

	cJobs.Pop();
	cJobs.Pop();
	EXPECT_EQ( cJobs.Peek(), cJobs.PeekBack() );	// one item: front is back
}

TEST( Queue, IteratesFrontToBack )
{
	Queue<int> cQueue;
	cQueue.Push( 1 ).Push( 2 ).Push( 3 );
	std::vector<int> cOrder( cQueue.begin(), cQueue.end() );
	EXPECT_EQ( Seq( 1, 2, 3 ), cOrder );
	std::vector<int> cReversed( cQueue.rbegin(), cQueue.rend() );
	EXPECT_EQ( Seq( 3, 2, 1 ), cReversed );

	std::vector<int> cSeen;
	cQueue.ForEach( Collect( &cSeen ) );
	EXPECT_EQ( Seq( 1, 2, 3 ), cSeen );
	cQueue.ForEach( Double() );
	EXPECT_EQ( 2, cQueue.Peek() );
	EXPECT_EQ( 6, cQueue.PeekBack() );
}

TEST( Queue, ClearSwapAndCompare )
{
	Queue<int> cA, cB;
	cA.Push( 1 ).Push( 2 );
	cB.Push( 1 ).Push( 2 );
	EXPECT_TRUE( cA == cB );
	Queue<int> cC;
	cC.Push( 2 ).Push( 1 );
	EXPECT_TRUE( cA != cC );	// same items, other order

	cB.Push( 3 );
	cA.Swap( cB );
	EXPECT_EQ( 3, cA.Size() );
	EXPECT_EQ( 2, cB.Size() );

	cA.Clear();
	EXPECT_TRUE( cA.IsEmpty() );
	EXPECT_FALSE( cA.Pop() );
}

TEST( Queue, PopDestroysWhatItTakes )
{
	Counted::s_nLive = 0;
	{
		Queue<Counted> cQueue;
		for( int i = 0; i < 100; i++ )
			cQueue.Push( Counted( i ) );
		Counted cFirst;
		cQueue.Pop( &cFirst );
		EXPECT_EQ( 0, cFirst.m_nValue );
		EXPECT_EQ( 100, Counted::s_nLive );	// 99 in, and cFirst
		cQueue.Clear();
		EXPECT_EQ( 1, Counted::s_nLive );
	}
	EXPECT_EQ( 0, Counted::s_nLive );
}

TEST( Queue, LongRunsKeepOrder )
{
	// a queue that stays short while many items pass through it
	Queue<int> cQueue;
	int nNextIn = 0, nNextOut = 0;
	bool bInOrder = true;
	for( int nRound = 0; nRound < 50000; nRound++ )
	{
		cQueue.Push( nNextIn );
		cQueue.Push( nNextIn + 1 );
		nNextIn += 2;
		int nValue = -1;
		cQueue.Pop( &nValue );
		bInOrder = bInOrder && nValue == nNextOut++;
		if( nRound % 3 == 0 )
		{
			cQueue.Pop( &nValue );
			bInOrder = bInOrder && nValue == nNextOut++;
		}
	}
	EXPECT_TRUE( bInOrder );
	EXPECT_EQ( nNextIn - nNextOut, cQueue.Size() );
	EXPECT_EQ( nNextOut, cQueue.Peek() );
	EXPECT_EQ( nNextIn - 1, cQueue.PeekBack() );
}

TEST( Queue, MatchesDequeOverRandomSteps )
{
	srand( 4321 );
	Queue<int> cQueue;
	std::deque<int> cModel;
	for( int nStep = 0; nStep < 20000; nStep++ )
	{
		int nValue = rand();
		switch( rand() % 4 )
		{
			case 0:
			case 1:
				cQueue.Push( nValue );
				cModel.push_back( nValue );
				break;
			case 2:
			{
				int nGot = -1;
				bool bGot = cQueue.Pop( &nGot );
				ASSERT_EQ( !cModel.empty(), bGot ) << "step " << nStep;
				if( bGot )
				{
					ASSERT_EQ( cModel.front(), nGot ) << "step " << nStep;
					cModel.pop_front();
				}
				break;
			}
			default:
			{
				int nGot = -1;
				ASSERT_EQ( !cModel.empty(), cQueue.Peek( &nGot ) ) << "step " << nStep;
				if( !cModel.empty() )
				{
					ASSERT_EQ( cModel.front(), nGot ) << "step " << nStep;
					ASSERT_EQ( cModel.back(), cQueue.PeekBack() ) << "step " << nStep;
				}
			}
		}
		ASSERT_EQ( int( cModel.size() ), cQueue.Size() ) << "step " << nStep;
	}
	EXPECT_TRUE( cModel == cQueue.GetContainer() );
}

#if __cplusplus >= 201103L
TEST( StackQueue, RangeFor )
{
	Stack<int> cStack;
	Queue<int> cQueue;
	for( int i = 1; i <= 3; i++ )
	{
		cStack.Push( i );
		cQueue.Push( i );
	}
	int nSum = 0;
	for( int n : cStack )
		nSum += n;
	for( int& n : cQueue )
		n += 10;
	EXPECT_EQ( 6, nSum );
	EXPECT_EQ( 11, cQueue.Peek() );
}
#endif
