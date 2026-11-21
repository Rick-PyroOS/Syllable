/*  FuzzyString - fuzzy text search, as fzf does it.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of version 2 of the GNU Library
 *  General Public License as published by the Free Software
 *  Foundation.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 */

#ifndef	__F_UTIL_FUZZYSTRING_H__
#define	__F_UTIL_FUZZYSTRING_H__

#include <util/string.h>
#include <vector>
#include <stddef.h>

namespace os
{

/** A search pattern that matches texts fuzzily, as the fzf finder does.
 * \ingroup util
 * \par Description:
 *	A FuzzyString is a pattern, such as what a user types into a search
 *	field, matched against texts (file names, menu items, commands...).
 *	"fstr" matches "FuzzyString.cpp": its characters are in the text in
 *	that order, with anything between them.  Each match gets a score:
 *	the higher, the better the match.  Characters at the start of words,
 *	of camelCase humps and after "/", "_", "-" or "." score higher, as do
 *	characters that follow each other, and gaps score lower, so
 *	"fstr" scores higher in "FuzzyString" than in "fast_reader".
 *	It is fzf's own algorithm, in C (telescope-fzf-native), so it ranks
 *	as fzf, Telescope and the like do.
 * \code
 *	os::FuzzyString cFind( "fstr" );
 *	if( cFind.Matches( "FuzzyString.cpp" ) )
 *	    printf( "%d\n", cFind.GetScore( "FuzzyString.cpp" ) );
 *
 *	std::vector<os::String> cFiles;		// file names, menu items...
 *	std::vector<os::FuzzyString::Result> cBest = cFind.Search( cFiles, 10 );
 *	for( size_t i = 0; i < cBest.size(); i++ )	// the 10 best, best first
 *	    printf( "%5d %s\n", cBest[i].nScore, cFiles[cBest[i].nIndex].c_str() );
 *
 *	// the matched characters, for highlighting: "<b>F</b>uzzy<b>Str</b>ing.cpp"
 *	os::String cMarked = cFind.Highlight( "FuzzyString.cpp", "<b>", "</b>" );
 * \endcode
 *
 *	The pattern is fzf's extended search syntax: words separated by
 *	spaces must all match ("fuzzy cpp"), and a word can be
 *	- 'word: an exact match (the characters together)
 *	- ^word: a prefix of the text
 *	- word$: a suffix of the text (^word$: the whole text)
 *	- !word: must not be in the text (!^word, !word$: not a prefix, suffix)
 *	- word | other: either of the two (or more, "a | b | c")
 *
 *	"\ " is a space inside a word.  An empty pattern, or one of spaces,
 *	matches every text with a score of 0.  With MATCH_EXACT, words are
 *	exact matches, and 'word a fuzzy one (fzf --exact).
 *
 *	Case: with CASE_SMART (the default), a word in lowercase matches
 *	either case, and a word with a capital letter in it only its own
 *	case.  CASE_IGNORE and CASE_RESPECT always ignore, or always respect,
 *	the case.
 *
 *	Texts are bytes: lowercase and uppercase are ASCII's, and the
 *	positions of GetPositions() and Highlight() are byte offsets into the
 *	UTF-8 text.  A UTF-8 character in the pattern matches the same
 *	character in the text.
 *
 *	A FuzzyString keeps a scratch buffer for matching, so it is not
 *	thread-safe: one thread at a time per FuzzyString (copies are
 *	independent, so give each thread its own).
 */
class FuzzyString
{
public:
	enum case_mode
	{
		CASE_SMART,	///< Ignore the case, unless the word has a capital letter
		CASE_IGNORE,	///< Always ignore the case (ASCII)
		CASE_RESPECT	///< Always respect the case
	};
	enum match_mode
	{
		MATCH_FUZZY,	///< Words match fuzzily, 'word exactly (fzf)
		MATCH_EXACT	///< Words match exactly, 'word fuzzily (fzf --exact)
	};

	/** One text that matched, out of Search() */
	struct Result
	{
		int nIndex;	///< Its index in the texts given to Search()
		int nScore;	///< Its score (GetScore())
	};

	FuzzyString( const String& cPattern = "", case_mode eCase = CASE_SMART,
		     match_mode eMode = MATCH_FUZZY );
	FuzzyString( const FuzzyString& cOther );
	~FuzzyString();
	FuzzyString& operator=( const FuzzyString& cOther );

	void		SetPattern( const String& cPattern );
	const String&	GetPattern() const;
	void		SetCaseMode( case_mode eCase );
	case_mode	GetCaseMode() const;
	void		SetMatchMode( match_mode eMode );
	match_mode	GetMatchMode() const;

	/** Whether the pattern has no words to match: it matches every text */
	bool		IsEmpty() const;

	/** Whether cText matches.  pnScore, when not NULL, gets its score
	 * (as GetScore()), and pcPositions the positions of the characters
	 * that matched (as GetPositions()). */
	bool		Match( const String& cText, int* pnScore = NULL,
			       std::vector<int>* pcPositions = NULL ) const;
	bool		Matches( const String& cText ) const;
	/** The score of cText (0 or more; higher is better), or -1 when it
	 * doesn't match */
	int		GetScore( const String& cText ) const;
	/** The byte offsets in cText of the characters that matched, in
	 * order; empty when it doesn't match, or the pattern is empty or only
	 * !words */
	std::vector<int>	GetPositions( const String& cText ) const;
	/** cText with the characters that matched between pzBefore and
	 * pzAfter (one pair around each run of them); cText as it is when it
	 * doesn't match */
	String		Highlight( const String& cText, const char* pzBefore,
				   const char* pzAfter ) const;

	/** The texts of cTexts that match, best first: by score, then the
	 * shorter text, then the first in cTexts (as fzf sorts).  nLimit,
	 * when not 0, keeps only that many of the best. */
	std::vector<Result>	Search( const std::vector<String>& cTexts,
					size_t nLimit = 0 ) const;

	/** One-off: the score of cText for cPattern, -1 when it doesn't match */
	static int	Score( const String& cPattern, const String& cText );

private:
	class Private;
	Private* m;
};

} // end of namespace

#endif // __F_UTIL_FUZZYSTRING_H__
