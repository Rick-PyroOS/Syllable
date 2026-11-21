/*  os::Deque: push and pop at both ends, peeking, positions, inserting
 *  and removing in the middle, rotating, and a random run against
 *  std::deque.
 */

#include <gtest/gtest.h>

#include <util/deque.h>

#include <deque>
#include <string>
#include <vector>
#include <cstdlib>
#include <stdio.h>

using namespace os;

namespace
{

std::string Joined( const Deque<int>& cDeque )
{
	std::string cText;
	char zNumber[16];
	for( Deque<int>::const_iterator i = cDeque.begin(); i != cDeque.end(); ++i )
	{
		snprintf( zNumber, sizeof( zNumber ), "%s%d", i == cDeque.begin() ? "" : ",", *i );
		cText += zNumber;
	}
	return cText;
}

Deque<int> Range( int nFrom, int nTo )
{
	Deque<int> cDeque;
	for( int i = nFrom; i <= nTo; i++ )
		cDeque.Push( i );
	return cDeque;
}

bool IsOdd( int n )
{
	return n % 2 != 0;
}

struct Double
{
	void operator()( int& n ) const { n *= 2; }
};

struct Sum
{
	explicit Sum( int* pnTotal ) : m_pnTotal( pnTotal ) {}
	void operator()( int n ) const { *m_pnTotal += n; }
	int* m_pnTotal;
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

} // namespace

TEST( Deque, StartsEmpty )
{
	Deque<int> cDeque;
	EXPECT_TRUE( cDeque.IsEmpty() );
	EXPECT_EQ( 0, cDeque.Size() );
	EXPECT_EQ( 0, cDeque.Count() );
	EXPECT_TRUE( cDeque.begin() == cDeque.end() );
	int nValue = 42;
	EXPECT_FALSE( cDeque.Pop( &nValue ) );
	EXPECT_FALSE( cDeque.PopBack( &nValue ) );
	EXPECT_FALSE( cDeque.PopFront( &nValue ) );
	EXPECT_FALSE( cDeque.First( &nValue ) );
	EXPECT_FALSE( cDeque.Last( &nValue ) );
	EXPECT_FALSE( cDeque.Pop() );
	EXPECT_FALSE( cDeque.PopFront() );
	EXPECT_EQ( 42, nValue );
	EXPECT_EQ( 7, cDeque.Get( 0, 7 ) );
	EXPECT_FALSE( cDeque.RemoveAt( 0 ) );
}

TEST( Deque, PushAndPopAtBothEnds )
{
	Deque<int> cDeque;
	cDeque.Push( 2 ).Push( 3 ).PushFront( 1 ).PushBack( 4 ).PushFront( 0 );
	EXPECT_EQ( std::string( "0,1,2,3,4" ), Joined( cDeque ) );
	EXPECT_EQ( 5, cDeque.Size() );

	int nValue = -1;
	ASSERT_TRUE( cDeque.Pop( &nValue ) );
	EXPECT_EQ( 4, nValue );
	ASSERT_TRUE( cDeque.PopFront( &nValue ) );
	EXPECT_EQ( 0, nValue );
	ASSERT_TRUE( cDeque.PopBack( &nValue ) );
	EXPECT_EQ( 3, nValue );
	EXPECT_TRUE( cDeque.PopFront() );
	EXPECT_TRUE( cDeque.Pop() );
	EXPECT_TRUE( cDeque.IsEmpty() );
}

TEST( Deque, PeekFirstLast )
{
	Deque<std::string> cHistory;
	cHistory.Push( "/home" ).Push( "/home/rick" ).PushFront( "/" );
	EXPECT_EQ( std::string( "/" ), cHistory.PeekFront() );
	EXPECT_EQ( std::string( "/home/rick" ), cHistory.PeekBack() );
	std::string cPath;
	ASSERT_TRUE( cHistory.First( &cPath ) );
	EXPECT_EQ( std::string( "/" ), cPath );
	ASSERT_TRUE( cHistory.Last( &cPath ) );
	EXPECT_EQ( std::string( "/home/rick" ), cPath );
	EXPECT_EQ( 3, cHistory.Size() );

	cHistory.PeekFront() = "/root";
	cHistory.PeekBack() = "/tmp";
	const Deque<std::string>& cConst = cHistory;
	EXPECT_EQ( std::string( "/root" ), cConst.PeekFront() );
	EXPECT_EQ( std::string( "/tmp" ), cConst.PeekBack() );
}

TEST( Deque, Positions )
{
	Deque<int> cDeque = Range( 10, 14 );
	EXPECT_EQ( 10, cDeque.At( 0 ) );
	EXPECT_EQ( 14, cDeque[4] );
	cDeque[2] = 99;
	cDeque.At( 3 ) = 98;
	EXPECT_EQ( std::string( "10,11,99,98,14" ), Joined( cDeque ) );
	EXPECT_EQ( 99, cDeque.Get( 2 ) );
	EXPECT_EQ( -1, cDeque.Get( 5, -1 ) );
	EXPECT_EQ( -1, cDeque.Get( -1, -1 ) );
	EXPECT_EQ( 0, cDeque.Get( 9 ) );
	const Deque<int>& cConst = cDeque;
	EXPECT_EQ( 11, cConst.At( 1 ) );
	EXPECT_EQ( 11, cConst[1] );
}

TEST( Deque, InsertAndRemoveInTheMiddle )
{
	Deque<int> cDeque = Range( 1, 4 );
	cDeque.InsertAt( 2, 9 );
	EXPECT_EQ( std::string( "1,2,9,3,4" ), Joined( cDeque ) );
	cDeque.InsertAt( 0, 0 ).InsertAt( -5, -1 ).InsertAt( 99, 5 );
	EXPECT_EQ( std::string( "-1,0,1,2,9,3,4,5" ), Joined( cDeque ) );

	int nValue = 0;
	ASSERT_TRUE( cDeque.RemoveAt( 4, &nValue ) );
	EXPECT_EQ( 9, nValue );
	EXPECT_TRUE( cDeque.RemoveAt( 0 ) );
	EXPECT_FALSE( cDeque.RemoveAt( 6 ) );
	EXPECT_FALSE( cDeque.RemoveAt( -1 ) );
	EXPECT_EQ( std::string( "0,1,2,3,4,5" ), Joined( cDeque ) );

	Deque<int>::iterator i = cDeque.Erase( cDeque.begin() + 1 );
	EXPECT_EQ( 2, *i );
	EXPECT_EQ( std::string( "0,2,3,4,5" ), Joined( cDeque ) );
}

TEST( Deque, FindAndRemoveByValue )
{
	Deque<int> cDeque;
	cDeque.Push( 5 ).Push( 7 ).Push( 5 ).Push( 8 );
	EXPECT_EQ( 0, cDeque.IndexOf( 5 ) );
	EXPECT_EQ( 3, cDeque.IndexOf( 8 ) );
	EXPECT_EQ( -1, cDeque.IndexOf( 6 ) );
	EXPECT_TRUE( cDeque.Has( 7 ) );
	EXPECT_FALSE( cDeque.Has( 6 ) );
	EXPECT_TRUE( cDeque.Remove( 5 ) );		// the first one
	EXPECT_EQ( std::string( "7,5,8" ), Joined( cDeque ) );
	EXPECT_FALSE( cDeque.Remove( 6 ) );
	EXPECT_EQ( 3, cDeque.Size() );
}

TEST( Deque, RemoveIfAndForEach )
{
	Deque<int> cDeque = Range( 1, 9 );
	EXPECT_EQ( 5, cDeque.RemoveIf( IsOdd ) );
	EXPECT_EQ( std::string( "2,4,6,8" ), Joined( cDeque ) );
	EXPECT_EQ( 0, cDeque.RemoveIf( IsOdd ) );
	int nTotal = 0;
	cDeque.ForEach( Sum( &nTotal ) );
	EXPECT_EQ( 20, nTotal );
	cDeque.ForEach( Double() );
	EXPECT_EQ( std::string( "4,8,12,16" ), Joined( cDeque ) );
}

TEST( Deque, RotateAndReverse )
{
	Deque<int> cDeque = Range( 1, 5 );
	cDeque.Rotate( 2 );
	EXPECT_EQ( std::string( "3,4,5,1,2" ), Joined( cDeque ) );
	cDeque.Rotate( -2 );
	EXPECT_EQ( std::string( "1,2,3,4,5" ), Joined( cDeque ) );
	cDeque.Rotate( -1 );
	EXPECT_EQ( std::string( "5,1,2,3,4" ), Joined( cDeque ) );
	cDeque.Rotate( 11 );				// 11 is 1 for five items
	EXPECT_EQ( std::string( "1,2,3,4,5" ), Joined( cDeque ) );
	cDeque.Rotate( -7 );
	EXPECT_EQ( std::string( "4,5,1,2,3" ), Joined( cDeque ) );
	cDeque.Rotate( 0 );
	EXPECT_EQ( std::string( "4,5,1,2,3" ), Joined( cDeque ) );
	cDeque.Reverse();
	EXPECT_EQ( std::string( "3,2,1,5,4" ), Joined( cDeque ) );

	Deque<int> cOne = Range( 1, 1 ), cNone;
	cOne.Rotate( 3 );
	cNone.Rotate( 3 );
	EXPECT_EQ( std::string( "1" ), Joined( cOne ) );
	EXPECT_TRUE( cNone.IsEmpty() );
}

TEST( Deque, IteratesBothWaysAndCopies )
{
	Deque<int> cDeque = Range( 1, 3 );
	std::vector<int> cBack( cDeque.rbegin(), cDeque.rend() );
	ASSERT_EQ( 3u, cBack.size() );
	EXPECT_EQ( 3, cBack[0] );
	EXPECT_EQ( 1, cBack[2] );
	std::vector<int> cItems = cDeque.Items();
	ASSERT_EQ( 3u, cItems.size() );
	EXPECT_EQ( 2, cItems[1] );

	Deque<int> cCopy( cItems.begin(), cItems.end() );
	EXPECT_TRUE( cCopy == cDeque );
	cCopy.PushFront( 0 );
	EXPECT_TRUE( cCopy != cDeque );
	EXPECT_EQ( 3, cDeque.Size() );

	cCopy.Swap( cDeque );
	EXPECT_EQ( 4, cDeque.Size() );
	EXPECT_EQ( 3, cCopy.Size() );
	cDeque.Clear();
	EXPECT_TRUE( cDeque.IsEmpty() );
	EXPECT_EQ( 3u, cCopy.GetContainer().size() );
}

TEST( Deque, PopDestroysWhatItTakes )
{
	Counted::s_nLive = 0;
	{
		Deque<Counted> cDeque;
		for( int i = 0; i < 100; i++ )
		{
			if( i % 2 )
				cDeque.Push( Counted( i ) );
			else
				cDeque.PushFront( Counted( i ) );
		}
		EXPECT_EQ( 100, Counted::s_nLive );
		for( int i = 0; i < 20; i++ )
		{
			cDeque.Pop();
			cDeque.PopFront();
		}
		EXPECT_EQ( 60, Counted::s_nLive );
		cDeque.RemoveAt( 10 );
		EXPECT_EQ( 59, Counted::s_nLive );
		cDeque.Clear();
		EXPECT_EQ( 0, Counted::s_nLive );
		cDeque.Push( Counted( 1 ) );
	}
	EXPECT_EQ( 0, Counted::s_nLive );
}

TEST( Deque, ManyItemsAtBothEnds )
{
	Deque<int> cDeque;
	for( int i = 0; i < 50000; i++ )
		cDeque.Push( i ).PushFront( -1 - i );
	EXPECT_EQ( 100000, cDeque.Size() );
	EXPECT_EQ( -50000, cDeque.PeekFront() );
	EXPECT_EQ( 49999, cDeque.PeekBack() );
	bool bInOrder = true;
	for( int i = 0; i < 100000; i++ )
		bInOrder = bInOrder && cDeque[i] == i - 50000;
	EXPECT_TRUE( bInOrder );
}

TEST( Deque, MatchesDequeOverRandomSteps )
{
	srand( 31337 );
	Deque<int> cDeque;
	std::deque<int> cModel;
	for( int nStep = 0; nStep < 20000; nStep++ )
	{
		int nValue = rand() % 1000;
		int nGot = -1;
		switch( rand() % 8 )
		{
			case 0:
				cDeque.Push( nValue );
				cModel.push_back( nValue );
				break;
			case 1:
				cDeque.PushFront( nValue );
				cModel.push_front( nValue );
				break;
			case 2:
			{
				bool bGot = cDeque.Pop( &nGot );
				ASSERT_EQ( !cModel.empty(), bGot ) << "step " << nStep;
				if( bGot )
				{
					ASSERT_EQ( cModel.back(), nGot ) << "step " << nStep;
					cModel.pop_back();
				}
				break;
			}
			case 3:
			{
				bool bGot = cDeque.PopFront( &nGot );
				ASSERT_EQ( !cModel.empty(), bGot ) << "step " << nStep;
				if( bGot )
				{
					ASSERT_EQ( cModel.front(), nGot ) << "step " << nStep;
					cModel.pop_front();
				}
				break;
			}
			case 4:
			{
				int nPos = int( cModel.size() ) > 0 ? rand() % ( int( cModel.size() ) + 1 ) : 0;
				cDeque.InsertAt( nPos, nValue );
				cModel.insert( cModel.begin() + nPos, nValue );
				break;
			}
			case 5:
				if( !cModel.empty() )
				{
					int nPos = rand() % int( cModel.size() );
					ASSERT_TRUE( cDeque.RemoveAt( nPos, &nGot ) ) << "step " << nStep;
					ASSERT_EQ( cModel[nPos], nGot ) << "step " << nStep;
					cModel.erase( cModel.begin() + nPos );
				}
				break;
			case 6:
				if( rand() % 50 == 0 && !cModel.empty() )
				{
					int nSteps = rand() % 21 - 10;
					cDeque.Rotate( nSteps );
					int nSize = int( cModel.size() ), nFirst = ( ( nSteps % nSize ) + nSize ) % nSize;
					std::rotate( cModel.begin(), cModel.begin() + nFirst, cModel.end() );
				}
				break;
			default:
				if( !cModel.empty() )
				{
					int nPos = rand() % int( cModel.size() );
					ASSERT_EQ( cModel[nPos], cDeque[nPos] ) << "step " << nStep;
					ASSERT_EQ( cModel.front(), cDeque.PeekFront() ) << "step " << nStep;
					ASSERT_EQ( cModel.back(), cDeque.PeekBack() ) << "step " << nStep;
				}
		}
		ASSERT_EQ( int( cModel.size() ), cDeque.Size() ) << "step " << nStep;
	}
	EXPECT_TRUE( cModel == cDeque.GetContainer() );
}

#if __cplusplus >= 201103L
TEST( Deque, RangeFor )
{
	Deque<int> cDeque = Range( 1, 4 );
	int nSum = 0;
	for( int n : cDeque )
		nSum += n;
	for( int& n : cDeque )
		n = -n;
	EXPECT_EQ( 10, nSum );
	EXPECT_EQ( -1, cDeque.PeekFront() );
}
#endif
