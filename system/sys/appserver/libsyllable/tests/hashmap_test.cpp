/*  os::HashMap: insert, update, delete, push and pop at both ends,
 *  positions, reversing, sorting, iterating, growth and compaction, and a
 *  random run checked against a plain list.
 */

#include <gtest/gtest.h>

#include <util/hashmap.h>
#include <util/string.h>

#include <string>
#include <stdio.h>
#include <vector>
#include <utility>

using namespace os;

namespace
{

typedef HashMap<String, int> Scores;

Scores BuildScores()
{
	Scores cScores;
	cScores.Set( "kurt", 3 ).Set( "anna", 9 ).Set( "rick", 5 ).Set( "dee", 7 );
	return cScores;
}

template<class M> std::string Joined( const M& cMap )
{
	std::string cText;
	for( typename M::const_iterator i = cMap.begin(); i != cMap.end(); ++i )
		cText += ( cText.empty() ? "" : "," ) + std::string( i->first.c_str() );
	return cText;
}

bool ByLength( const String& cA, const String& cB )
{
	return cA.size() < cB.size();
}

bool Greater( const int& nA, const int& nB )
{
	return nA > nB;
}

struct Below
{
	explicit Below( int nLimit ) : m_nLimit( nLimit ) {}
	bool operator()( const String&, int nValue ) const { return nValue < m_nLimit; }
	int m_nLimit;
};

struct Sum
{
	explicit Sum( int* pnTotal ) : m_pnTotal( pnTotal ) {}
	void operator()( const String&, int nValue ) const { *m_pnTotal += nValue; }
	int* m_pnTotal;
};

/* Every key hashes the same: each lookup walks the whole probe chain */
struct BadHash
{
	size_t operator()( int ) const { return 7; }
};

/* Counts its live copies, so a test can see how many entries and holes a
 * HashMap keeps (a hole holds a V()) */
int g_nLiveValues = 0;
struct LiveValue
{
	LiveValue() { g_nLiveValues++; }
	LiveValue( const LiveValue& ) { g_nLiveValues++; }
	~LiveValue() { g_nLiveValues--; }
	LiveValue& operator=( const LiveValue& ) { return *this; }
	bool operator==( const LiveValue& ) const { return true; }
};

} // namespace

TEST( HashMapTest, InsertUpdateAndSet )
{
	Scores cScores = BuildScores();
	EXPECT_EQ( 4, cScores.Count() );
	EXPECT_FALSE( cScores.Insert( "anna", 1 ) );
	EXPECT_EQ( 9, cScores.Get( "anna" ) );
	EXPECT_TRUE( cScores.Insert( "zoe", 1 ) );
	EXPECT_TRUE( cScores.Update( "anna", 10 ) );
	EXPECT_FALSE( cScores.Update( "nobody", 1 ) );
	EXPECT_FALSE( cScores.Has( "nobody" ) );
	EXPECT_EQ( -1, cScores.Get( "nobody", -1 ) );
	cScores.Set( "kurt", 4 );				// update: kurt stays first
	EXPECT_EQ( "kurt,anna,rick,dee,zoe", Joined( cScores ) );
	EXPECT_EQ( 4, cScores.Get( "kurt" ) );
	*cScores.Find( "rick" ) += 1;
	EXPECT_EQ( 6, cScores.Get( "rick" ) );
	cScores["new"] += 2;					// added at the end with 0
	EXPECT_EQ( 2, cScores.Get( "new" ) );
	EXPECT_EQ( "kurt,anna,rick,dee,zoe,new", Joined( cScores ) );
}

TEST( HashMapTest, PushMovesToTheEnd )
{
	Scores cScores = BuildScores();
	cScores.Push( "kurt", 7 );
	EXPECT_EQ( "anna,rick,dee,kurt", Joined( cScores ) );
	EXPECT_EQ( 7, cScores.Get( "kurt" ) );
	cScores.Push( "kurt", 8 );				// last already
	EXPECT_EQ( "anna,rick,dee,kurt", Joined( cScores ) );
	cScores.PushFront( "zoe", 1 );
	cScores.PushFront( "dee", 2 );
	EXPECT_EQ( "dee,zoe,anna,rick,kurt", Joined( cScores ) );
	EXPECT_EQ( 2, cScores.Get( "dee" ) );
	EXPECT_EQ( 5, cScores.Count() );
}

TEST( HashMapTest, PopAndPopFront )
{
	Scores cScores = BuildScores();
	String cName;
	int nScore = 0;
	EXPECT_TRUE( cScores.First( &cName, &nScore ) );
	EXPECT_STREQ( "kurt", cName.c_str() );
	EXPECT_TRUE( cScores.Last( &cName ) );
	EXPECT_STREQ( "dee", cName.c_str() );
	EXPECT_TRUE( cScores.Pop( &cName, &nScore ) );
	EXPECT_STREQ( "dee", cName.c_str() );
	EXPECT_EQ( 7, nScore );
	EXPECT_TRUE( cScores.PopFront( &cName, &nScore ) );
	EXPECT_STREQ( "kurt", cName.c_str() );
	EXPECT_EQ( "anna,rick", Joined( cScores ) );
	EXPECT_FALSE( cScores.Has( "kurt" ) );
	EXPECT_TRUE( cScores.Has( "anna" ) );
	EXPECT_TRUE( cScores.Pop() );
	EXPECT_TRUE( cScores.PopFront() );
	EXPECT_FALSE( cScores.Pop() );
	EXPECT_FALSE( cScores.PopFront( &cName ) );
	EXPECT_TRUE( cScores.IsEmpty() );
	EXPECT_TRUE( cScores.begin() == cScores.end() );
	cScores.Set( "again", 1 );				// usable after emptying
	EXPECT_EQ( 1, cScores.Get( "again" ) );
}

TEST( HashMapTest, QueueThroughPushAndPopFront )
{
	HashMap<int, int> cQueue;
	int nNext = 0;
	for( int nRound = 0; nRound < 2000; nRound++ )
	{
		cQueue.Push( nRound, nRound * 2 );
		if( nRound % 3 != 0 )
		{
			int nKey = -1, nValue = -1;
			ASSERT_TRUE( cQueue.PopFront( &nKey, &nValue ) );
			ASSERT_EQ( nNext, nKey );
			ASSERT_EQ( nNext * 2, nValue );
			nNext++;
		}
	}
	EXPECT_EQ( 2000 - nNext, cQueue.Count() );
	EXPECT_EQ( nNext, cQueue.KeyAt( 0 ) );
	EXPECT_LE( cQueue.GetCapacity(), 2048u );		// the holes were compacted away
}

TEST( HashMapTest, RemoveKeepsTheOrder )
{
	Scores cScores = BuildScores();
	EXPECT_TRUE( cScores.Remove( "anna" ) );
	EXPECT_FALSE( cScores.Remove( "anna" ) );
	int nScore = 0;
	EXPECT_TRUE( cScores.Take( "rick", &nScore ) );
	EXPECT_EQ( 5, nScore );
	EXPECT_EQ( "kurt,dee", Joined( cScores ) );
	cScores.Set( "anna", 1 );				// back in, at the end
	EXPECT_EQ( "kurt,dee,anna", Joined( cScores ) );
}

TEST( HashMapTest, PositionsInsertAtAndIndexOf )
{
	Scores cScores = BuildScores();
	EXPECT_STREQ( "anna", cScores.KeyAt( 1 ).c_str() );
	EXPECT_EQ( 5, cScores.ValueAt( 2 ) );
	EXPECT_EQ( 3, cScores.IndexOf( "dee" ) );
	EXPECT_EQ( -1, cScores.IndexOf( "nobody" ) );
	cScores.Remove( "anna" );				// a hole: positions still count entries
	EXPECT_STREQ( "rick", cScores.KeyAt( 1 ).c_str() );
	EXPECT_EQ( 2, cScores.IndexOf( "dee" ) );
	cScores.ValueAt( 0 ) = 30;
	EXPECT_EQ( 30, cScores.Get( "kurt" ) );
	cScores.InsertAt( 1, "bob", 2 );
	EXPECT_EQ( "kurt,bob,rick,dee", Joined( cScores ) );
	cScores.InsertAt( 99, "kurt", 31 );			// moves kurt to the end
	EXPECT_EQ( "bob,rick,dee,kurt", Joined( cScores ) );
	cScores.InsertAt( -5, "zed", 0 );
	EXPECT_EQ( "zed,bob,rick,dee,kurt", Joined( cScores ) );
	EXPECT_EQ( 4, cScores.IndexOf( "kurt" ) );
}

TEST( HashMapTest, Reverse )
{
	Scores cScores = BuildScores();
	cScores.Remove( "anna" );
	cScores.Reverse();
	EXPECT_EQ( "dee,rick,kurt", Joined( cScores ) );
	EXPECT_EQ( 3, cScores.Get( "kurt" ) );			// lookups still work
	cScores.Set( "x", 1 );
	cScores.Reverse();
	EXPECT_EQ( "x,kurt,rick,dee", Joined( cScores ) );
	std::string cBack;
	for( Scores::const_reverse_iterator i = cScores.rbegin(); i != cScores.rend(); ++i )
		cBack += i->first.c_str();
	EXPECT_EQ( "deerickkurtx", cBack );
}

TEST( HashMapTest, SortByKeyAndValue )
{
	Scores cScores = BuildScores();
	cScores.Sort();
	EXPECT_EQ( "anna,dee,kurt,rick", Joined( cScores ) );
	cScores.Sort( NULL, true );
	EXPECT_EQ( "rick,kurt,dee,anna", Joined( cScores ) );
	cScores.Sort( ByLength );				// dee, then the 4s in some order
	EXPECT_STREQ( "dee", cScores.KeyAt( 0 ).c_str() );
	cScores.SortByValue();
	EXPECT_EQ( "kurt,rick,dee,anna", Joined( cScores ) );	// 3 5 7 9
	cScores.SortByValue( NULL, true );
	EXPECT_EQ( "anna,dee,rick,kurt", Joined( cScores ) );
	cScores.SortByValue( Greater );
	EXPECT_EQ( "anna,dee,rick,kurt", Joined( cScores ) );
	EXPECT_EQ( 9, cScores.Get( "anna" ) );
	cScores.Set( "bob", 5 ).Set( "amy", 5 );		// ties keep their order
	cScores.SortByValue();
	EXPECT_EQ( "kurt,rick,bob,amy,dee,anna", Joined( cScores ) );
}

TEST( HashMapTest, EraseWhileIterating )
{
	HashMap<int, int> cMap;
	for( int i = 0; i < 100; i++ )
		cMap.Set( i, i );
	for( HashMap<int, int>::iterator i = cMap.begin(); i != cMap.end(); )
	{
		if( i->first % 3 == 0 )
			i = cMap.Erase( i );
		else
			++i;
	}
	EXPECT_EQ( 66, cMap.Count() );
	EXPECT_FALSE( cMap.Has( 99 ) );
	EXPECT_EQ( 1, cMap.KeyAt( 0 ) );
	EXPECT_EQ( 98, cMap.KeyAt( 65 ) );
	int nLast = 0;
	cMap.Last( &nLast );
	EXPECT_EQ( 98, nLast );
}

TEST( HashMapTest, RemoveIfForEachItems )
{
	Scores cScores = BuildScores();
	int nTotal = 0;
	cScores.ForEach( Sum( &nTotal ) );
	EXPECT_EQ( 24, nTotal );
	EXPECT_EQ( 2, cScores.RemoveIf( Below( 6 ) ) );
	EXPECT_EQ( "anna,dee", Joined( cScores ) );
	std::vector<Scores::Item> cItems = cScores.Items();
	ASSERT_EQ( 2u, cItems.size() );
	EXPECT_STREQ( "dee", cItems[1].first.c_str() );
	EXPECT_EQ( 7, cItems[1].second );
	EXPECT_EQ( 2u, cScores.Keys().size() );
	EXPECT_EQ( 9, cScores.Values()[0] );
}

TEST( HashMapTest, GrowsAndFindsEverything )
{
	HashMap<String, int> cMap;
	for( int i = 0; i < 5000; i++ )
	{
		char zKey[16];
		snprintf( zKey, sizeof( zKey ), "k%d", i );
		cMap.Set( zKey, i );
	}
	EXPECT_EQ( 5000, cMap.Count() );
	EXPECT_EQ( 8192u, cMap.GetCapacity() );			// a power of two, a third free
	for( int i = 0; i < 5000; i += 7 )
	{
		char zKey[16];
		snprintf( zKey, sizeof( zKey ), "k%d", i );
		ASSERT_EQ( i, cMap.Get( zKey, -1 ) ) << zKey;
	}
	EXPECT_STREQ( "k4999", cMap.KeyAt( 4999 ).c_str() );
}

TEST( HashMapTest, ReserveAndClear )
{
	HashMap<int, int> cMap;
	cMap.Reserve( 1000 );
	size_t nCapacity = cMap.GetCapacity();
	EXPECT_GE( nCapacity * 2, 3000u );
	for( int i = 0; i < 1000; i++ )
		cMap.Set( i, i );
	EXPECT_EQ( nCapacity, cMap.GetCapacity() );		// no regrowth
	cMap.Reserve( 10 );					// never shrinks below the entries
	EXPECT_EQ( 999, cMap.Get( 999 ) );
	cMap.Clear();
	EXPECT_TRUE( cMap.IsEmpty() );
	EXPECT_FALSE( cMap.Has( 1 ) );
}

TEST( HashMapTest, CollidingHashes )
{
	HashMap<int, int, BadHash> cMap;
	for( int i = 0; i < 200; i++ )
		cMap.Set( i, -i );
	for( int i = 0; i < 200; i += 2 )
		cMap.Remove( i );
	for( int i = 1; i < 200; i += 2 )
		ASSERT_EQ( -i, cMap.Get( i, 1 ) );
	EXPECT_FALSE( cMap.Has( 100 ) );
	cMap.Set( 100, 5 );
	EXPECT_EQ( 5, cMap.Get( 100 ) );
	EXPECT_EQ( 101, cMap.Count() );
}

TEST( HashMapTest, MergeCompareCopyAndSwap )
{
	Scores cA = BuildScores();
	Scores cB;
	cB.Set( "anna", 1 ).Set( "new", 2 );
	Scores cKeep = cA;
	cKeep.Merge( cB, false );
	EXPECT_EQ( 9, cKeep.Get( "anna" ) );
	EXPECT_EQ( "kurt,anna,rick,dee,new", Joined( cKeep ) );
	cA.Merge( cB );
	EXPECT_EQ( 1, cA.Get( "anna" ) );
	Scores cCopy = cA;
	EXPECT_TRUE( cCopy == cA );
	cCopy.Reverse();
	EXPECT_TRUE( cCopy == cA );				// the order doesn't matter
	cCopy.Remove( "new" );
	EXPECT_TRUE( cCopy != cA );
	cA.Set( "x", 1 );
	EXPECT_FALSE( cCopy.Has( "x" ) );			// a copy is independent
	cCopy.Swap( cB );
	EXPECT_EQ( 2, cCopy.Count() );
	EXPECT_STREQ( "dee", cB.KeyAt( 0 ).c_str() );
}

TEST( HashMapTest, AppendAndRemoveAtForVectorValues )
{
	HashMap<String, std::vector<int> > cByUser;
	cByUser.Append( "rick", 1 ).Append( "anna", 2 ).Append( "rick", 3 ).Append( "rick", 4 );
	EXPECT_EQ( "rick,anna", Joined( cByUser ) );		// a new key goes at the end
	ASSERT_EQ( 3u, cByUser.Find( "rick" )->size() );
	EXPECT_TRUE( cByUser.RemoveAt( "rick", 1 ) );		// the 3
	EXPECT_EQ( 1, ( *cByUser.Find( "rick" ) )[0] );
	EXPECT_EQ( 4, ( *cByUser.Find( "rick" ) )[1] );
	EXPECT_FALSE( cByUser.RemoveAt( "rick", 2 ) );
	EXPECT_FALSE( cByUser.RemoveAt( "nobody", 0 ) );
	EXPECT_TRUE( cByUser.RemoveAt( "anna", 0, true ) );	// the key goes with its last item
	EXPECT_FALSE( cByUser.Has( "anna" ) );
	EXPECT_TRUE( cByUser.RemoveAt( "rick", 0 ) );
	EXPECT_TRUE( cByUser.RemoveAt( "rick", 0 ) );
	EXPECT_TRUE( cByUser.Has( "rick" ) );			// kept, empty
	EXPECT_TRUE( cByUser.Find( "rick" )->empty() );
	cByUser.Append( "zoe", 9 );
	EXPECT_EQ( "rick,zoe", Joined( cByUser ) );
}

/* Random operations on a HashMap and on a plain list of pairs that does
 * the same, slowly; they must agree after each one */
TEST( HashMapTest, RandomRunMatchesAList )
{
	typedef std::vector<std::pair<int, int> > List;
	HashMap<int, int> cMap;
	List cList;
	uint32 nSeed = 12345;
	for( int nStep = 0; nStep < 20000; nStep++ )
	{
		nSeed = nSeed * 1103515245U + 12345U;
		const int nOp = int( ( nSeed >> 16 ) % 11 );
		nSeed = nSeed * 1103515245U + 12345U;
		const int nKey = int( ( nSeed >> 16 ) % 64 );
		const int nValue = nStep;
		int nAt = -1;
		for( size_t i = 0; i < cList.size(); i++ )
			if( cList[i].first == nKey )
				nAt = int( i );
		switch( nOp )
		{
			case 0: case 1:		// Set
				cMap.Set( nKey, nValue );
				if( nAt >= 0 )
					cList[nAt].second = nValue;
				else
					cList.push_back( std::make_pair( nKey, nValue ) );
				break;
			case 2:			// Push
				cMap.Push( nKey, nValue );
				if( nAt >= 0 )
					cList.erase( cList.begin() + nAt );
				cList.push_back( std::make_pair( nKey, nValue ) );
				break;
			case 3:			// PushFront
				cMap.PushFront( nKey, nValue );
				if( nAt >= 0 )
					cList.erase( cList.begin() + nAt );
				cList.insert( cList.begin(), std::make_pair( nKey, nValue ) );
				break;
			case 4: case 5:		// Remove
				ASSERT_EQ( nAt >= 0, cMap.Remove( nKey ) );
				if( nAt >= 0 )
					cList.erase( cList.begin() + nAt );
				break;
			case 6:			// Pop
			{
				int nK = -1;
				ASSERT_EQ( !cList.empty(), cMap.Pop( &nK ) );
				if( !cList.empty() )
				{
					ASSERT_EQ( cList.back().first, nK );
					cList.pop_back();
				}
				break;
			}
			case 7:			// PopFront
			{
				int nK = -1;
				ASSERT_EQ( !cList.empty(), cMap.PopFront( &nK ) );
				if( !cList.empty() )
				{
					ASSERT_EQ( cList.front().first, nK );
					cList.erase( cList.begin() );
				}
				break;
			}
			case 8:			// Insert / Update
				if( nAt >= 0 )
				{
					ASSERT_FALSE( cMap.Insert( nKey, nValue ) );
					ASSERT_TRUE( cMap.Update( nKey, nValue ) );
					cList[nAt].second = nValue;
				}
				else
				{
					ASSERT_FALSE( cMap.Update( nKey, nValue ) );
					ASSERT_TRUE( cMap.Insert( nKey, nValue ) );
					cList.push_back( std::make_pair( nKey, nValue ) );
				}
				break;
			case 9:			// InsertAt
			{
				if( nAt >= 0 )
					cList.erase( cList.begin() + nAt );
				const int nPos = cList.empty() ? 0 : nValue % int( cList.size() + 1 );
				cMap.InsertAt( nPos, nKey, nValue );
				cList.insert( cList.begin() + nPos, std::make_pair( nKey, nValue ) );
				break;
			}
			case 10:		// Reverse, now and then
				if( nStep % 50 == 0 )
				{
					cMap.Reverse();
					std::reverse( cList.begin(), cList.end() );
				}
				break;
		}
		ASSERT_EQ( int( cList.size() ), cMap.Count() ) << "step " << nStep;
		if( nStep % 97 == 0 || nStep > 19900 )
		{
			size_t i = 0;
			for( HashMap<int, int>::const_iterator c = cMap.begin(); c != cMap.end(); ++c, ++i )
			{
				ASSERT_LT( i, cList.size() );
				ASSERT_EQ( cList[i].first, c->first ) << "step " << nStep << " at " << i;
				ASSERT_EQ( cList[i].second, c->second );
				ASSERT_EQ( int( i ), cMap.IndexOf( c->first ) );
			}
			ASSERT_EQ( cList.size(), i );
			for( int k = 0; k < 64; k++ )
			{
				int nWant = -1;
				for( size_t j = 0; j < cList.size(); j++ )
					if( cList[j].first == k )
						nWant = cList[j].second;
				ASSERT_EQ( nWant, cMap.Get( k, -1 ) ) << "step " << nStep << " key " << k;
			}
		}
	}
}

TEST( HashMapTest, PushOverAndOverKeepsItSmall )
{
	// Push() of a key that is not last leaves a hole; the holes must be
	// compacted away, not piled up behind the entries (an LRU cache does
	// nothing but such Push()es)
	g_nLiveValues = 0;
	{
		HashMap<int, LiveValue> cMap;
		for( int i = 0; i < 1000; i++ )
			cMap.Set( i, LiveValue() );
		uint32 nSeed = 1;
		for( int n = 0; n < 200000; n++ )
		{
			nSeed = nSeed * 1103515245U + 12345U;
			cMap.Push( int( ( nSeed >> 16 ) % 1000 ), LiveValue() );
		}
		EXPECT_EQ( 1000, cMap.Count() );
		EXPECT_LE( g_nLiveValues, 3000 );	// 1000 entries and some holes, not 200000
		EXPECT_EQ( 999, cMap.IndexOf( cMap.KeyAt( 999 ) ) );
	}
	EXPECT_EQ( 0, g_nLiveValues );
}
