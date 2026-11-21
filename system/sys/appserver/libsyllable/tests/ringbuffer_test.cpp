/*  os::RingBuffer: push and pop going round, overwriting or rejecting when
 *  full, Write() and Read() in runs, positions, changing the capacity,
 *  and a random run of each kind against std::deque.
 */

#include <gtest/gtest.h>

#include <util/ringbuffer.h>

#include <deque>
#include <string>
#include <vector>
#include <cstdlib>
#include <stdio.h>

using namespace os;

namespace
{

typedef RingBuffer<int> Ring;

std::string Joined( const Ring& cRing )
{
	std::string cText;
	char zNumber[16];
	for( Ring::const_iterator i = cRing.begin(); i != cRing.end(); ++i )
	{
		snprintf( zNumber, sizeof( zNumber ), "%s%d", i == cRing.begin() ? "" : ",", *i );
		cText += zNumber;
	}
	return cText;
}

struct Sum
{
	explicit Sum( int* pnTotal ) : m_pnTotal( pnTotal ) {}
	void operator()( int n ) const { *m_pnTotal += n; }
	int* m_pnTotal;
};

struct Negate
{
	void operator()( int& n ) const { n = -n; }
};

struct Counted
{
	static int s_nLive;
	int m_nValue;
	explicit Counted( int nValue = 0 ) : m_nValue( nValue ) { s_nLive++; }
	Counted( const Counted& cOther ) : m_nValue( cOther.m_nValue ) { s_nLive++; }
	Counted& operator=( const Counted& cOther ) { m_nValue = cOther.m_nValue; return *this; }
	~Counted() { s_nLive--; }
};
int Counted::s_nLive = 0;

/* A value that holds something until it is set back to its default */
struct Holder
{
	static int s_nHeld;
	bool m_bHolds;
	Holder() : m_bHolds( false ) {}
	explicit Holder( bool bHolds ) : m_bHolds( bHolds ) { s_nHeld += bHolds; }
	Holder( const Holder& cOther ) : m_bHolds( cOther.m_bHolds ) { s_nHeld += m_bHolds; }
	Holder& operator=( const Holder& cOther ) { s_nHeld += cOther.m_bHolds - m_bHolds; m_bHolds = cOther.m_bHolds; return *this; }
	~Holder() { s_nHeld -= m_bHolds; }
};
int Holder::s_nHeld = 0;

} // namespace

TEST( RingBuffer, StartsEmpty )
{
	Ring cRing( 4 );
	EXPECT_TRUE( cRing.IsEmpty() );
	EXPECT_FALSE( cRing.IsFull() );
	EXPECT_EQ( 0, cRing.Size() );
	EXPECT_EQ( 0, cRing.Count() );
	EXPECT_EQ( 4, cRing.GetCapacity() );
	EXPECT_EQ( 4, cRing.GetFree() );
	EXPECT_EQ( 0, cRing.GetDropped() );
	EXPECT_EQ( Ring::OVERWRITE, cRing.GetWhenFull() );
	EXPECT_TRUE( cRing.begin() == cRing.end() );
	int nValue = 42;
	EXPECT_FALSE( cRing.Pop( &nValue ) );
	EXPECT_FALSE( cRing.PopBack( &nValue ) );
	EXPECT_FALSE( cRing.Peek( &nValue ) );
	EXPECT_EQ( 42, nValue );
	EXPECT_EQ( 0, cRing.Read( &nValue, 1 ) );
	EXPECT_EQ( 0, cRing.Discard( 3 ) );
	EXPECT_EQ( 9, cRing.Get( 0, 9 ) );
}

TEST( RingBuffer, CapacityIsAtLeastOne )
{
	Ring cZero( 0 ), cNegative( -5 );
	EXPECT_EQ( 1, cZero.GetCapacity() );
	EXPECT_EQ( 1, cNegative.GetCapacity() );
	cZero.Push( 1 );
	cZero.Push( 2 );
	EXPECT_EQ( std::string( "2" ), Joined( cZero ) );
	EXPECT_EQ( 1, cZero.GetDropped() );
}

TEST( RingBuffer, FirstInFirstOutGoingRound )
{
	Ring cRing( 3 );
	int nValue = 0, nNext = 0;
	bool bInOrder = true;
	for( int nRound = 0; nRound < 10; nRound++ )	// goes round many times
	{
		cRing.Push( nRound * 2 );
		cRing.Push( nRound * 2 + 1 );
		for( int i = 0; i < 2; i++ )
		{
			bInOrder = bInOrder && cRing.Pop( &nValue ) && nValue == nNext;
			nNext++;
		}
	}
	EXPECT_TRUE( bInOrder );
	EXPECT_TRUE( cRing.IsEmpty() );
	EXPECT_EQ( 0, cRing.GetDropped() );
}

TEST( RingBuffer, OverwriteDropsTheOldest )
{
	Ring cLog( 3 );
	for( int i = 1; i <= 5; i++ )
		EXPECT_TRUE( cLog.Push( i ) );
	EXPECT_TRUE( cLog.IsFull() );
	EXPECT_EQ( 0, cLog.GetFree() );
	EXPECT_EQ( std::string( "3,4,5" ), Joined( cLog ) );
	EXPECT_EQ( 2, cLog.GetDropped() );
	EXPECT_EQ( 3, cLog.Peek() );
	EXPECT_EQ( 5, cLog.PeekBack() );
	cLog.ResetDropped();
	EXPECT_EQ( 0, cLog.GetDropped() );
}

TEST( RingBuffer, RejectKeepsWhatIsWaiting )
{
	Ring cEvents( 3, Ring::REJECT );
	EXPECT_TRUE( cEvents.Push( 1 ) );
	EXPECT_TRUE( cEvents.Push( 2 ) );
	EXPECT_TRUE( cEvents.Push( 3 ) );
	EXPECT_FALSE( cEvents.Push( 4 ) );
	EXPECT_FALSE( cEvents.Push( 5 ) );
	EXPECT_EQ( std::string( "1,2,3" ), Joined( cEvents ) );
	EXPECT_EQ( 2, cEvents.GetDropped() );
	EXPECT_TRUE( cEvents.Pop() );
	EXPECT_TRUE( cEvents.Push( 6 ) );
	EXPECT_EQ( std::string( "2,3,6" ), Joined( cEvents ) );

	cEvents.SetWhenFull( Ring::OVERWRITE );
	EXPECT_TRUE( cEvents.Push( 7 ) );
	EXPECT_EQ( std::string( "3,6,7" ), Joined( cEvents ) );
	EXPECT_EQ( 3, cEvents.GetDropped() );
}

TEST( RingBuffer, PopBackUndoesAPush )
{
	Ring cRing( 3 );
	cRing.Push( 1 );
	cRing.Push( 2 );
	cRing.Push( 3 );
	cRing.Push( 4 );			// round: 2, 3, 4
	int nValue = 0;
	ASSERT_TRUE( cRing.PopBack( &nValue ) );
	EXPECT_EQ( 4, nValue );
	ASSERT_TRUE( cRing.PopBack( &nValue ) );
	EXPECT_EQ( 3, nValue );
	cRing.Push( 9 );
	EXPECT_EQ( std::string( "2,9" ), Joined( cRing ) );
	EXPECT_TRUE( cRing.PopBack() );
	EXPECT_TRUE( cRing.Pop() );
	EXPECT_TRUE( cRing.IsEmpty() );
}

TEST( RingBuffer, PeekAndPositions )
{
	Ring cRing( 4 );
	for( int i = 1; i <= 6; i++ )
		cRing.Push( i * 10 );		// 30, 40, 50, 60, starting mid-array
	EXPECT_EQ( 30, cRing.At( 0 ) );
	EXPECT_EQ( 60, cRing[3] );
	EXPECT_EQ( 40, cRing.Get( 1 ) );
	EXPECT_EQ( -1, cRing.Get( 4, -1 ) );
	EXPECT_EQ( -1, cRing.Get( -1, -1 ) );
	cRing[1] = 41;
	cRing.Peek() = 31;
	cRing.PeekBack() = 61;
	cRing.At( 2 ) = 51;
	EXPECT_EQ( std::string( "31,41,51,61" ), Joined( cRing ) );
	int nValue = 0;
	ASSERT_TRUE( cRing.Peek( &nValue ) );
	EXPECT_EQ( 31, nValue );
	const Ring& cConst = cRing;
	EXPECT_EQ( 31, cConst.Peek() );
	EXPECT_EQ( 61, cConst.PeekBack() );
	EXPECT_EQ( 41, cConst[1] );
	EXPECT_EQ( 51, cConst.At( 2 ) );
}

TEST( RingBuffer, WriteAndReadInRuns )
{
	Ring cAudio( 8 );
	int anIn[12], anOut[12];
	for( int i = 0; i < 12; i++ )
		anIn[i] = i;
	EXPECT_EQ( 5, cAudio.Write( anIn, 5 ) );		// 0..4
	EXPECT_EQ( 3, cAudio.Read( anOut, 3 ) );		// 0, 1, 2
	EXPECT_EQ( 0, anOut[0] );
	EXPECT_EQ( 2, anOut[2] );
	EXPECT_EQ( 6, cAudio.Write( anIn + 5, 6 ) );	// 5..10, going round the end
	EXPECT_EQ( std::string( "3,4,5,6,7,8,9,10" ), Joined( cAudio ) );
	EXPECT_EQ( 0, cAudio.GetDropped() );
	EXPECT_EQ( 8, cAudio.Read( anOut, 12 ) );		// all there is, in two runs
	bool bInOrder = true;
	for( int i = 0; i < 8; i++ )
		bInOrder = bInOrder && anOut[i] == i + 3;
	EXPECT_TRUE( bInOrder );
	EXPECT_TRUE( cAudio.IsEmpty() );
	EXPECT_EQ( 0, cAudio.Write( anIn, 0 ) );
	EXPECT_EQ( 0, cAudio.Write( anIn, -3 ) );
	EXPECT_EQ( 0, cAudio.Read( anOut, -3 ) );
}

TEST( RingBuffer, WriteOverwritesTheOldest )
{
	Ring cRing( 5 );
	int anIn[20];
	for( int i = 0; i < 20; i++ )
		anIn[i] = i;
	cRing.Write( anIn, 4 );				// 0..3
	EXPECT_EQ( 3, cRing.Write( anIn + 4, 3 ) );		// 4..6: 0 and 1 go
	EXPECT_EQ( std::string( "2,3,4,5,6" ), Joined( cRing ) );
	EXPECT_EQ( 2, cRing.GetDropped() );
	EXPECT_EQ( 12, cRing.Write( anIn + 7, 12 ) );	// more than fit: 14..18 stay
	EXPECT_EQ( std::string( "14,15,16,17,18" ), Joined( cRing ) );
	EXPECT_EQ( 2 + 5 + 7, cRing.GetDropped() );
	EXPECT_EQ( 5, cRing.Write( anIn, 5 ) );		// exactly the capacity
	EXPECT_EQ( std::string( "0,1,2,3,4" ), Joined( cRing ) );
}

TEST( RingBuffer, WriteRejectsWhatDoesNotFit )
{
	Ring cRing( 5, Ring::REJECT );
	int anIn[10];
	for( int i = 0; i < 10; i++ )
		anIn[i] = i;
	EXPECT_EQ( 3, cRing.Write( anIn, 3 ) );
	cRing.Pop();
	EXPECT_EQ( 3, cRing.Write( anIn + 3, 7 ) );	// room for 3 of 7
	EXPECT_EQ( std::string( "1,2,3,4,5" ), Joined( cRing ) );
	EXPECT_EQ( 4, cRing.GetDropped() );
	EXPECT_EQ( 0, cRing.Write( anIn, 2 ) );
	EXPECT_EQ( 6, cRing.GetDropped() );
}

TEST( RingBuffer, Discard )
{
	Ring cRing( 4 );
	for( int i = 1; i <= 6; i++ )
		cRing.Push( i );			// 3..6
	EXPECT_EQ( 2, cRing.Discard( 2 ) );
	EXPECT_EQ( std::string( "5,6" ), Joined( cRing ) );
	EXPECT_EQ( 2, cRing.Discard( 9 ) );
	EXPECT_TRUE( cRing.IsEmpty() );
	EXPECT_EQ( 0, cRing.Discard( -1 ) );
	EXPECT_EQ( 2, cRing.GetDropped() );		// only the two overwritten
}

TEST( RingBuffer, SetCapacityKeepsTheNewest )
{
	Ring cRing( 4 );
	for( int i = 1; i <= 6; i++ )
		cRing.Push( i );			// 3..6, going round
	cRing.SetCapacity( 6 );
	EXPECT_EQ( 6, cRing.GetCapacity() );
	EXPECT_EQ( std::string( "3,4,5,6" ), Joined( cRing ) );
	cRing.Push( 7 );
	cRing.Push( 8 );
	EXPECT_TRUE( cRing.IsFull() );
	EXPECT_EQ( 2, cRing.GetDropped() );
	cRing.SetCapacity( 2 );
	EXPECT_EQ( std::string( "7,8" ), Joined( cRing ) );
	EXPECT_EQ( 6, cRing.GetDropped() );
	cRing.SetCapacity( 0 );
	EXPECT_EQ( 1, cRing.GetCapacity() );
	EXPECT_EQ( std::string( "8" ), Joined( cRing ) );
}

TEST( RingBuffer, IteratesAndCompares )
{
	Ring cRing( 4 );
	for( int i = 1; i <= 6; i++ )
		cRing.Push( i );
	std::vector<int> cBack( cRing.rbegin(), cRing.rend() );
	ASSERT_EQ( 4u, cBack.size() );
	EXPECT_EQ( 6, cBack[0] );
	EXPECT_EQ( 3, cBack[3] );
	std::vector<int> cItems = cRing.Items();
	ASSERT_EQ( 4u, cItems.size() );
	EXPECT_EQ( 3, cItems[0] );
	EXPECT_EQ( 6, cItems[3] );

	int nTotal = 0;
	cRing.ForEach( Sum( &nTotal ) );
	EXPECT_EQ( 18, nTotal );
	cRing.ForEach( Negate() );
	EXPECT_EQ( -3, cRing.Peek() );
	for( Ring::iterator i = cRing.begin(); i != cRing.end(); ++i )
		*i = -*i;
	Ring::const_iterator c = cRing.begin();		// an iterator converts
	EXPECT_EQ( 3, *c );

	Ring cOther( 10 );				// another capacity, the same items
	for( int i = 3; i <= 6; i++ )
		cOther.Push( i );
	EXPECT_TRUE( cRing == cOther );
	cOther.Push( 7 );
	EXPECT_TRUE( cRing != cOther );
	cRing.Swap( cOther );
	EXPECT_EQ( 10, cRing.GetCapacity() );
	EXPECT_EQ( 5, cRing.Count() );
	EXPECT_EQ( 4, cOther.GetCapacity() );
	EXPECT_EQ( 2, cOther.GetDropped() );		// the dropped count goes along

	Ring cCopy = cOther;
	cCopy.Push( 99 );
	EXPECT_EQ( 6, cOther.PeekBack() );
	cCopy.Clear();
	EXPECT_TRUE( cCopy.IsEmpty() );
	EXPECT_EQ( 4, cCopy.GetCapacity() );
	EXPECT_EQ( 3, cCopy.GetDropped() );
}

TEST( RingBuffer, TakenItemsLetGo )
{
	Holder::s_nHeld = 0;
	{
		RingBuffer<Holder> cRing( 4 );
		EXPECT_EQ( 0, Holder::s_nHeld );
		for( int i = 0; i < 4; i++ )
			cRing.Push( Holder( true ) );
		EXPECT_EQ( 4, Holder::s_nHeld );
		cRing.Pop();
		cRing.PopBack();
		EXPECT_EQ( 2, Holder::s_nHeld );
		cRing.Discard( 1 );
		EXPECT_EQ( 1, Holder::s_nHeld );
		Holder acOut[2];
		EXPECT_EQ( 1, cRing.Read( acOut, 2 ) );
		EXPECT_EQ( 1, Holder::s_nHeld );		// the one in acOut
		acOut[0] = Holder();
		cRing.Push( Holder( true ) );
		cRing.Push( Holder( true ) );
		cRing.Clear();
		EXPECT_EQ( 0, Holder::s_nHeld );
		cRing.Push( Holder( true ) );
		cRing.SetCapacity( 8 );
		EXPECT_EQ( 1, Holder::s_nHeld );
	}
	EXPECT_EQ( 0, Holder::s_nHeld );

	Counted::s_nLive = 0;
	{
		RingBuffer<Counted> cRing( 16 );
		EXPECT_EQ( 16, Counted::s_nLive );		// made once, up front
		for( int i = 0; i < 100; i++ )
			cRing.Push( Counted( i ) );
		EXPECT_EQ( 16, Counted::s_nLive );		// and never more
		EXPECT_EQ( 84, cRing.Peek().m_nValue );
	}
	EXPECT_EQ( 0, Counted::s_nLive );
}

TEST( RingBuffer, StringsGoingRound )
{
	RingBuffer<std::string> cLog( 3 );
	cLog.Push( "boot" );
	cLog.Push( "login" );
	cLog.Push( "mount /home" );
	cLog.Push( "start desktop" );
	EXPECT_EQ( std::string( "login" ), cLog[0] );
	EXPECT_EQ( std::string( "start desktop" ), cLog[2] );
	std::string cLine;
	cLog.Pop( &cLine );
	EXPECT_EQ( std::string( "login" ), cLine );
	EXPECT_EQ( 2, cLog.Count() );
	EXPECT_EQ( std::string( "mount /home" ), cLog.Peek() );
	EXPECT_EQ( 11u, cLog.begin()->size() );
}

/* Against a std::deque that is cut to the capacity by hand, in both
 * modes, with every operation mixed in. */
void MatchModel( Ring::WhenFull eWhenFull, unsigned nSeed )
{
	srand( nSeed );
	int nCapacity = 7;
	Ring cRing( nCapacity, eWhenFull );
	std::deque<int> cModel;
	int nDropped = 0;
	int anBuffer[32];
	for( int nStep = 0; nStep < 20000; nStep++ )
	{
		int nValue = rand();
		switch( rand() % 9 )
		{
			case 0:
			case 1:
			{
				bool bFull = int( cModel.size() ) == nCapacity;
				bool bStored = cRing.Push( nValue );
				ASSERT_EQ( !( bFull && eWhenFull == Ring::REJECT ), bStored ) << "step " << nStep;
				if( bFull )
				{
					nDropped++;
					if( eWhenFull == Ring::OVERWRITE )
						cModel.pop_front();
				}
				if( bStored )
					cModel.push_back( nValue );
				break;
			}
			case 2:
			{
				int nGot = -1;
				bool bGot = cRing.Pop( &nGot );
				ASSERT_EQ( !cModel.empty(), bGot ) << "step " << nStep;
				if( bGot )
				{
					ASSERT_EQ( cModel.front(), nGot ) << "step " << nStep;
					cModel.pop_front();
				}
				break;
			}
			case 3:
			{
				int nGot = -1;
				bool bGot = cRing.PopBack( &nGot );
				ASSERT_EQ( !cModel.empty(), bGot ) << "step " << nStep;
				if( bGot )
				{
					ASSERT_EQ( cModel.back(), nGot ) << "step " << nStep;
					cModel.pop_back();
				}
				break;
			}
			case 4:
			{
				int nCount = rand() % 20;
				for( int i = 0; i < nCount; i++ )
					anBuffer[i] = nValue + i;
				int nStored = cRing.Write( anBuffer, nCount );
				int nExpected = nCount;
				if( eWhenFull == Ring::REJECT )
					nExpected = std::min( nCount, nCapacity - int( cModel.size() ) );
				ASSERT_EQ( nExpected, nStored ) << "step " << nStep;
				for( int i = 0; i < nStored; i++ )
					cModel.push_back( anBuffer[i] );
				nDropped += nCount - nStored;
				while( int( cModel.size() ) > nCapacity )
				{
					cModel.pop_front();
					nDropped++;
				}
				break;
			}
			case 5:
			{
				int nCount = rand() % 12;
				int nGot = cRing.Read( anBuffer, nCount );
				ASSERT_EQ( std::min( nCount, int( cModel.size() ) ), nGot ) << "step " << nStep;
				for( int i = 0; i < nGot; i++ )
				{
					ASSERT_EQ( cModel.front(), anBuffer[i] ) << "step " << nStep;
					cModel.pop_front();
				}
				break;
			}
			case 6:
			{
				int nCount = rand() % 4;
				int nGone = cRing.Discard( nCount );
				ASSERT_EQ( std::min( nCount, int( cModel.size() ) ), nGone ) << "step " << nStep;
				cModel.erase( cModel.begin(), cModel.begin() + nGone );
				break;
			}
			case 7:
				if( rand() % 40 == 0 )
				{
					nCapacity = 1 + rand() % 12;
					cRing.SetCapacity( nCapacity );
					while( int( cModel.size() ) > nCapacity )
					{
						cModel.pop_front();
						nDropped++;
					}
				}
				break;
			default:
				if( !cModel.empty() )
				{
					int nPos = rand() % int( cModel.size() );
					ASSERT_EQ( cModel[nPos], cRing[nPos] ) << "step " << nStep;
					ASSERT_EQ( cModel.front(), cRing.Peek() ) << "step " << nStep;
					ASSERT_EQ( cModel.back(), cRing.PeekBack() ) << "step " << nStep;
				}
		}
		ASSERT_EQ( int( cModel.size() ), cRing.Count() ) << "step " << nStep;
		ASSERT_EQ( nDropped, cRing.GetDropped() ) << "step " << nStep;
		ASSERT_EQ( nCapacity - int( cModel.size() ), cRing.GetFree() ) << "step " << nStep;
	}
	std::vector<int> cItems = cRing.Items();
	EXPECT_TRUE( std::equal( cModel.begin(), cModel.end(), cItems.begin() ) && cModel.size() == cItems.size() );
}

TEST( RingBuffer, OverwriteMatchesDequeOverRandomSteps )
{
	MatchModel( Ring::OVERWRITE, 1001 );
}

TEST( RingBuffer, RejectMatchesDequeOverRandomSteps )
{
	MatchModel( Ring::REJECT, 2002 );
}

#if __cplusplus >= 201103L
TEST( RingBuffer, RangeFor )
{
	Ring cRing( 3 );
	for( int i : { 1, 2, 3, 4 } )
		cRing.Push( i );
	int nSum = 0;
	for( int n : cRing )
		nSum += n;
	for( int& n : cRing )
		n *= 10;
	EXPECT_EQ( 9, nSum );
	EXPECT_EQ( 20, cRing.Peek() );
}
#endif
