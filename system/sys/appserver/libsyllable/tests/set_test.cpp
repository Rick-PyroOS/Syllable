/*  os::Set (ordered by key) and os::HashSet (hashed, in the order of its
 *  keys): add, find, remove, push and pop, sorting, iterating, the set
 *  operations, and a random run of each against a plain list.
 */

#include <gtest/gtest.h>

#include <util/set.h>
#include <util/hashset.h>

#include <algorithm>
#include <list>
#include <string>
#include <vector>
#include <cstdlib>

using namespace os;

namespace
{

typedef Set<std::string> Words;
typedef HashSet<std::string> HashWords;

template<class S> std::string Joined( const S& cSet )
{
	std::string cText;
	for( typename S::const_iterator i = cSet.begin(); i != cSet.end(); ++i )
		cText += ( i == cSet.begin() ? "" : "," ) + *i;
	return cText;
}

std::string JoinedVector( const std::vector<std::string>& cKeys )
{
	std::string cText;
	for( size_t i = 0; i < cKeys.size(); i++ )
		cText += ( i > 0 ? "," : "" ) + cKeys[i];
	return cText;
}

template<class S> S Of( const char* pzList )
{
	S cSet;
	std::string cList( pzList );
	size_t nStart = 0;
	while( nStart <= cList.size() && !cList.empty() )
	{
		size_t nComma = cList.find( ',', nStart );
		if( nComma == std::string::npos )
			nComma = cList.size();
		cSet.Add( cList.substr( nStart, nComma - nStart ) );
		nStart = nComma + 1;
	}
	return cSet;
}

bool ByLength( const std::string& cA, const std::string& cB )
{
	return cA.size() < cB.size();
}

bool IsOdd( int n )
{
	return n % 2 != 0;
}

struct StartsWithB
{
	bool operator()( const std::string& cWord ) const { return !cWord.empty() && cWord[0] == 'b'; }
};

struct Concat
{
	explicit Concat( std::string* pcOut ) : m_pcOut( pcOut ) {}
	void operator()( const std::string& cWord ) const { *m_pcOut += cWord; }
	std::string* m_pcOut;
};

/* Equal by name only, so Find() and Take() can give back the one in the set */
struct Font
{
	Font( const std::string& cName = "", int nSize = 0 ) : m_cName( cName ), m_nSize( nSize ) {}
	bool operator<( const Font& cOther ) const { return m_cName < cOther.m_cName; }
	bool operator==( const Font& cOther ) const { return m_cName == cOther.m_cName; }
	std::string m_cName;
	int m_nSize;
};
struct FontHash
{
	size_t operator()( const Font& cFont ) const { return Hash<std::string>()( cFont.m_cName ); }
};

} // namespace

// --- os::Set -----------------------------------------------------------

TEST( Set, AddInsertHasRemove )
{
	Words cTags;
	EXPECT_TRUE( cTags.IsEmpty() );
	cTags.Add( "work" ).Add( "home" ).Add( "urgent" ).Add( "home" );
	EXPECT_EQ( 3, cTags.Count() );
	EXPECT_EQ( std::string( "home,urgent,work" ), Joined( cTags ) );
	EXPECT_FALSE( cTags.Insert( "home" ) );
	EXPECT_TRUE( cTags.Insert( "garden" ) );
	EXPECT_TRUE( cTags.Has( "garden" ) );
	EXPECT_FALSE( cTags.Has( "office" ) );
	EXPECT_TRUE( cTags.Remove( "urgent" ) );
	EXPECT_FALSE( cTags.Remove( "urgent" ) );
	EXPECT_EQ( std::string( "garden,home,work" ), Joined( cTags ) );
	cTags.Push( "alpha" );
	EXPECT_EQ( std::string( "alpha" ), *cTags.begin() );
	EXPECT_EQ( 4u, cTags.size() );
}

TEST( Set, FromRange )
{
	std::vector<int> cNumbers;
	cNumbers.push_back( 3 );
	cNumbers.push_back( 1 );
	cNumbers.push_back( 3 );
	cNumbers.push_back( 2 );
	Set<int> cSet( cNumbers.begin(), cNumbers.end() );
	EXPECT_EQ( 3, cSet.Count() );
	int nFirst = 0, nLast = 0;
	ASSERT_TRUE( cSet.First( &nFirst ) );
	ASSERT_TRUE( cSet.Last( &nLast ) );
	EXPECT_EQ( 1, nFirst );
	EXPECT_EQ( 3, nLast );
}

TEST( Set, FindAndTakeGiveTheKeyThatIsIn )
{
	Set<Font> cFonts;
	cFonts.Add( Font( "sans", 10 ) ).Add( Font( "mono", 12 ) );
	cFonts.Add( Font( "sans", 99 ) );		// in already: the 10 stays
	const Font* pcFont = cFonts.Find( Font( "sans" ) );
	ASSERT_TRUE( pcFont != NULL );
	EXPECT_EQ( 10, pcFont->m_nSize );
	EXPECT_TRUE( cFonts.Find( Font( "serif" ) ) == NULL );

	Font cTaken;
	EXPECT_TRUE( cFonts.Take( Font( "mono" ), &cTaken ) );
	EXPECT_EQ( 12, cTaken.m_nSize );
	EXPECT_FALSE( cFonts.Take( Font( "mono" ), &cTaken ) );
	EXPECT_TRUE( cFonts.Take( Font( "sans" ), NULL ) );
	EXPECT_TRUE( cFonts.IsEmpty() );
}

TEST( Set, PopFirstLastAndEmpty )
{
	Set<int> cSet;
	int nValue = 42;
	EXPECT_FALSE( cSet.Pop( &nValue ) );
	EXPECT_FALSE( cSet.PopFront( &nValue ) );
	EXPECT_FALSE( cSet.First( &nValue ) );
	EXPECT_FALSE( cSet.Last( &nValue ) );
	EXPECT_EQ( 42, nValue );

	cSet.Add( 5 ).Add( 1 ).Add( 9 ).Add( 3 );
	ASSERT_TRUE( cSet.Pop( &nValue ) );
	EXPECT_EQ( 9, nValue );
	ASSERT_TRUE( cSet.PopFront( &nValue ) );
	EXPECT_EQ( 1, nValue );
	EXPECT_TRUE( cSet.Pop() );
	EXPECT_TRUE( cSet.PopFront() );
	EXPECT_TRUE( cSet.IsEmpty() );
}

TEST( Set, EraseRemoveIfForEach )
{
	Set<int> cSet;
	for( int i = 0; i < 10; i++ )
		cSet.Add( i );
	EXPECT_EQ( 5, cSet.RemoveIf( IsOdd ) );
	EXPECT_EQ( 5, cSet.Count() );
	Set<int>::iterator i = cSet.Erase( cSet.LowerBound( 4 ) );
	EXPECT_EQ( 6, *i );
	std::vector<int> cKeys;
	for( Set<int>::const_iterator j = cSet.begin(); j != cSet.end(); ++j )
		cKeys.push_back( *j );
	ASSERT_EQ( 4u, cKeys.size() );
	EXPECT_EQ( 0, cKeys[0] );
	EXPECT_EQ( 8, cKeys[3] );

	Words cWords = Of<Words>( "cat,ant,bee" );
	std::string cAll;
	cWords.ForEach( Concat( &cAll ) );
	EXPECT_EQ( std::string( "antbeecat" ), cAll );
}

TEST( Set, ReverseAndSort )
{
	Words cWords = Of<Words>( "pear,fig,banana,kiwi" );
	EXPECT_EQ( std::string( "banana,fig,kiwi,pear" ), Joined( cWords ) );
	cWords.Reverse();
	EXPECT_TRUE( cWords.IsReversed() );
	EXPECT_EQ( std::string( "pear,kiwi,fig,banana" ), Joined( cWords ) );
	cWords.Add( "apple" );			// goes in its place in the new order
	EXPECT_EQ( std::string( "pear,kiwi,fig,banana,apple" ), Joined( cWords ) );
	std::string cLast;
	cWords.Pop( &cLast );
	EXPECT_EQ( std::string( "apple" ), cLast );

	EXPECT_FALSE( cWords.Sort( ByLength ) );	// pear and kiwi are as long
	EXPECT_EQ( std::string( "pear,kiwi,fig,banana" ), Joined( cWords ) );
	cWords.Remove( "kiwi" );
	ASSERT_TRUE( cWords.Sort( ByLength, true ) );
	EXPECT_EQ( std::string( "banana,pear,fig" ), Joined( cWords ) );
	cWords.Add( "plum" ).Add( "a" );		// plum is as long as pear: not added
	EXPECT_EQ( std::string( "banana,pear,fig,a" ), Joined( cWords ) );
	ASSERT_TRUE( cWords.Sort() );
	EXPECT_EQ( std::string( "a,banana,fig,pear" ), Joined( cWords ) );
}

TEST( Set, Bounds )
{
	Set<int> cSet;
	cSet.Add( 10 ).Add( 20 ).Add( 30 );
	EXPECT_EQ( 20, *cSet.LowerBound( 20 ) );
	EXPECT_EQ( 30, *cSet.UpperBound( 20 ) );
	EXPECT_EQ( 20, *cSet.LowerBound( 15 ) );
	EXPECT_TRUE( cSet.UpperBound( 30 ) == cSet.end() );
	const Set<int>& cConst = cSet;
	EXPECT_EQ( 10, *cConst.LowerBound( 0 ) );
}

TEST( Set, Operations )
{
	Words cA = Of<Words>( "a,b,c,d" ), cB = Of<Words>( "c,d,e" );
	EXPECT_EQ( std::string( "a,b,c,d,e" ), Joined( cA.Union( cB ) ) );
	EXPECT_EQ( std::string( "c,d" ), Joined( cA.Intersection( cB ) ) );
	EXPECT_EQ( std::string( "a,b" ), Joined( cA.Difference( cB ) ) );
	EXPECT_EQ( std::string( "e" ), Joined( cB.Difference( cA ) ) );
	EXPECT_EQ( std::string( "a,b,e" ), Joined( cA.SymmetricDifference( cB ) ) );
	EXPECT_EQ( std::string( "a,b,c,d" ), Joined( cA ) );	// left alone

	Words cC = cA;
	cC.Merge( cB );
	EXPECT_EQ( std::string( "a,b,c,d,e" ), Joined( cC ) );
	cC.Intersect( Of<Words>( "b,e,z" ) );
	EXPECT_EQ( std::string( "b,e" ), Joined( cC ) );
	cC = cA;
	cC.Subtract( cB ).Add( "q" );
	EXPECT_EQ( std::string( "a,b,q" ), Joined( cC ) );

	EXPECT_TRUE( Of<Words>( "b,c" ).IsSubsetOf( cA ) );
	EXPECT_FALSE( cB.IsSubsetOf( cA ) );
	EXPECT_TRUE( cA.IsSubsetOf( cA ) );
	EXPECT_TRUE( Words().IsSubsetOf( cA ) );
	EXPECT_TRUE( cA.IsSupersetOf( Of<Words>( "a,d" ) ) );
	EXPECT_TRUE( cA.Intersects( cB ) );
	EXPECT_FALSE( cA.Intersects( Of<Words>( "x,y" ) ) );
	EXPECT_FALSE( cA.Intersects( Words() ) );
}

TEST( Set, OperationsKeepThisOrder )
{
	Words cA = Of<Words>( "a,b,c" ), cB = Of<Words>( "b,c,d" );
	cA.Reverse();
	EXPECT_EQ( std::string( "c,b" ), Joined( cA.Intersection( cB ) ) );
	EXPECT_EQ( std::string( "d,c,b,a" ), Joined( cA.Union( cB ) ) );
	EXPECT_EQ( std::string( "a" ), Joined( cA.Difference( cB ) ) );
	EXPECT_TRUE( cA.Difference( cB ).IsReversed() );
	EXPECT_EQ( std::string( "d,a" ), Joined( cA.SymmetricDifference( cB ) ) );
}

TEST( Set, EqualityIgnoresOrder )
{
	Words cA = Of<Words>( "x,y,z" ), cB = Of<Words>( "z,y,x" );
	cB.Reverse();
	EXPECT_TRUE( cA == cB );
	cB.Add( "w" );
	EXPECT_TRUE( cA != cB );
	cA.Swap( cB );
	EXPECT_EQ( 4, cA.Count() );
	EXPECT_EQ( 3, cB.Count() );
	cA.Clear();
	EXPECT_TRUE( cA.IsEmpty() );
	EXPECT_TRUE( cA == Words() );
	EXPECT_EQ( std::string( "x,y,z" ), JoinedVector( Of<Words>( "z,x,y" ).Keys() ) );
	EXPECT_EQ( 3u, cB.GetContainer().size() );
}

TEST( Set, MatchesListOverRandomSteps )
{
	srand( 99 );
	Set<int> cSet;
	std::list<int> cModel;		// sorted, no duplicates
	for( int nStep = 0; nStep < 20000; nStep++ )
	{
		int nKey = rand() % 500;
		bool bIn = std::find( cModel.begin(), cModel.end(), nKey ) != cModel.end();
		switch( rand() % 5 )
		{
			case 0:
			case 1:
				ASSERT_EQ( !bIn, cSet.Insert( nKey ) ) << "step " << nStep;
				if( !bIn )
				{
					std::list<int>::iterator i = cModel.begin();
					while( i != cModel.end() && *i < nKey )
						++i;
					cModel.insert( i, nKey );
				}
				break;
			case 2:
				ASSERT_EQ( bIn, cSet.Remove( nKey ) ) << "step " << nStep;
				cModel.remove( nKey );
				break;
			case 3:
				ASSERT_EQ( bIn, cSet.Has( nKey ) ) << "step " << nStep;
				break;
			default:
			{
				int nGot = -1;
				bool bGot = rand() % 2 ? cSet.Pop( &nGot ) : cSet.PopFront( &nGot );
				ASSERT_EQ( !cModel.empty(), bGot ) << "step " << nStep;
				if( bGot )
				{
					ASSERT_TRUE( nGot == cModel.back() || nGot == cModel.front() ) << "step " << nStep;
					cModel.remove( nGot );
				}
			}
		}
		ASSERT_EQ( int( cModel.size() ), cSet.Count() ) << "step " << nStep;
	}
	EXPECT_TRUE( std::equal( cModel.begin(), cModel.end(), cSet.begin() ) );
}

// --- os::HashSet -------------------------------------------------------

TEST( HashSet, KeepsTheOrderKeysWentIn )
{
	HashWords cSeen;
	EXPECT_TRUE( cSeen.IsEmpty() );
	cSeen.Add( "b.png" ).Add( "a.png" ).Add( "b.png" ).Add( "c.png" );
	EXPECT_EQ( 3, cSeen.Count() );
	EXPECT_EQ( std::string( "b.png,a.png,c.png" ), Joined( cSeen ) );
	EXPECT_FALSE( cSeen.Insert( "a.png" ) );
	EXPECT_TRUE( cSeen.Insert( "d.png" ) );
	EXPECT_TRUE( cSeen.Has( "d.png" ) );
	EXPECT_FALSE( cSeen.Has( "e.png" ) );
	EXPECT_TRUE( cSeen.Remove( "a.png" ) );
	EXPECT_FALSE( cSeen.Remove( "a.png" ) );
	EXPECT_EQ( std::string( "b.png,c.png,d.png" ), Joined( cSeen ) );
	cSeen.Add( "a.png" );			// comes back at the end
	EXPECT_EQ( std::string( "b.png,c.png,d.png,a.png" ), Joined( cSeen ) );
	EXPECT_EQ( 4u, cSeen.size() );
}

TEST( HashSet, PushPushFrontInsertAt )
{
	HashWords cSet = Of<HashWords>( "a,b,c" );
	cSet.Push( "a" );
	EXPECT_EQ( std::string( "b,c,a" ), Joined( cSet ) );
	cSet.Push( "d" );
	EXPECT_EQ( std::string( "b,c,a,d" ), Joined( cSet ) );
	cSet.PushFront( "d" );
	EXPECT_EQ( std::string( "d,b,c,a" ), Joined( cSet ) );
	cSet.InsertAt( 2, "z" );
	EXPECT_EQ( std::string( "d,b,z,c,a" ), Joined( cSet ) );
	cSet.InsertAt( 99, "b" );
	EXPECT_EQ( std::string( "d,z,c,a,b" ), Joined( cSet ) );
	EXPECT_EQ( std::string( "c" ), cSet.KeyAt( 2 ) );
	EXPECT_EQ( 4, cSet.IndexOf( "b" ) );
	EXPECT_EQ( -1, cSet.IndexOf( "nope" ) );
	cSet.Remove( "d" );
	EXPECT_EQ( 3, cSet.IndexOf( "b" ) );		// with a hole before it
	EXPECT_EQ( std::string( "z" ), cSet.KeyAt( 0 ) );
}

TEST( HashSet, PopFirstLastAndEmpty )
{
	HashSet<int> cSet;
	int nValue = 42;
	EXPECT_FALSE( cSet.Pop( &nValue ) );
	EXPECT_FALSE( cSet.PopFront( &nValue ) );
	EXPECT_FALSE( cSet.First( &nValue ) );
	EXPECT_FALSE( cSet.Last( &nValue ) );
	EXPECT_EQ( 42, nValue );

	cSet.Add( 5 ).Add( 1 ).Add( 9 ).Add( 3 );
	ASSERT_TRUE( cSet.First( &nValue ) );
	EXPECT_EQ( 5, nValue );
	ASSERT_TRUE( cSet.Last( &nValue ) );
	EXPECT_EQ( 3, nValue );
	ASSERT_TRUE( cSet.Pop( &nValue ) );
	EXPECT_EQ( 3, nValue );
	ASSERT_TRUE( cSet.PopFront( &nValue ) );
	EXPECT_EQ( 5, nValue );
	EXPECT_TRUE( cSet.Pop() );
	EXPECT_TRUE( cSet.PopFront() );
	EXPECT_TRUE( cSet.IsEmpty() );
}

TEST( HashSet, FindAndTakeGiveTheKeyThatIsIn )
{
	HashSet<Font, FontHash> cFonts;
	cFonts.Add( Font( "sans", 10 ) ).Add( Font( "mono", 12 ) );
	cFonts.Add( Font( "sans", 99 ) );
	const Font* pcFont = cFonts.Find( Font( "sans" ) );
	ASSERT_TRUE( pcFont != NULL );
	EXPECT_EQ( 10, pcFont->m_nSize );
	EXPECT_TRUE( cFonts.Find( Font( "serif" ) ) == NULL );

	Font cTaken;
	EXPECT_TRUE( cFonts.Take( Font( "mono" ), &cTaken ) );
	EXPECT_EQ( 12, cTaken.m_nSize );
	EXPECT_FALSE( cFonts.Take( Font( "mono" ), &cTaken ) );
	EXPECT_FALSE( cFonts.Take( Font( "mono" ), NULL ) );
	EXPECT_TRUE( cFonts.Take( Font( "sans" ), NULL ) );
	EXPECT_TRUE( cFonts.IsEmpty() );
}

TEST( HashSet, EraseRemoveIfForEach )
{
	HashWords cSet = Of<HashWords>( "bee,ant,bat,cat,bug" );
	EXPECT_EQ( 3, cSet.RemoveIf( StartsWithB() ) );
	EXPECT_EQ( std::string( "ant,cat" ), Joined( cSet ) );

	cSet.Add( "dog" ).Add( "eel" );
	HashWords::iterator i = cSet.begin();
	++i;
	i = cSet.Erase( i );			// cat
	EXPECT_EQ( std::string( "dog" ), *i );
	EXPECT_EQ( 3u, i->size() );
	i = cSet.Erase( i );			// the others stay valid
	EXPECT_EQ( std::string( "eel" ), *i );
	EXPECT_TRUE( cSet.Erase( i ) == cSet.end() );
	EXPECT_EQ( std::string( "ant" ), Joined( cSet ) );

	cSet.Add( "fox" ).Add( "gnu" );
	std::string cAll;
	cSet.ForEach( Concat( &cAll ) );
	EXPECT_EQ( std::string( "antfoxgnu" ), cAll );
}

TEST( HashSet, IteratesBothWays )
{
	HashSet<int> cSet;
	for( int i = 0; i < 6; i++ )
		cSet.Add( i * 10 );
	cSet.Remove( 20 );
	std::vector<int> cBack( cSet.rbegin(), cSet.rend() );
	ASSERT_EQ( 5u, cBack.size() );
	EXPECT_EQ( 50, cBack[0] );
	EXPECT_EQ( 40, cBack[1] );
	EXPECT_EQ( 30, cBack[2] );
	EXPECT_EQ( 10, cBack[3] );
	EXPECT_EQ( 0, cBack[4] );
	HashSet<int>::const_iterator i = cSet.end();
	--i;
	EXPECT_EQ( 50, *i );
	i--;
	EXPECT_EQ( 40, *i );
}

TEST( HashSet, ReverseAndSort )
{
	HashWords cWords = Of<HashWords>( "pear,fig,banana,kiwi" );
	cWords.Reverse();
	EXPECT_EQ( std::string( "kiwi,banana,fig,pear" ), Joined( cWords ) );
	cWords.Sort();
	EXPECT_EQ( std::string( "banana,fig,kiwi,pear" ), Joined( cWords ) );
	cWords.Sort( NULL, true );
	EXPECT_EQ( std::string( "pear,kiwi,fig,banana" ), Joined( cWords ) );
	cWords.Sort( ByLength );
	EXPECT_EQ( 3u, cWords.KeyAt( 0 ).size() );
	EXPECT_EQ( std::string( "banana" ), cWords.KeyAt( 3 ) );
	cWords.Add( "apple" );			// after a sort, at the end
	EXPECT_EQ( std::string( "apple" ), cWords.KeyAt( 4 ) );
}

TEST( HashSet, Operations )
{
	HashWords cA = Of<HashWords>( "d,a,c,b" ), cB = Of<HashWords>( "e,c,d" );
	EXPECT_EQ( std::string( "d,a,c,b,e" ), Joined( cA.Union( cB ) ) );
	EXPECT_EQ( std::string( "d,c" ), Joined( cA.Intersection( cB ) ) );
	EXPECT_EQ( std::string( "c,d" ), Joined( cB.Intersection( cA ) ) );
	EXPECT_EQ( std::string( "a,b" ), Joined( cA.Difference( cB ) ) );
	EXPECT_EQ( std::string( "e" ), Joined( cB.Difference( cA ) ) );
	EXPECT_EQ( std::string( "a,b,e" ), Joined( cA.SymmetricDifference( cB ) ) );
	EXPECT_EQ( std::string( "d,a,c,b" ), Joined( cA ) );

	HashWords cC = cA;
	cC.Merge( cB );
	EXPECT_EQ( std::string( "d,a,c,b,e" ), Joined( cC ) );
	cC.Intersect( Of<HashWords>( "z,e,a" ) );
	EXPECT_EQ( std::string( "a,e" ), Joined( cC ) );

	// Subtract() goes both ways: by the other set's keys when it's smaller
	cC = cA;
	cC.Subtract( Of<HashWords>( "c" ) );
	EXPECT_EQ( std::string( "d,a,b" ), Joined( cC ) );
	cC.Subtract( Of<HashWords>( "a,x,y,z,w" ) ).Add( "q" );
	EXPECT_EQ( std::string( "d,b,q" ), Joined( cC ) );

	EXPECT_TRUE( Of<HashWords>( "b,c" ).IsSubsetOf( cA ) );
	EXPECT_FALSE( cB.IsSubsetOf( cA ) );
	EXPECT_TRUE( HashWords().IsSubsetOf( cA ) );
	EXPECT_TRUE( cA.IsSupersetOf( Of<HashWords>( "a,d" ) ) );
	EXPECT_TRUE( cA.Intersects( cB ) );
	EXPECT_TRUE( cB.Intersects( cA ) );
	EXPECT_FALSE( cA.Intersects( Of<HashWords>( "x,y" ) ) );
}

TEST( HashSet, EqualityIgnoresOrder )
{
	HashWords cA = Of<HashWords>( "x,y,z" ), cB = Of<HashWords>( "z,y,x" );
	EXPECT_TRUE( cA == cB );
	EXPECT_TRUE( cA.Keys() != cB.Keys() );
	cB.Add( "w" );
	EXPECT_TRUE( cA != cB );
	cA.Swap( cB );
	EXPECT_EQ( 4, cA.Count() );
	EXPECT_EQ( 3, cB.Count() );
	EXPECT_EQ( std::string( "z,y,x,w" ), JoinedVector( cA.Keys() ) );
	cA.Clear();
	EXPECT_TRUE( cA.IsEmpty() );
	EXPECT_TRUE( cA.begin() == cA.end() );
	cA.Reserve( 1000 );
	EXPECT_TRUE( cA.GetCapacity() >= 1000u );
	cA.Add( "v" );
	EXPECT_EQ( std::string( "v" ), Joined( cA ) );
}

TEST( HashSet, ManyKeysWithRemovals )
{
	HashSet<int> cSet;
	for( int i = 0; i < 50000; i++ )
		cSet.Add( i );
	for( int i = 0; i < 50000; i += 3 )
		cSet.Remove( i );		// enough holes to compact
	EXPECT_EQ( 33333, cSet.Count() );
	int nExpected = 1;
	bool bInOrder = true;
	for( HashSet<int>::const_iterator i = cSet.begin(); i != cSet.end(); ++i )
	{
		bInOrder = bInOrder && *i == nExpected;
		nExpected += nExpected % 3 == 1 ? 1 : 2;
	}
	EXPECT_TRUE( bInOrder );
	EXPECT_TRUE( cSet.Has( 49999 ) );
	EXPECT_FALSE( cSet.Has( 49998 ) );
}

TEST( HashSet, MatchesListOverRandomSteps )
{
	srand( 77 );
	HashSet<int> cSet;
	std::list<int> cModel;		// in order, no duplicates
	for( int nStep = 0; nStep < 20000; nStep++ )
	{
		int nKey = rand() % 300;
		bool bIn = std::find( cModel.begin(), cModel.end(), nKey ) != cModel.end();
		switch( rand() % 7 )
		{
			case 0:
			case 1:
				ASSERT_EQ( !bIn, cSet.Insert( nKey ) ) << "step " << nStep;
				if( !bIn )
					cModel.push_back( nKey );
				break;
			case 2:
				cSet.Push( nKey );
				cModel.remove( nKey );
				cModel.push_back( nKey );
				break;
			case 3:
				ASSERT_EQ( bIn, cSet.Remove( nKey ) ) << "step " << nStep;
				cModel.remove( nKey );
				break;
			case 4:
				ASSERT_EQ( bIn, cSet.Has( nKey ) ) << "step " << nStep;
				break;
			case 5:
				if( rand() % 20 == 0 )
				{
					cSet.PushFront( nKey );
					cModel.remove( nKey );
					cModel.push_front( nKey );
				}
				break;
			default:
			{
				int nGot = -1;
				bool bBack = rand() % 2 != 0;
				bool bGot = bBack ? cSet.Pop( &nGot ) : cSet.PopFront( &nGot );
				ASSERT_EQ( !cModel.empty(), bGot ) << "step " << nStep;
				if( bGot )
				{
					ASSERT_EQ( bBack ? cModel.back() : cModel.front(), nGot ) << "step " << nStep;
					if( bBack )
						cModel.pop_back();
					else
						cModel.pop_front();
				}
			}
		}
		ASSERT_EQ( int( cModel.size() ), cSet.Count() ) << "step " << nStep;
	}
	ASSERT_EQ( cModel.size(), cSet.size() );
	EXPECT_TRUE( std::equal( cModel.begin(), cModel.end(), cSet.begin() ) );
}

#if __cplusplus >= 201103L
TEST( SetHashSet, RangeFor )
{
	Set<int> cSet;
	HashSet<int> cHashSet;
	for( int i : { 3, 1, 2 } )
	{
		cSet.Add( i );
		cHashSet.Add( i );
	}
	std::string cOrder;
	for( int n : cSet )
		cOrder += char( '0' + n );
	cOrder += '/';
	for( int n : cHashSet )
		cOrder += char( '0' + n );
	EXPECT_EQ( std::string( "123/312" ), cOrder );
}
#endif
