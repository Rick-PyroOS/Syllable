/*  FuzzyString - fuzzy text search, as fzf does it, on telescope-fzf-native.
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

#include <util/fuzzystring.h>

#include <fzf/fzf.h>

#include <algorithm>
#include <string>
#include <stdlib.h>
#include <string.h>

using namespace os;

// The slab is fzf's scratch memory, reused from one match to the next (a
// match that needs more allocates its own).  fzf's default is 200 KB of
// 16-bit scores, sized for Telescope's long lines; this covers a text of a
// few hundred bytes against a pattern of a few dozen.
static const size_t SLAB_16 = 16 * 1024;
static const size_t SLAB_32 = 2048;

class FuzzyString::Private
{
public:
	Private( const String& cPattern, case_mode eCase, match_mode eMode )
		: m_cPattern( cPattern ), m_eCase( eCase ), m_eMode( eMode ),
		  m_psPattern( NULL ), m_psSlab( NULL )
	{
		Parse();
	}
	~Private()
	{
		if( m_psPattern != NULL )
			fzf_free_pattern( m_psPattern );
		if( m_psSlab != NULL )
			fzf_free_slab( m_psSlab );
	}

	void Parse()
	{
		if( m_psPattern != NULL )
			fzf_free_pattern( m_psPattern );
		fzf_case_types eCase = CaseSmart;
		if( m_eCase == CASE_IGNORE )
			eCase = CaseIgnore;
		else if( m_eCase == CASE_RESPECT )
			eCase = CaseRespect;
		// fzf_parse_pattern() trims the pattern in place: give it a copy
		const char* pzPattern = m_cPattern.c_str();
		std::vector<char> cBuffer( pzPattern, pzPattern + strlen( pzPattern ) + 1 );
		m_psPattern = fzf_parse_pattern( eCase, false, &cBuffer[0], m_eMode == MATCH_FUZZY );
	}

	bool IsEmpty() const
	{
		return m_psPattern == NULL || m_psPattern->ptr == NULL || m_psPattern->size == 0;
	}

	fzf_slab_t* Slab() const
	{
		if( m_psSlab == NULL )
		{
			fzf_slab_config_t sConfig = { SLAB_16, SLAB_32 };
			m_psSlab = fzf_make_slab( sConfig );
		}
		return m_psSlab;
	}

	// fzf_get_score() and fzf_get_positions() in one, on a text with a
	// length (an os::String can hold a NUL): every set of words must
	// match, a set when one of its words does (a | b); a !word matches
	// when it isn't in the text, and scores nothing.  psPositions, when
	// not NULL, gets the positions of the words that matched.
	bool Evaluate( const String& cText, int* pnScore, fzf_position_t* psPositions ) const
	{
		int nTotal = 0;
		if( !IsEmpty() )
		{
			fzf_string_t sText;
			sText.data = cText.c_str();
			sText.size = cText.size();
			fzf_slab_t* psSlab = Slab();
			for( size_t i = 0; i < m_psPattern->size; i++ )
			{
				const fzf_term_set_t* psSet = m_psPattern->ptr[i];
				bool bMatched = false;
				int nScore = 0;
				for( size_t j = 0; j < psSet->size; j++ )
				{
					const fzf_term_t* psTerm = &psSet->ptr[j];
					fzf_result_t sResult = psTerm->fn( psTerm->case_sensitive, false, &sText,
									   (fzf_string_t*)psTerm->text,
									   psTerm->inv ? NULL : psPositions, psSlab );
					if( sResult.start >= 0 )
					{
						if( psTerm->inv )
							continue;
						nScore = sResult.score;
						bMatched = true;
						break;
					}
					if( psTerm->inv )
					{
						nScore = 0;
						bMatched = true;
					}
				}
				if( !bMatched )
					return false;
				nTotal += nScore;
			}
		}
		if( pnScore != NULL )
			*pnScore = std::max( nTotal, 0 );
		return true;
	}

	String m_cPattern;
	case_mode m_eCase;
	match_mode m_eMode;
	fzf_pattern_t* m_psPattern;
	mutable fzf_slab_t* m_psSlab;
};

/** Makes a pattern.
 * \param cPattern the pattern, in fzf's extended syntax ("fuzzy 'exact ^prefix suffix$ !not a | b")
 * \param eCase how it matches lowercase and uppercase
 * \param eMode whether its words match fuzzily (MATCH_FUZZY) or exactly (MATCH_EXACT)
 */
FuzzyString::FuzzyString( const String& cPattern, case_mode eCase, match_mode eMode )
	: m( new Private( cPattern, eCase, eMode ) )
{
}

FuzzyString::FuzzyString( const FuzzyString& cOther )
	: m( new Private( cOther.m->m_cPattern, cOther.m->m_eCase, cOther.m->m_eMode ) )
{
}

FuzzyString::~FuzzyString()
{
	delete m;
}

FuzzyString& FuzzyString::operator=( const FuzzyString& cOther )
{
	if( this != &cOther )
	{
		m->m_cPattern = cOther.m->m_cPattern;
		m->m_eCase = cOther.m->m_eCase;
		m->m_eMode = cOther.m->m_eMode;
		m->Parse();
	}
	return *this;
}

void FuzzyString::SetPattern( const String& cPattern )
{
	m->m_cPattern = cPattern;
	m->Parse();
}

const String& FuzzyString::GetPattern() const
{
	return m->m_cPattern;
}

void FuzzyString::SetCaseMode( case_mode eCase )
{
	m->m_eCase = eCase;
	m->Parse();
}

FuzzyString::case_mode FuzzyString::GetCaseMode() const
{
	return m->m_eCase;
}

void FuzzyString::SetMatchMode( match_mode eMode )
{
	m->m_eMode = eMode;
	m->Parse();
}

FuzzyString::match_mode FuzzyString::GetMatchMode() const
{
	return m->m_eMode;
}

bool FuzzyString::IsEmpty() const
{
	return m->IsEmpty();
}

bool FuzzyString::Match( const String& cText, int* pnScore, std::vector<int>* pcPositions ) const
{
	if( pcPositions == NULL )
		return m->Evaluate( cText, pnScore, NULL );

	pcPositions->clear();
	fzf_position_t* psPositions = fzf_pos_array( 0 );
	bool bMatch = m->Evaluate( cText, pnScore, psPositions );
	if( bMatch && psPositions != NULL )
	{
		// in the order of the words, each from its end: sorted, and
		// once each (two words can match the same character)
		pcPositions->assign( psPositions->data, psPositions->data + psPositions->size );
		std::sort( pcPositions->begin(), pcPositions->end() );
		pcPositions->erase( std::unique( pcPositions->begin(), pcPositions->end() ), pcPositions->end() );
	}
	fzf_free_positions( psPositions );
	return bMatch;
}

bool FuzzyString::Matches( const String& cText ) const
{
	return m->Evaluate( cText, NULL, NULL );
}

int FuzzyString::GetScore( const String& cText ) const
{
	int nScore = 0;
	return m->Evaluate( cText, &nScore, NULL ) ? nScore : -1;
}

std::vector<int> FuzzyString::GetPositions( const String& cText ) const
{
	std::vector<int> cPositions;
	Match( cText, NULL, &cPositions );
	return cPositions;
}

String FuzzyString::Highlight( const String& cText, const char* pzBefore, const char* pzAfter ) const
{
	std::vector<int> cPositions;
	if( !Match( cText, NULL, &cPositions ) || cPositions.empty() )
		return cText;
	if( pzBefore == NULL )
		pzBefore = "";
	if( pzAfter == NULL )
		pzAfter = "";

	const char* pzText = cText.c_str();
	const size_t nLength = cText.size();
	std::string cResult;
	size_t nDone = 0;
	for( size_t i = 0; i < cPositions.size(); )
	{
		// one run of characters that follow each other
		size_t nStart = cPositions[i];
		size_t nEnd = nStart + 1;
		for( i++; i < cPositions.size() && size_t( cPositions[i] ) == nEnd; i++ )
			nEnd++;
		if( nEnd > nLength )
			break;
		cResult.append( pzText + nDone, nStart - nDone );
		cResult += pzBefore;
		cResult.append( pzText + nStart, nEnd - nStart );
		cResult += pzAfter;
		nDone = nEnd;
	}
	cResult.append( pzText + nDone, nLength - nDone );
	return String( cResult );
}

namespace
{

struct Ranked
{
	FuzzyString::Result sResult;
	size_t nLength;
};

// fzf's order: the score, then the shorter text, then the first one
struct BetterFirst
{
	bool operator()( const Ranked& sA, const Ranked& sB ) const
	{
		if( sA.sResult.nScore != sB.sResult.nScore )
			return sA.sResult.nScore > sB.sResult.nScore;
		if( sA.nLength != sB.nLength )
			return sA.nLength < sB.nLength;
		return sA.sResult.nIndex < sB.sResult.nIndex;
	}
};

}

std::vector<FuzzyString::Result> FuzzyString::Search( const std::vector<String>& cTexts, size_t nLimit ) const
{
	std::vector<Ranked> cRanked;
	for( size_t i = 0; i < cTexts.size(); i++ )
	{
		Ranked sRanked;
		if( m->Evaluate( cTexts[i], &sRanked.sResult.nScore, NULL ) )
		{
			sRanked.sResult.nIndex = int( i );
			sRanked.nLength = cTexts[i].size();
			cRanked.push_back( sRanked );
		}
	}
	if( nLimit != 0 && nLimit < cRanked.size() )
	{
		std::partial_sort( cRanked.begin(), cRanked.begin() + nLimit, cRanked.end(), BetterFirst() );
		cRanked.resize( nLimit );
	}
	else
		std::sort( cRanked.begin(), cRanked.end(), BetterFirst() );

	std::vector<Result> cResults;
	cResults.reserve( cRanked.size() );
	for( size_t i = 0; i < cRanked.size(); i++ )
		cResults.push_back( cRanked[i].sResult );
	return cResults;
}

int FuzzyString::Score( const String& cPattern, const String& cText )
{
	return FuzzyString( cPattern ).GetScore( cText );
}
