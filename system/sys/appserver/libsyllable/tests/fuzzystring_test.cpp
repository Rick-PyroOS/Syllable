/*  os::FuzzyString: fuzzy matching and its scores, positions and
 *  highlighting, fzf's extended syntax, the case modes, searching and
 *  ranking, copies, and texts at the edges (empty, all spaces, long, UTF-8).
 */

#include <gtest/gtest.h>

#include <util/fuzzystring.h>
#include <util/string.h>

#include <string>
#include <vector>
#include <stdio.h>
#include <string.h>

using namespace os;

namespace
{

std::string Joined( const std::vector<int>& cPositions )
{
	std::string cText;
	for( size_t i = 0; i < cPositions.size(); i++ )
	{
		char zNumber[16];
		snprintf( zNumber, sizeof( zNumber ), "%s%d", i ? "," : "", cPositions[i] );
		cText += zNumber;
	}
	return cText;
}

std::vector<String> Texts( const char* const* ppzTexts )
{
	std::vector<String> cTexts;
	for( ; *ppzTexts != NULL; ppzTexts++ )
		cTexts.push_back( *ppzTexts );
	return cTexts;
}

} // namespace

TEST( FuzzyString, MatchesCharactersInOrder )
{
	FuzzyString cFind( "fstr" );
	EXPECT_TRUE( cFind.Matches( "FuzzyString.cpp" ) );
	EXPECT_TRUE( cFind.Matches( "fast_reader" ) );
	EXPECT_TRUE( cFind.Matches( "fstr" ) );
	EXPECT_FALSE( cFind.Matches( "strf" ) );	// out of order
	EXPECT_FALSE( cFind.Matches( "fst" ) );		// one missing
	EXPECT_FALSE( cFind.Matches( "" ) );
	EXPECT_EQ( cFind.GetScore( "xyz" ), -1 );
	EXPECT_FALSE( cFind.IsEmpty() );
	EXPECT_TRUE( cFind.GetPattern() == "fstr" );
}

TEST( FuzzyString, BetterMatchesScoreHigher )
{
	FuzzyString cFind( "fstr" );
	// word starts and camelCase humps beat letters inside words
	EXPECT_LT( cFind.GetScore( "fast_reader" ), cFind.GetScore( "FuzzyString" ) );
	// the whole text beats a part of it
	EXPECT_LT( cFind.GetScore( "FuzzyString" ), cFind.GetScore( "fstr" ) );
	// characters together beat characters apart
	FuzzyString cJson( "json" );
	EXPECT_LT( cJson.GetScore( "j_s_o_n.cpp" ), cJson.GetScore( "json.cpp" ) );
	// a gap costs: a long one more than a short one
	EXPECT_LT( FuzzyString::Score( "ab", "a----------b" ), FuzzyString::Score( "ab", "a--b" ) );
	EXPECT_EQ( FuzzyString::Score( "ab", "ba" ), -1 );
}

TEST( FuzzyString, PositionsAndHighlight )
{
	FuzzyString cFind( "fstr" );
	EXPECT_EQ( Joined( cFind.GetPositions( "FuzzyString.cpp" ) ), "0,5,6,7" );
	EXPECT_TRUE( cFind.Highlight( "FuzzyString.cpp", "<b>", "</b>" ) == "<b>F</b>uzzy<b>Str</b>ing.cpp" );
	EXPECT_TRUE( cFind.Highlight( "fstr", "[", "]" ) == "[fstr]" );
	EXPECT_TRUE( cFind.Highlight( "nothing", "[", "]" ) == "nothing" );
	EXPECT_TRUE( cFind.Highlight( "FuzzyString", NULL, "|" ) == "F|uzzyStr|ing" );
	EXPECT_TRUE( cFind.GetPositions( "nothing" ).empty() );

	int nScore = -2;
	std::vector<int> cPositions( 3, 99 );
	EXPECT_TRUE( cFind.Match( "FuzzyString", &nScore, &cPositions ) );
	EXPECT_EQ( nScore, cFind.GetScore( "FuzzyString" ) );
	EXPECT_EQ( Joined( cPositions ), "0,5,6,7" );
	EXPECT_FALSE( cFind.Match( "nothing", &nScore, &cPositions ) );
	EXPECT_TRUE( cPositions.empty() );

	// several words: every word's positions, sorted
	FuzzyString cTwo( "cpp fuz" );
	EXPECT_EQ( Joined( cTwo.GetPositions( "FuzzyString.cpp" ) ), "0,1,2,12,13,14" );
	EXPECT_TRUE( cTwo.Highlight( "FuzzyString.cpp", "<", ">" ) == "<Fuz>zyString.<cpp>" );
}

TEST( FuzzyString, ExtendedSyntax )
{
	// all words must match, in any order
	FuzzyString cBoth( "cpp fuzzy" );
	EXPECT_TRUE( cBoth.Matches( "FuzzyString.cpp" ) );
	EXPECT_FALSE( cBoth.Matches( "FuzzyString.h" ) );

	// 'exact
	FuzzyString cExact( "'zzy" );
	EXPECT_TRUE( cExact.Matches( "fuzzy" ) );
	EXPECT_FALSE( cExact.Matches( "fzuzy" ) );
	EXPECT_TRUE( FuzzyString( "zzy" ).Matches( "zaza_y" ) );
	EXPECT_FALSE( cExact.Matches( "zaza_y" ) );

	// ^prefix, suffix$, ^whole$
	FuzzyString cPrefix( "^fu" );
	EXPECT_TRUE( cPrefix.Matches( "fuzzy" ) );
	EXPECT_FALSE( cPrefix.Matches( "afuzzy" ) );
	FuzzyString cSuffix( ".h$" );
	EXPECT_TRUE( cSuffix.Matches( "string.h" ) );
	EXPECT_FALSE( cSuffix.Matches( "string.hh" ) );
	EXPECT_EQ( Joined( cSuffix.GetPositions( "a.h" ) ), "1,2" );
	FuzzyString cWhole( "^main.cpp$" );
	EXPECT_TRUE( cWhole.Matches( "main.cpp" ) );
	EXPECT_FALSE( cWhole.Matches( "main.cpp.orig" ) );
	EXPECT_FALSE( cWhole.Matches( "old_main.cpp" ) );

	// !not: never in the text; scores nothing, has no positions
	FuzzyString cNot( "!test" );
	EXPECT_TRUE( cNot.Matches( "main.cpp" ) );
	EXPECT_FALSE( cNot.Matches( "main_test.cpp" ) );
	EXPECT_EQ( cNot.GetScore( "main.cpp" ), 0 );
	EXPECT_TRUE( cNot.GetPositions( "main.cpp" ).empty() );
	FuzzyString cCppNotTest( "cpp !test" );
	EXPECT_TRUE( cCppNotTest.Matches( "main.cpp" ) );
	EXPECT_FALSE( cCppNotTest.Matches( "main_test.cpp" ) );
	EXPECT_FALSE( cCppNotTest.Matches( "main.h" ) );
	EXPECT_EQ( cCppNotTest.GetScore( "main.cpp" ), FuzzyString( "cpp" ).GetScore( "main.cpp" ) );
	EXPECT_FALSE( FuzzyString( "!^old" ).Matches( "old_main.cpp" ) );
	EXPECT_TRUE( FuzzyString( "!^old" ).Matches( "main_old.cpp" ) );

	// a | b
	FuzzyString cEither( "json | xml" );
	EXPECT_TRUE( cEither.Matches( "json.cpp" ) );
	EXPECT_TRUE( cEither.Matches( "xml.cpp" ) );
	EXPECT_FALSE( cEither.Matches( "hash.cpp" ) );
	FuzzyString cEitherAnd( "json | xml cpp" );
	EXPECT_TRUE( cEitherAnd.Matches( "xml.cpp" ) );
	EXPECT_FALSE( cEitherAnd.Matches( "xml.h" ) );

	// "\ " is a space in a word
	FuzzyString cSpace( "'a\\ b" );
	EXPECT_TRUE( cSpace.Matches( "a b" ) );
	EXPECT_FALSE( cSpace.Matches( "ab" ) );
	EXPECT_FALSE( cSpace.Matches( "a_b" ) );
}

TEST( FuzzyString, CaseModes )
{
	// smart: lowercase matches both, a capital only itself
	EXPECT_TRUE( FuzzyString( "fu" ).Matches( "Fuzzy" ) );
	EXPECT_TRUE( FuzzyString( "Fu" ).Matches( "Fuzzy" ) );
	EXPECT_FALSE( FuzzyString( "Fu" ).Matches( "fuzzy" ) );
	// ignore
	EXPECT_TRUE( FuzzyString( "Fu", FuzzyString::CASE_IGNORE ).Matches( "fuzzy" ) );
	EXPECT_TRUE( FuzzyString( "FU", FuzzyString::CASE_IGNORE ).Matches( "fuzzy" ) );
	// respect
	EXPECT_FALSE( FuzzyString( "fu", FuzzyString::CASE_RESPECT ).Matches( "Fuzzy" ) );
	EXPECT_TRUE( FuzzyString( "Fu", FuzzyString::CASE_RESPECT ).Matches( "Fuzzy" ) );

	FuzzyString cFind( "Fu" );
	EXPECT_EQ( cFind.GetCaseMode(), FuzzyString::CASE_SMART );
	cFind.SetCaseMode( FuzzyString::CASE_IGNORE );
	EXPECT_EQ( cFind.GetCaseMode(), FuzzyString::CASE_IGNORE );
	EXPECT_TRUE( cFind.Matches( "fuzzy" ) );
}

TEST( FuzzyString, ExactMode )
{
	FuzzyString cFind( "zzy", FuzzyString::CASE_SMART, FuzzyString::MATCH_EXACT );
	EXPECT_EQ( cFind.GetMatchMode(), FuzzyString::MATCH_EXACT );
	EXPECT_TRUE( cFind.Matches( "fuzzy" ) );
	EXPECT_FALSE( cFind.Matches( "zaza_y" ) );
	// 'word is fuzzy then
	cFind.SetPattern( "'zzy" );
	EXPECT_TRUE( cFind.Matches( "zaza_y" ) );
	cFind.SetMatchMode( FuzzyString::MATCH_FUZZY );
	EXPECT_FALSE( cFind.Matches( "zaza_y" ) );
}

TEST( FuzzyString, EmptyPatternMatchesEverything )
{
	const char* apzPatterns[] = { "", "   ", "!", "^", "'" };
	for( size_t i = 0; i < sizeof( apzPatterns ) / sizeof( apzPatterns[0] ); i++ )
	{
		FuzzyString cFind( apzPatterns[i] );
		EXPECT_TRUE( cFind.IsEmpty() ) << apzPatterns[i];
		EXPECT_TRUE( cFind.Matches( "anything" ) ) << apzPatterns[i];
		EXPECT_TRUE( cFind.Matches( "" ) ) << apzPatterns[i];
		EXPECT_EQ( cFind.GetScore( "anything" ), 0 ) << apzPatterns[i];
		EXPECT_TRUE( cFind.GetPositions( "anything" ).empty() ) << apzPatterns[i];
		EXPECT_TRUE( cFind.Highlight( "anything", "<", ">" ) == "anything" ) << apzPatterns[i];
	}
	FuzzyString cDefault;
	EXPECT_TRUE( cDefault.IsEmpty() );
	cDefault.SetPattern( "x" );
	EXPECT_FALSE( cDefault.IsEmpty() );
	EXPECT_FALSE( cDefault.Matches( "anything" ) );
}

TEST( FuzzyString, SearchRanksBestFirst )
{
	const char* apzFiles[] = { "util/hash.cpp", "util/fuzzystring.cpp", "gui/font.cpp",
				   "tests/fuzzystring_test.cpp", "util/fuzzystring.h", "README", NULL };
	std::vector<String> cFiles = Texts( apzFiles );

	FuzzyString cFind( "fuzzy" );
	std::vector<FuzzyString::Result> cResults = cFind.Search( cFiles );
	ASSERT_EQ( cResults.size(), size_t( 3 ) );
	for( size_t i = 0; i < cResults.size(); i++ )
	{
		EXPECT_EQ( cResults[i].nScore, cFind.GetScore( cFiles[cResults[i].nIndex] ) );
		if( i > 0 )
		{
			EXPECT_GE( cResults[i - 1].nScore, cResults[i].nScore );
		}
	}
	// equal scores: the shorter text first
	EXPECT_EQ( cResults[0].nIndex, 4 );	// util/fuzzystring.h
	EXPECT_EQ( cResults[1].nIndex, 1 );	// util/fuzzystring.cpp

	std::vector<FuzzyString::Result> cTop = cFind.Search( cFiles, 2 );
	ASSERT_EQ( cTop.size(), size_t( 2 ) );
	EXPECT_EQ( cTop[0].nIndex, 4 );
	EXPECT_EQ( cTop[1].nIndex, 1 );
	EXPECT_EQ( cFind.Search( cFiles, 10 ).size(), size_t( 3 ) );

	// equal scores and lengths: the first one first
	const char* apzSame[] = { "b_x", "a_x", "c_x", NULL };
	std::vector<FuzzyString::Result> cSame = FuzzyString( "x" ).Search( Texts( apzSame ) );
	ASSERT_EQ( cSame.size(), size_t( 3 ) );
	EXPECT_EQ( cSame[0].nIndex, 0 );
	EXPECT_EQ( cSame[1].nIndex, 1 );
	EXPECT_EQ( cSame[2].nIndex, 2 );

	EXPECT_TRUE( FuzzyString( "qqq" ).Search( cFiles ).empty() );
	EXPECT_EQ( FuzzyString( "" ).Search( cFiles ).size(), cFiles.size() );
	EXPECT_TRUE( cFind.Search( std::vector<String>() ).empty() );
}

TEST( FuzzyString, CopiesAreIndependent )
{
	FuzzyString cFind( "abc", FuzzyString::CASE_RESPECT );
	FuzzyString cCopy( cFind );
	EXPECT_TRUE( cCopy.GetPattern() == "abc" );
	EXPECT_EQ( cCopy.GetCaseMode(), FuzzyString::CASE_RESPECT );
	cCopy.SetPattern( "xyz" );
	EXPECT_TRUE( cFind.Matches( "a_b_c" ) );
	EXPECT_FALSE( cFind.Matches( "x_y_z" ) );
	EXPECT_TRUE( cCopy.Matches( "x_y_z" ) );

	FuzzyString cAssigned;
	cAssigned = cFind;
	EXPECT_TRUE( cAssigned.Matches( "a_b_c" ) );
	EXPECT_FALSE( cAssigned.Matches( "A_B_C" ) );
	cAssigned = cAssigned;
	EXPECT_TRUE( cAssigned.Matches( "a_b_c" ) );
}

TEST( FuzzyString, EdgeTexts )
{
	// empty and all-space texts against every kind of word (fzf's suffix
	// and whole-text matches read before an empty text's start)
	const char* apzPatterns[] = { "a", "'a", "^a", "a$", "^a$", "!a", "!a$", "!^a$", " $", "^ ", NULL };
	// (40 spaces: on the heap, not in the string, so a read before it
	// is caught by the address sanitizer)
	const char* apzTexts[] = { "", " ", "    ", "                                        ", "a", " a ", NULL };
	for( const char* const* p = apzPatterns; *p != NULL; p++ )
		for( const char* const* t = apzTexts; *t != NULL; t++ )
		{
			FuzzyString cFind( *p );
			int nScore = -2;
			std::vector<int> cPositions;
			bool bMatch = cFind.Match( *t, &nScore, &cPositions );
			EXPECT_EQ( bMatch, cFind.Matches( *t ) ) << *p << " / '" << *t << "'";
			EXPECT_EQ( bMatch ? nScore : -1, cFind.GetScore( *t ) ) << *p << " / '" << *t << "'";
			for( size_t i = 0; i < cPositions.size(); i++ )
				EXPECT_LT( cPositions[i], int( strlen( *t ) ) ) << *p << " / '" << *t << "'";
		}
	EXPECT_FALSE( FuzzyString( "a$" ).Matches( "" ) );
	EXPECT_FALSE( FuzzyString( ".cpp$" ).Matches( "a.h" ) );	// shorter than the suffix
	EXPECT_TRUE( FuzzyString( "!a$" ).Matches( "" ) );
	EXPECT_TRUE( FuzzyString( "^a$" ).Matches( " a " ) );	// fzf trims the text's spaces for ^ and $

	// longer than the scratch buffer: still matches, and finds the end
	std::string cLong( 5000, 'x' );
	cLong[0] = 'a';
	cLong += "bc";
	FuzzyString cFind( "abc" );
	EXPECT_TRUE( cFind.Matches( cLong ) );
	EXPECT_GE( cFind.GetScore( cLong ), 0 );
	EXPECT_EQ( Joined( cFind.GetPositions( cLong ) ), "0,5000,5001" );
	std::string cLongPattern( 300, 'x' );
	EXPECT_TRUE( FuzzyString( cLongPattern ).Matches( cLong ) );
	EXPECT_FALSE( FuzzyString( cLongPattern + "q" ).Matches( cLong ) );
}

TEST( FuzzyString, Utf8IsBytes )
{
	// "é" is two bytes, matched as they are; positions are byte offsets
	FuzzyString cFind( "caf\xc3\xa9" );
	EXPECT_TRUE( cFind.Matches( "Caf\xc3\xa9 au lait" ) );
	EXPECT_FALSE( cFind.Matches( "Cafe au lait" ) );
	EXPECT_EQ( Joined( cFind.GetPositions( "caf\xc3\xa9" ) ), "0,1,2,3,4" );
	EXPECT_TRUE( FuzzyString( "\xc3\xa9t\xc3\xa9" ).Highlight( "l'\xc3\xa9t\xc3\xa9", "[", "]" ) == "l'[\xc3\xa9t\xc3\xa9]" );
}
