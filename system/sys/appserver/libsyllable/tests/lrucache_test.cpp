/*  os::LRUCache: forgetting the least recently used, costs, the eviction
 *  function, Peek() against Get(), and a random run against a plain list.
 */

#include <gtest/gtest.h>

#include <util/lrucache.h>

#include <string>
#include <vector>
#include <cstdlib>
#include <stdio.h>

using namespace os;

namespace
{

typedef LRUCache<std::string, int> Cache;

std::string Joined( const std::vector<std::string>& cKeys )
{
	std::string cText;
	for( size_t i = 0; i < cKeys.size(); i++ )
		cText += ( i > 0 ? "," : "" ) + cKeys[i];
	return cText;
}

/* An eviction function that writes down what it was given */
struct Evicted
{
	std::vector<std::string> m_cKeys;
	int m_nTotal;
	Evicted() : m_nTotal( 0 ) {}
};
void Record( const std::string& cKey, int& nValue, void* pUser )
{
	Evicted* pcEvicted = static_cast<Evicted*>( pUser );
	pcEvicted->m_cKeys.push_back( cKey );
	pcEvicted->m_nTotal += nValue;
}

/* Values that are things to free: the eviction function deletes them */
int g_nLiveIcons = 0;
struct Icon
{
	Icon() { g_nLiveIcons++; }
	~Icon() { g_nLiveIcons--; }
};
void FreeIcon( const int&, Icon*& pcIcon, void* )
{
	delete pcIcon;
	pcIcon = NULL;
}

/* An entry of the random run's model */
struct Entry
{
	int m_nKey, m_nValue;
	size_t m_nCost;
};

struct SumValues
{
	explicit SumValues( int* pnTotal ) : m_pnTotal( pnTotal ) {}
	void operator()( const std::string&, int nValue ) const { *m_pnTotal += nValue; }
	int* m_pnTotal;
};

} // namespace

TEST( LRUCache, StartsEmpty )
{
	Cache cCache( 3 );
	EXPECT_TRUE( cCache.IsEmpty() );
	EXPECT_EQ( 0, cCache.Count() );
	EXPECT_EQ( 3u, cCache.GetCapacity() );
	EXPECT_EQ( 0u, cCache.GetCost() );
	EXPECT_EQ( -1, cCache.Get( "a", -1 ) );
	EXPECT_TRUE( cCache.Find( "a" ) == NULL );
	EXPECT_TRUE( cCache.Peek( "a" ) == NULL );
	EXPECT_FALSE( cCache.Has( "a" ) );
	EXPECT_FALSE( cCache.Remove( "a" ) );
	EXPECT_FALSE( cCache.PopOldest() );
	EXPECT_EQ( 0, cCache.GetHits() );
	EXPECT_EQ( 2, cCache.GetMisses() );
}

TEST( LRUCache, ForgetsTheLeastRecentlyUsed )
{
	Cache cCache( 3 );
	cCache.Set( "a", 1 );
	cCache.Set( "b", 2 );
	cCache.Set( "c", 3 );
	EXPECT_EQ( std::string( "a,b,c" ), Joined( cCache.Keys() ) );
	EXPECT_EQ( 1, cCache.Get( "a" ) );		// a is the newest now
	EXPECT_EQ( std::string( "b,c,a" ), Joined( cCache.Keys() ) );
	cCache.Set( "d", 4 );				// b goes
	EXPECT_EQ( std::string( "c,a,d" ), Joined( cCache.Keys() ) );
	EXPECT_FALSE( cCache.Has( "b" ) );
	EXPECT_EQ( 3, cCache.Count() );
	EXPECT_EQ( 3u, cCache.GetCost() );
}

TEST( LRUCache, PeekAndHasDoNotUse )
{
	Cache cCache( 2 );
	cCache.Set( "a", 1 );
	cCache.Set( "b", 2 );
	ASSERT_TRUE( cCache.Peek( "a" ) != NULL );
	EXPECT_EQ( 1, *cCache.Peek( "a" ) );
	EXPECT_TRUE( cCache.Has( "a" ) );
	cCache.Set( "c", 3 );				// a is still the oldest: it goes
	EXPECT_FALSE( cCache.Has( "a" ) );
	EXPECT_EQ( 0, cCache.GetHits() + cCache.GetMisses() );
}

TEST( LRUCache, FindChangesInPlace )
{
	Cache cCache( 3 );
	cCache.Set( "a", 1 );
	cCache.Set( "b", 2 );
	int* pnValue = cCache.Find( "a" );
	ASSERT_TRUE( pnValue != NULL );
	*pnValue = 10;
	EXPECT_EQ( 10, *cCache.Peek( "a" ) );
	EXPECT_EQ( std::string( "b,a" ), Joined( cCache.Keys() ) );
	EXPECT_EQ( 1, cCache.GetHits() );
	cCache.Find( "nope" );
	EXPECT_EQ( 1, cCache.GetMisses() );
	cCache.ResetStats();
	EXPECT_EQ( 0, cCache.GetHits() );
	EXPECT_EQ( 0, cCache.GetMisses() );
}

TEST( LRUCache, SetReplacesAndMakesNewest )
{
	Cache cCache( 3 );
	cCache.Set( "a", 1 );
	cCache.Set( "b", 2 );
	cCache.Set( "c", 3 );
	cCache.Set( "a", 100 );
	EXPECT_EQ( std::string( "b,c,a" ), Joined( cCache.Keys() ) );
	EXPECT_EQ( 100, *cCache.Peek( "a" ) );
	EXPECT_EQ( 3, cCache.Count() );
	EXPECT_EQ( 3u, cCache.GetCost() );
}

TEST( LRUCache, CostsMakeAMemoryBudget )
{
	Cache cCache( 100 );
	cCache.Set( "small", 1, 10 );
	cCache.Set( "medium", 2, 30 );
	cCache.Set( "large", 3, 50 );
	EXPECT_EQ( 90u, cCache.GetCost() );
	EXPECT_EQ( 30u, cCache.GetCostOf( "medium" ) );
	EXPECT_EQ( 0u, cCache.GetCostOf( "huge" ) );
	cCache.Set( "another", 4, 25 );			// small and medium go: 10 + 30
	EXPECT_EQ( std::string( "large,another" ), Joined( cCache.Keys() ) );
	EXPECT_EQ( 75u, cCache.GetCost() );
	cCache.Set( "large", 5, 20 );			// cheaper now
	EXPECT_EQ( 45u, cCache.GetCost() );
	EXPECT_EQ( std::string( "another,large" ), Joined( cCache.Keys() ) );
	cCache.Set( "exact", 6, 55 );			// fills it exactly: nothing goes
	EXPECT_EQ( 100u, cCache.GetCost() );
	EXPECT_EQ( 3, cCache.Count() );
}

TEST( LRUCache, TooCostlyIsNotKept )
{
	Cache cCache( 10 );
	cCache.Set( "a", 1, 4 );
	cCache.Set( "b", 2, 4 );
	cCache.Set( "huge", 3, 11 );
	EXPECT_FALSE( cCache.Has( "huge" ) );
	EXPECT_EQ( std::string( "a,b" ), Joined( cCache.Keys() ) );	// nothing went for it
	cCache.Set( "a", 9, 11 );			// and a's old value is gone
	EXPECT_FALSE( cCache.Has( "a" ) );
	EXPECT_EQ( 4u, cCache.GetCost() );
	cCache.Set( "whole", 7, 10 );			// the whole capacity is fine
	EXPECT_EQ( std::string( "whole" ), Joined( cCache.Keys() ) );
}

TEST( LRUCache, EvictionFunctionGetsWhatGoes )
{
	Evicted cEvicted;
	{
		Cache cCache( 3 );
		cCache.SetEvictHook( Record, &cEvicted );
		cCache.Set( "a", 1 );
		cCache.Set( "b", 2 );
		cCache.Set( "c", 3 );
		cCache.Get( "a" );
		cCache.Set( "d", 4 );			// b
		cCache.Set( "e", 5 );			// c
		EXPECT_EQ( std::string( "b,c" ), Joined( cEvicted.m_cKeys ) );
		EXPECT_EQ( 5, cEvicted.m_nTotal );

		cCache.Set( "a", 10 );			// replaced: not evicted
		cCache.Remove( "d" );			// taken out: not evicted
		int nValue = 0;
		EXPECT_TRUE( cCache.Take( "e", &nValue ) );
		EXPECT_EQ( 5, nValue );
		EXPECT_EQ( 2, int( cEvicted.m_cKeys.size() ) );

		cCache.Set( "f", 6 ).Set( "g", 7 );
		cCache.SetCapacity( 1 );		// a and f go
		EXPECT_EQ( std::string( "b,c,a,f" ), Joined( cEvicted.m_cKeys ) );
		EXPECT_EQ( 1u, cCache.GetCapacity() );
		EXPECT_EQ( std::string( "g" ), Joined( cCache.Keys() ) );
	}
	// the destructor drops g through it too
	EXPECT_EQ( std::string( "b,c,a,f,g" ), Joined( cEvicted.m_cKeys ) );
	EXPECT_EQ( 5 + 10 + 6 + 7, cEvicted.m_nTotal );
}

TEST( LRUCache, ClearAndPopOldest )
{
	Evicted cEvicted;
	Cache cCache( 5 );
	cCache.SetEvictHook( Record, &cEvicted );
	cCache.Set( "a", 1 ).Set( "b", 2 ).Set( "c", 3, 0 );	// c costs nothing
	std::string cKey;
	int nValue = 0;
	ASSERT_TRUE( cCache.PopOldest( &cKey, &nValue ) );
	EXPECT_EQ( std::string( "a" ), cKey );
	EXPECT_EQ( 1, nValue );
	EXPECT_TRUE( cEvicted.m_cKeys.empty() );		// popped, not evicted
	EXPECT_EQ( 1u, cCache.GetCost() );
	cCache.Clear();
	EXPECT_EQ( std::string( "b,c" ), Joined( cEvicted.m_cKeys ) );	// the free one too
	EXPECT_TRUE( cCache.IsEmpty() );
	EXPECT_EQ( 0u, cCache.GetCost() );
	cCache.Set( "d", 4 );
	EXPECT_EQ( 1, cCache.Count() );
}

TEST( LRUCache, FreesThingsItOwns )
{
	g_nLiveIcons = 0;
	{
		LRUCache<int, Icon*> cIcons( 8 );
		cIcons.SetEvictHook( FreeIcon );
		for( int i = 0; i < 100; i++ )
		{
			if( cIcons.Get( i % 20, NULL ) == NULL )
				cIcons.Set( i % 20, new Icon );
		}
		EXPECT_EQ( 8, g_nLiveIcons );
		EXPECT_EQ( 8, cIcons.Count() );
	}
	EXPECT_EQ( 0, g_nLiveIcons );
}

TEST( LRUCache, ForEachAndSwap )
{
	Cache cA( 3 ), cB( 10 );
	cA.Set( "a", 1 ).Set( "b", 2 );
	cB.Set( "x", 5 );
	int nTotal = 0;
	cA.ForEach( SumValues( &nTotal ) );
	EXPECT_EQ( 3, nTotal );
	EXPECT_EQ( std::string( "a,b" ), Joined( cA.Keys() ) );	// ForEach used nothing
	cA.Swap( cB );
	EXPECT_EQ( 10u, cA.GetCapacity() );
	EXPECT_EQ( std::string( "x" ), Joined( cA.Keys() ) );
	EXPECT_EQ( 2u, cB.size() );
	EXPECT_FALSE( cB.empty() );
}

TEST( LRUCache, ValuesAreNotCopiedToMove )
{
	// a big value moves to the back by a swap, and stays whole
	LRUCache<int, std::vector<int> > cCache( 3 );
	std::vector<int> cBig( 1000, 7 );
	cCache.Set( 1, cBig ).Set( 2, std::vector<int>( 5, 2 ) );
	std::vector<int>* pcValue = cCache.Find( 1 );
	ASSERT_TRUE( pcValue != NULL );
	EXPECT_EQ( 1000u, pcValue->size() );
	EXPECT_EQ( 7, ( *pcValue )[999] );
	EXPECT_EQ( 5u, cCache.Get( 2 ).size() );
	EXPECT_EQ( 1000u, cCache.Peek( 1 )->size() );
}

TEST( LRUCache, ManyUsesStayFast )
{
	// a million uses of a few hundred keys: HashMap::Push() must clear
	// the holes it leaves, or the table grows with every use
	LRUCache<int, int> cCache( 500 );
	for( int i = 0; i < 500; i++ )
		cCache.Set( i, i );
	srand( 5 );
	long nSum = 0;
	for( int i = 0; i < 1000000; i++ )
		nSum += cCache.Get( rand() % 500 );
	EXPECT_EQ( 500, cCache.Count() );
	EXPECT_TRUE( nSum > 0 );
	EXPECT_TRUE( cCache.Has( 0 ) && cCache.Has( 499 ) );
}

/* Against a list of (key, value, cost), oldest first */
TEST( LRUCache, MatchesListOverRandomSteps )
{
	srand( 4242 );
	Evicted cEvicted;
	LRUCache<std::string, int> cCache( 40 );
	cCache.SetEvictHook( Record, &cEvicted );
	std::vector<Entry> cModel;
	size_t nCapacity = 40, nEvicted = 0;
	char zKey[16];
	for( int nStep = 0; nStep < 20000; nStep++ )
	{
		int nKey = rand() % 30;
		snprintf( zKey, sizeof( zKey ), "k%d", nKey );
		int nAt = -1;
		for( size_t i = 0; i < cModel.size(); i++ )
			if( cModel[i].m_nKey == nKey )
				nAt = int( i );
		size_t nCost = 0;
		for( size_t i = 0; i < cModel.size(); i++ )
			nCost += cModel[i].m_nCost;
		switch( rand() % 6 )
		{
			case 0:
			case 1:
			{
				Entry cNew = { nKey, rand() % 1000, size_t( rand() % 8 ) };
				if( nAt >= 0 )
				{
					nCost -= cModel[nAt].m_nCost;
					cModel.erase( cModel.begin() + nAt );
				}
				bool bKept = cNew.m_nCost <= nCapacity;
				cCache.Set( zKey, cNew.m_nValue, cNew.m_nCost );
				ASSERT_EQ( bKept, cCache.Has( zKey ) ) << "step " << nStep;
				if( bKept )
				{
					while( nCost + cNew.m_nCost > nCapacity )
					{
						nCost -= cModel.front().m_nCost;
						cModel.erase( cModel.begin() );
						nEvicted++;
					}
					cModel.push_back( cNew );
				}
				break;
			}
			case 2:
			{
				int nGot = cCache.Get( zKey, -1 );
				if( nAt >= 0 )
				{
					ASSERT_EQ( cModel[nAt].m_nValue, nGot ) << "step " << nStep;
					Entry cUsed = cModel[nAt];
					cModel.erase( cModel.begin() + nAt );
					cModel.push_back( cUsed );
				}
				else
				{
					ASSERT_EQ( -1, nGot ) << "step " << nStep;
				}
				break;
			}
			case 3:
			{
				const int* pnPeek = cCache.Peek( zKey );
				ASSERT_EQ( nAt >= 0, pnPeek != NULL ) << "step " << nStep;
				if( nAt >= 0 )
				{
					ASSERT_EQ( cModel[nAt].m_nValue, *pnPeek ) << "step " << nStep;
				}
				break;
			}
			case 4:
				ASSERT_EQ( nAt >= 0, cCache.Remove( zKey ) ) << "step " << nStep;
				if( nAt >= 0 )
					cModel.erase( cModel.begin() + nAt );
				break;
			default:
				if( rand() % 100 == 0 )
				{
					nCapacity = size_t( 5 + rand() % 60 );
					cCache.SetCapacity( nCapacity );
					while( nCost > nCapacity )
					{
						nCost -= cModel.front().m_nCost;
						cModel.erase( cModel.begin() );
						nEvicted++;
					}
				}
		}
		ASSERT_EQ( int( cModel.size() ), cCache.Count() ) << "step " << nStep;
		ASSERT_EQ( nEvicted, cEvicted.m_cKeys.size() ) << "step " << nStep;
		std::vector<std::string> cKeys = cCache.Keys();
		for( size_t i = 0; i < cModel.size(); i++ )
		{
			snprintf( zKey, sizeof( zKey ), "k%d", cModel[i].m_nKey );
			ASSERT_EQ( std::string( zKey ), cKeys[i] ) << "step " << nStep;
		}
	}
	size_t nCost = 0;
	for( size_t i = 0; i < cModel.size(); i++ )
		nCost += cModel[i].m_nCost;
	EXPECT_EQ( nCost, cCache.GetCost() );
}
