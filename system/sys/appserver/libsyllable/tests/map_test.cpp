/*  os::Map (ordered by key) and os::UnorderedMap: insert, update, delete,
 *  push and pop, reversing, sorting, iterating, and os::Hash.
 */

#include <gtest/gtest.h>

#include <util/map.h>
#include <util/string.h>

#include <string>
#include <vector>
#include <list>

using namespace os;

namespace
{

typedef Map<String, int> Ages;
typedef UnorderedMap<int, String> Names;

Ages BuildAges()
{
	Ages cAges;
	cAges.Set( "kurt", 41 ).Set( "anna", 33 ).Set( "rick", 50 ).Set( "dee", 27 );
	return cAges;
}

std::string Joined( const std::vector<String>& cKeys )
{
	std::string cText;
	for( size_t i = 0; i < cKeys.size(); i++ )
		cText += ( i > 0 ? "," : "" ) + std::string( cKeys[i].c_str() );
	return cText;
}

bool ByLength( const String& cA, const String& cB )
{
	return cA.size() < cB.size();
}

bool CaseInsensitive( const String& cA, const String& cB )
{
	return cA.CompareNoCase( cB ) < 0;
}

bool IsOdd( const int& nKey, const String& )
{
	return nKey % 2 != 0;
}

struct Even
{
	bool operator()( int nKey, int ) const { return nKey % 2 == 0; }
};

struct Sum
{
	explicit Sum( int* pnTotal ) : m_pnTotal( pnTotal ) {}
	template<class K> void operator()( const K&, int nValue ) const { *m_pnTotal += nValue; }
	int* m_pnTotal;
};

} // namespace

// ---- os::Map ----

TEST( MapTest, InsertUpdateAndSet )
{
	Ages cAges = BuildAges();
	EXPECT_EQ( 4, cAges.Count() );
	EXPECT_FALSE( cAges.Insert( "anna", 99 ) );		// in already: stays
	EXPECT_EQ( 33, cAges.Get( "anna" ) );
	EXPECT_TRUE( cAges.Insert( "zoe", 8 ) );
	EXPECT_TRUE( cAges.Update( "anna", 34 ) );
	EXPECT_EQ( 34, cAges.Get( "anna" ) );
	EXPECT_FALSE( cAges.Update( "nobody", 1 ) );		// not in: not added
	EXPECT_FALSE( cAges.Has( "nobody" ) );
	cAges.Set( "anna", 35 ).Set( "bob", 60 );
	EXPECT_EQ( 35, cAges.Get( "anna" ) );
	EXPECT_EQ( 6, cAges.Count() );
	EXPECT_EQ( -1, cAges.Get( "nobody", -1 ) );
	EXPECT_EQ( 0, cAges.Get( "nobody" ) );
}

TEST( MapTest, FindAndSubscriptChangeInPlace )
{
	Ages cAges = BuildAges();
	int* pnAge = cAges.Find( "rick" );
	ASSERT_TRUE( pnAge != NULL );
	*pnAge += 1;
	EXPECT_EQ( 51, cAges.Get( "rick" ) );
	EXPECT_TRUE( cAges.Find( "nobody" ) == NULL );
	cAges["new"] += 5;					// added with 0
	EXPECT_EQ( 5, cAges.Get( "new" ) );
	const Ages& cConst = cAges;
	EXPECT_EQ( 5, *cConst.Find( "new" ) );
}

TEST( MapTest, KeysStayInOrder )
{
	Ages cAges = BuildAges();
	EXPECT_EQ( "anna,dee,kurt,rick", Joined( cAges.Keys() ) );
	cAges.Set( "bob", 1 );
	EXPECT_EQ( "anna,bob,dee,kurt,rick", Joined( cAges.Keys() ) );
	std::vector<int> cValues = cAges.Values();
	ASSERT_EQ( 5u, cValues.size() );
	EXPECT_EQ( 33, cValues[0] );
	EXPECT_EQ( 50, cValues[4] );
	std::string cWalk;
	for( Ages::const_iterator i = cAges.begin(); i != cAges.end(); ++i )
		cWalk += i->first.c_str();
	EXPECT_EQ( "annabobdeekurtrick", cWalk );
	std::string cBack;
	for( Ages::const_reverse_iterator i = cAges.rbegin(); i != cAges.rend(); ++i )
		cBack += i->first.c_str();
	EXPECT_EQ( "rickkurtdeebobanna", cBack );
}

TEST( MapTest, RemoveTakeAndErase )
{
	Ages cAges = BuildAges();
	EXPECT_TRUE( cAges.Remove( "kurt" ) );
	EXPECT_FALSE( cAges.Remove( "kurt" ) );
	int nAge = 0;
	EXPECT_TRUE( cAges.Take( "dee", &nAge ) );
	EXPECT_EQ( 27, nAge );
	EXPECT_FALSE( cAges.Take( "dee", &nAge ) );
	Ages::iterator i = cAges.Erase( cAges.begin() );	// anna
	EXPECT_STREQ( "rick", i->first.c_str() );
	EXPECT_EQ( 1, cAges.Count() );
	cAges.Clear();
	EXPECT_TRUE( cAges.IsEmpty() );
}

TEST( MapTest, RemoveIfAndForEach )
{
	Map<int, int> cSquares;
	for( int i = 0; i < 10; i++ )
		cSquares.Set( i, i * i );
	int nTotal = 0;
	cSquares.ForEach( Sum( &nTotal ) );
	EXPECT_EQ( 285, nTotal );
	EXPECT_EQ( 5, cSquares.RemoveIf( Even() ) );
	EXPECT_EQ( 5, cSquares.Count() );
	EXPECT_FALSE( cSquares.Has( 4 ) );
	EXPECT_EQ( 81, cSquares.Get( 9 ) );
}

TEST( MapTest, PushAndPopTakeTheEnds )
{
	Ages cAges = BuildAges();
	cAges.Push( "bob", 60 );				// goes where its key belongs
	String cName;
	int nAge = 0;
	EXPECT_TRUE( cAges.First( &cName, &nAge ) );
	EXPECT_STREQ( "anna", cName.c_str() );
	EXPECT_TRUE( cAges.Last( &cName ) );
	EXPECT_STREQ( "rick", cName.c_str() );
	EXPECT_TRUE( cAges.Pop( &cName, &nAge ) );
	EXPECT_STREQ( "rick", cName.c_str() );
	EXPECT_EQ( 50, nAge );
	EXPECT_TRUE( cAges.PopFront( &cName, &nAge ) );
	EXPECT_STREQ( "anna", cName.c_str() );
	EXPECT_TRUE( cAges.PopFront( &cName ) );
	EXPECT_STREQ( "bob", cName.c_str() );
	EXPECT_TRUE( cAges.Pop() );
	EXPECT_TRUE( cAges.Pop() );
	EXPECT_FALSE( cAges.Pop( &cName ) );
	EXPECT_FALSE( cAges.PopFront() );
	EXPECT_FALSE( cAges.First( &cName ) );
	EXPECT_FALSE( cAges.Last( &cName ) );
}

TEST( MapTest, ReverseKeepsTheReversedOrder )
{
	Ages cAges = BuildAges();
	EXPECT_FALSE( cAges.IsReversed() );
	cAges.Reverse();
	EXPECT_TRUE( cAges.IsReversed() );
	EXPECT_EQ( "rick,kurt,dee,anna", Joined( cAges.Keys() ) );
	cAges.Set( "bob", 1 ).Set( "zoe", 2 );		// new keys go in their place
	EXPECT_EQ( "zoe,rick,kurt,dee,bob,anna", Joined( cAges.Keys() ) );
	EXPECT_EQ( 41, cAges.Get( "kurt" ) );		// lookups still work
	String cName;
	cAges.Pop( &cName );
	EXPECT_STREQ( "anna", cName.c_str() );
	cAges.Reverse();
	EXPECT_EQ( "bob,dee,kurt,rick,zoe", Joined( cAges.Keys() ) );
}

TEST( MapTest, SortByAComparisonOfYourOwn )
{
	Map<String, int> cWords;
	cWords.Set( "ccc", 3 ).Set( "a", 1 ).Set( "dddd", 4 ).Set( "bb", 2 );
	EXPECT_TRUE( cWords.Sort( ByLength ) );
	EXPECT_EQ( "a,bb,ccc,dddd", Joined( cWords.Keys() ) );
	EXPECT_TRUE( cWords.Sort( ByLength, true ) );
	EXPECT_EQ( "dddd,ccc,bb,a", Joined( cWords.Keys() ) );
	EXPECT_EQ( 3, cWords.Get( "ccc" ) );
	cWords.Set( "eeeee", 5 );
	EXPECT_STREQ( "eeeee", cWords.begin()->first.c_str() );
	EXPECT_TRUE( cWords.Sort( CaseInsensitive ) );
	EXPECT_EQ( "a,bb,ccc,dddd,eeeee", Joined( cWords.Keys() ) );
	EXPECT_TRUE( cWords.Sort() );			// back to operator<
	EXPECT_EQ( "a,bb,ccc,dddd,eeeee", Joined( cWords.Keys() ) );
}

TEST( MapTest, SortThatWouldMergeKeysFails )
{
	Map<String, int> cWords;
	cWords.Set( "aa", 1 ).Set( "bb", 2 ).Set( "c", 3 );
	EXPECT_FALSE( cWords.Sort( ByLength ) );		// "aa" and "bb" are equal by length
	EXPECT_EQ( 3, cWords.Count() );
	EXPECT_EQ( "aa,bb,c", Joined( cWords.Keys() ) );	// unchanged
	Map<String, int> cCase;
	cCase.Set( "B", 1 ).Set( "a", 2 );
	EXPECT_EQ( "B,a", Joined( cCase.Keys() ) );		// operator<: capitals first
	EXPECT_TRUE( cCase.Sort( CaseInsensitive ) );
	EXPECT_EQ( "a,B", Joined( cCase.Keys() ) );
}

TEST( MapTest, ItemsByValue )
{
	Ages cAges = BuildAges();
	cAges.Set( "bob", 33 );
	std::vector<Ages::Item> cItems = cAges.ItemsByValue();
	ASSERT_EQ( 5u, cItems.size() );
	EXPECT_STREQ( "dee", cItems[0].first.c_str() );
	EXPECT_STREQ( "anna", cItems[1].first.c_str() );	// 33, and before bob by key
	EXPECT_STREQ( "bob", cItems[2].first.c_str() );
	EXPECT_STREQ( "rick", cItems[4].first.c_str() );
	cItems = cAges.ItemsByValue( NULL, true );
	EXPECT_STREQ( "rick", cItems[0].first.c_str() );
	EXPECT_STREQ( "anna", cItems[2].first.c_str() );	// ties keep their order
	EXPECT_STREQ( "bob", cItems[3].first.c_str() );
	EXPECT_EQ( "anna,bob,dee,kurt,rick", Joined( cAges.Keys() ) );	// the map is unchanged
}

TEST( MapTest, Bounds )
{
	Map<int, int> cMap;
	for( int i = 0; i < 100; i += 10 )
		cMap.Set( i, i );
	EXPECT_EQ( 30, cMap.LowerBound( 30 )->first );
	EXPECT_EQ( 40, cMap.UpperBound( 30 )->first );
	EXPECT_EQ( 40, cMap.LowerBound( 31 )->first );
	EXPECT_TRUE( cMap.LowerBound( 91 ) == cMap.end() );
	int nInRange = 0;
	for( Map<int, int>::iterator i = cMap.LowerBound( 20 ); i != cMap.UpperBound( 50 ); ++i )
		nInRange++;
	EXPECT_EQ( 4, nInRange );				// 20, 30, 40, 50
}

TEST( MapTest, MergeCompareAndSwap )
{
	Ages cA = BuildAges();
	Ages cB;
	cB.Set( "anna", 1 ).Set( "new", 2 );
	Ages cKeep = cA;
	cKeep.Merge( cB, false );
	EXPECT_EQ( 33, cKeep.Get( "anna" ) );
	EXPECT_EQ( 2, cKeep.Get( "new" ) );
	cA.Merge( cB );
	EXPECT_EQ( 1, cA.Get( "anna" ) );
	EXPECT_EQ( 5, cA.Count() );
	Ages cCopy = cA;
	EXPECT_TRUE( cCopy == cA );
	cCopy.Reverse();
	EXPECT_TRUE( cCopy == cA );				// the order doesn't matter
	cCopy.Set( "anna", 2 );
	EXPECT_TRUE( cCopy != cA );
	cCopy.Swap( cB );
	EXPECT_EQ( 2, cCopy.Count() );
	EXPECT_EQ( 5, cB.Count() );
	EXPECT_TRUE( cB.IsReversed() );
}

TEST( MapTest, AppendAndRemoveAtForVectorValues )
{
	Map<String, std::vector<String> > cTags;
	cTags.Append( "pyro", String( "os" ) ).Append( "pyro", "desktop" ).Append( "afs", "fs" );
	EXPECT_EQ( 2, cTags.Count() );
	ASSERT_EQ( 2u, cTags.Find( "pyro" )->size() );
	EXPECT_STREQ( "desktop", ( *cTags.Find( "pyro" ) )[1].c_str() );
	EXPECT_TRUE( cTags.RemoveAt( "pyro", 0 ) );
	ASSERT_EQ( 1u, cTags.Find( "pyro" )->size() );
	EXPECT_STREQ( "desktop", ( *cTags.Find( "pyro" ) )[0].c_str() );
	EXPECT_FALSE( cTags.RemoveAt( "pyro", 1 ) );		// past the end
	EXPECT_FALSE( cTags.RemoveAt( "pyro", -1 ) );
	EXPECT_FALSE( cTags.RemoveAt( "nobody", 0 ) );
	EXPECT_TRUE( cTags.RemoveAt( "pyro", 0 ) );		// empty vector stays
	EXPECT_TRUE( cTags.Has( "pyro" ) );
	EXPECT_TRUE( cTags.Find( "pyro" )->empty() );
	EXPECT_TRUE( cTags.RemoveAt( "afs", 0, true ) );	// the key goes with its last item
	EXPECT_FALSE( cTags.Has( "afs" ) );
	Map<int, std::list<int> > cLists;			// any V with push_back() and erase()
	cLists.Append( 1, 10 ).Append( 1, 20 ).Append( 1, 30 );
	EXPECT_TRUE( cLists.RemoveAt( 1, 1 ) );
	EXPECT_EQ( 30, cLists.Find( 1 )->back() );
	EXPECT_EQ( 2u, cLists.Find( 1 )->size() );
}

// ---- os::UnorderedMap ----

TEST( UnorderedMapTest, InsertUpdateDelete )
{
	Names cNames;
	cNames.Set( 3, "three" ).Set( 1, "one" ).Set( 2, "two" );
	EXPECT_EQ( 3, cNames.Count() );
	EXPECT_FALSE( cNames.Insert( 1, "uno" ) );
	EXPECT_TRUE( cNames.Update( 1, "uno" ) );
	EXPECT_STREQ( "uno", cNames.Get( 1 ).c_str() );
	EXPECT_FALSE( cNames.Update( 9, "nine" ) );
	EXPECT_STREQ( "?", cNames.Get( 9, "?" ).c_str() );
	cNames[4] = "four";
	EXPECT_TRUE( cNames.Has( 4 ) );
	String cName;
	EXPECT_TRUE( cNames.Take( 4, &cName ) );
	EXPECT_STREQ( "four", cName.c_str() );
	EXPECT_TRUE( cNames.Remove( 3 ) );
	EXPECT_FALSE( cNames.Remove( 3 ) );
	EXPECT_EQ( 2, cNames.Count() );
}

TEST( UnorderedMapTest, SortedViews )
{
	Names cNames;
	cNames.Set( 3, "c" ).Set( 1, "z" ).Set( 2, "a" ).Set( 5, "b" );
	std::vector<int> cKeys = cNames.SortedKeys();
	ASSERT_EQ( 4u, cKeys.size() );
	EXPECT_EQ( 1, cKeys[0] );
	EXPECT_EQ( 5, cKeys[3] );
	cKeys = cNames.SortedKeys( NULL, true );
	EXPECT_EQ( 5, cKeys[0] );
	std::vector<Names::Item> cItems = cNames.ItemsByKey();
	EXPECT_STREQ( "z", cItems[0].second.c_str() );
	cItems = cNames.ItemsByValue();
	EXPECT_EQ( 2, cItems[0].first );			// "a"
	EXPECT_EQ( 1, cItems[3].first );			// "z"
	cItems = cNames.ItemsByValue( NULL, true );
	EXPECT_EQ( 1, cItems[0].first );
}

TEST( UnorderedMapTest, PopEmptiesIt )
{
	Names cNames;
	for( int i = 0; i < 50; i++ )
		cNames.Set( i, "x" );
	int nKey = -1;
	int nPopped = 0;
	while( cNames.Pop( &nKey ) )
	{
		EXPECT_TRUE( nKey >= 0 && nKey < 50 );
		nPopped++;
	}
	EXPECT_EQ( 50, nPopped );
	EXPECT_TRUE( cNames.IsEmpty() );
}

TEST( UnorderedMapTest, ManyKeysRemoveIfAndReserve )
{
	UnorderedMap<String, int> cMap;
	cMap.Reserve( 1000 );
	size_t nBuckets = cMap.GetBucketCount();
	for( int i = 0; i < 1000; i++ )
		cMap.Set( String( "key" ) + String( std::string( 1, char( 'a' + i % 26 ) ) ) + String( std::string( i / 26 + 1, '#' ) ), i );
	EXPECT_EQ( 1000, cMap.Count() );
	EXPECT_EQ( nBuckets, cMap.GetBucketCount() );		// no rehash
	EXPECT_LE( cMap.GetLoadFactor(), 1.0f );
	Names cNumbers;
	for( int i = 0; i < 100; i++ )
		cNumbers.Set( i, "n" );
	EXPECT_EQ( 50, cNumbers.RemoveIf( IsOdd ) );
	EXPECT_FALSE( cNumbers.Has( 7 ) );
	EXPECT_TRUE( cNumbers.Has( 8 ) );
	Names cCopy = cNumbers;
	EXPECT_TRUE( cCopy == cNumbers );
	cCopy.Set( 8, "m" );
	EXPECT_TRUE( cCopy != cNumbers );
}

TEST( UnorderedMapTest, AppendAndRemoveAtForVectorValues )
{
	UnorderedMap<int, std::vector<int> > cGroups;
	for( int i = 0; i < 10; i++ )
		cGroups.Append( i % 3, i );
	EXPECT_EQ( 3, cGroups.Count() );
	ASSERT_EQ( 4u, cGroups.Find( 0 )->size() );		// 0 3 6 9
	EXPECT_TRUE( cGroups.RemoveAt( 0, 3 ) );
	EXPECT_EQ( 6, cGroups.Find( 0 )->back() );
	EXPECT_FALSE( cGroups.RemoveAt( 0, 3 ) );
	EXPECT_FALSE( cGroups.RemoveAt( 7, 0 ) );
	cGroups.Append( 7, 70 );
	EXPECT_TRUE( cGroups.RemoveAt( 7, 0, true ) );
	EXPECT_FALSE( cGroups.Has( 7 ) );
}

// ---- os::Hash ----

TEST( HashTest, EqualKeysHashTheSame )
{
	EXPECT_EQ( Hash<String>()( "pyro" ), Hash<std::string>()( std::string( "pyro" ) ) );
	EXPECT_NE( Hash<String>()( "pyro" ), Hash<String>()( "pyrO" ) );
	EXPECT_EQ( Hash<double>()( 0.0 ), Hash<double>()( -0.0 ) );
	EXPECT_EQ( Hash<float>()( 0.0f ), Hash<float>()( -0.0f ) );
	EXPECT_NE( Hash<int>()( 1 ), Hash<int>()( 2 ) );
	int n = 0;
	EXPECT_EQ( Hash<int*>()( &n ), Hash<int*>()( &n ) );
	EXPECT_EQ( 0u, HashBytes( "", 0 ) );			// MurmurHash3's value for nothing
	EXPECT_EQ( 0x248bfa47u, HashBytes( "hello", 5 ) );	// and for "hello" (seed 0)
	EXPECT_NE( HashBytes( "hello", 5, 1 ), HashBytes( "hello", 5 ) );
}

TEST( HashTest, NextPowerOfTwo )
{
	EXPECT_EQ( 1u, NextPowerOfTwo( 0 ) );
	EXPECT_EQ( 1u, NextPowerOfTwo( 1 ) );
	EXPECT_EQ( 2u, NextPowerOfTwo( 2 ) );
	EXPECT_EQ( 4u, NextPowerOfTwo( 3 ) );
	EXPECT_EQ( 1024u, NextPowerOfTwo( 1000 ) );
	EXPECT_EQ( 1024u, NextPowerOfTwo( 1024 ) );
	EXPECT_EQ( 2048u, NextPowerOfTwo( 1025 ) );
}
