/*  JSONPath (RFC 9535) queries on cJSON trees, for os::JSON.
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
 *
 *  What is supported, as RFC 9535 has it:
 *    $  @                    the root, the current node (in filters)
 *    .name  ['name']  ["n"]  members (with \-escapes, \uXXXX)
 *    .*  [*]                 every member or element
 *    [3]  [-1]               elements, from the end when negative
 *    [1:5:2]  [::-1]         slices
 *    [0,2]  ['a','b']        several selectors
 *    ..name  ..*  ..[0]      descendants
 *    [?@.price < 10]         filters: == != < <= > >=, && || !, ( ),
 *                            existence ([?@.isbn]), literals (numbers,
 *                            'strings', "strings", true, false, null)
 *    length() count() value() match() search()   the RFC's functions;
 *                            match() and search() take POSIX extended
 *                            regular expressions (close to I-Regexp)
 *  Comparisons take a query that selects one node; one that selects none
 *  or several compares as "nothing" (the RFC rejects those queries).
 */

#include "jsonpath.h"
#include <cjson/cJSON.h>

#include <algorithm>
#include <map>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <regex.h>

// GCC 4.1.2 exports anonymous-namespace functions from a shared library;
// these helpers stay out of libsyllable's ABI
#pragma GCC visibility push(hidden)

namespace
{

// ---- The parsed query ----------------------------------------------------------

struct Expr;
struct Comparable;

struct Selector
{
	enum Kind { NAME, WILDCARD, INDEX, SLICE, FILTER };
	Kind eKind;
	std::string cName;
	int nIndex;
	bool bHasStart, bHasEnd;
	int nStart, nEnd, nStep;
	Expr* psFilter;
	Selector() : eKind( NAME ), nIndex( 0 ), bHasStart( false ), bHasEnd( false ), nStart( 0 ), nEnd( 0 ), nStep( 1 ), psFilter( NULL ) {}
};

struct Segment
{
	bool bDescendant;
	std::vector<Selector> asSelectors;
	Segment() : bDescendant( false ) {}
};

struct QueryAst
{
	bool bAbsolute;		// $, or @
	std::vector<Segment> asSegments;
	QueryAst() : bAbsolute( true ) {}
	bool IsSingular() const
	{
		for( size_t i = 0; i < asSegments.size(); i++ )
		{
			const Segment& s = asSegments[i];
			if( s.bDescendant || s.asSelectors.size() != 1 ||
			    ( s.asSelectors[0].eKind != Selector::NAME && s.asSelectors[0].eKind != Selector::INDEX ) )
				return false;
		}
		return true;
	}
};

enum CompareOp { EQ, NE, LT, LE, GT, GE };

struct Comparable
{
	enum Kind { LITERAL, QUERY, LENGTH, COUNT, VALUE };
	Kind eKind;
	cJSON* psLiteral;
	QueryAst* psQuery;
	Comparable* psArg;
	Comparable() : eKind( LITERAL ), psLiteral( NULL ), psQuery( NULL ), psArg( NULL ) {}
};

struct Expr
{
	enum Kind { OR, AND, NOT, COMPARE, EXISTS, MATCH, SEARCH };
	Kind eKind;
	Expr* psA;
	Expr* psB;
	CompareOp eOp;
	Comparable* psLeft;
	Comparable* psRight;
	QueryAst* psQuery;
	Expr() : eKind( EXISTS ), psA( NULL ), psB( NULL ), eOp( EQ ), psLeft( NULL ), psRight( NULL ), psQuery( NULL ) {}
};

/* Owns everything a parse makes */
struct Arena
{
	std::vector<QueryAst*> apsQueries;
	std::vector<Expr*> apsExprs;
	std::vector<Comparable*> apsComparables;
	std::vector<cJSON*> apsValues;		// literals, and function results while evaluating
	~Arena()
	{
		for( size_t i = 0; i < apsQueries.size(); i++ ) delete apsQueries[i];
		for( size_t i = 0; i < apsExprs.size(); i++ ) delete apsExprs[i];
		for( size_t i = 0; i < apsComparables.size(); i++ ) delete apsComparables[i];
		for( size_t i = 0; i < apsValues.size(); i++ ) cJSON_Delete( apsValues[i] );
	}
	QueryAst* NewQuery() { apsQueries.push_back( new QueryAst ); return apsQueries.back(); }
	Expr* NewExpr() { apsExprs.push_back( new Expr ); return apsExprs.back(); }
	Comparable* NewComparable() { apsComparables.push_back( new Comparable ); return apsComparables.back(); }
	cJSON* Keep( cJSON* ps ) { apsValues.push_back( ps ); return ps; }
};

// ---- Parser ----------------------------------------------------------------------

class Parser
{
public:
	Parser( const char* pzText, Arena* pcArena ) : m_pz( pzText ), m_nPos( 0 ), m_pcArena( pcArena ), m_bFailed( false ) {}

	QueryAst* ParseTop()
	{
		QueryAst* psQuery = ParseQuery( false );
		if( psQuery != NULL && Peek() != '\0' )
			return Fail( "unexpected text after the query" ), (QueryAst*)NULL;
		return m_bFailed ? NULL : psQuery;
	}

	std::string Error() const { return m_cError; }

private:
	char Peek( size_t n = 0 ) const
	{
		for( size_t i = 0; i < n; i++ )
			if( m_pz[m_nPos + i] == '\0' )
				return '\0';
		return m_pz[m_nPos + n];
	}
	static bool IsBlank( char c ) { return c == ' ' || c == '\t' || c == '\n' || c == '\r'; }
	void SkipBlank() { while( IsBlank( m_pz[m_nPos] ) ) m_nPos++; }
	bool Eat( const char* pz )
	{
		size_t n = strlen( pz );
		if( strncmp( m_pz + m_nPos, pz, n ) != 0 )
			return false;
		m_nPos += n;
		return true;
	}
	void* Fail( const char* pzWhat )
	{
		if( !m_bFailed )
		{
			char zBuffer[32];
			snprintf( zBuffer, sizeof( zBuffer ), "column %d: ", (int)m_nPos + 1 );
			m_cError = std::string( zBuffer ) + pzWhat;
			m_bFailed = true;
		}
		return NULL;
	}

	static bool IsNameFirst( unsigned char c ) { return ( c >= 'a' && c <= 'z' ) || ( c >= 'A' && c <= 'Z' ) || c == '_' || c >= 0x80; }
	static bool IsNameChar( unsigned char c ) { return IsNameFirst( c ) || ( c >= '0' && c <= '9' ); }

	/* $ or @ and the segments after it */
	QueryAst* ParseQuery( bool bRelativeAllowed )
	{
		QueryAst* psQuery = m_pcArena->NewQuery();
		if( Eat( "$" ) )
			psQuery->bAbsolute = true;
		else if( bRelativeAllowed && Eat( "@" ) )
			psQuery->bAbsolute = false;
		else
			return (QueryAst*)Fail( bRelativeAllowed ? "expected $ or @" : "a query starts with $" );
		for( ;; )
		{
			size_t nSave = m_nPos;
			SkipBlank();
			Segment sSegment;
			if( Eat( ".." ) )
			{
				sSegment.bDescendant = true;
				if( Peek() == '[' )
				{
					if( !ParseBracket( &sSegment ) )
						return NULL;
				}
				else if( !ParseDotted( &sSegment ) )
					return NULL;
			}
			else if( Peek() == '.' )
			{
				m_nPos++;
				if( !ParseDotted( &sSegment ) )
					return NULL;
			}
			else if( Peek() == '[' )
			{
				if( !ParseBracket( &sSegment ) )
					return NULL;
			}
			else
			{
				m_nPos = nSave;		// the blank space belongs to what follows
				break;
			}
			psQuery->asSegments.push_back( sSegment );
		}
		return psQuery;
	}

	/* * or a name, after . or .. */
	bool ParseDotted( Segment* psSegment )
	{
		Selector sSel;
		if( Eat( "*" ) )
			sSel.eKind = Selector::WILDCARD;
		else if( IsNameFirst( Peek() ) )
		{
			size_t nStart = m_nPos;
			while( IsNameChar( m_pz[m_nPos] ) )
				m_nPos++;
			sSel.eKind = Selector::NAME;
			sSel.cName.assign( m_pz + nStart, m_nPos - nStart );
		}
		else
			return Fail( "expected a name or *" ), false;
		psSegment->asSelectors.push_back( sSel );
		return true;
	}

	bool ParseBracket( Segment* psSegment )
	{
		m_nPos++;	// [
		for( ;; )
		{
			SkipBlank();
			Selector sSel;
			if( !ParseSelector( &sSel ) )
				return false;
			psSegment->asSelectors.push_back( sSel );
			SkipBlank();
			if( Eat( "," ) )
				continue;
			if( Eat( "]" ) )
				return true;
			return Fail( "expected , or ]" ), false;
		}
	}

	bool ParseInt( int* pn )
	{
		size_t nStart = m_nPos;
		if( Peek() == '-' )
			m_nPos++;
		if( !( Peek() >= '0' && Peek() <= '9' ) )
			return Fail( "expected a number" ), false;
		while( m_pz[m_nPos] >= '0' && m_pz[m_nPos] <= '9' )
			m_nPos++;
		*pn = atoi( std::string( m_pz + nStart, m_nPos - nStart ).c_str() );
		return true;
	}

	bool ParseSelector( Selector* psSel )
	{
		char c = Peek();
		if( c == '\'' || c == '"' )
		{
			psSel->eKind = Selector::NAME;
			return ParseString( &psSel->cName );
		}
		if( Eat( "*" ) )
		{
			psSel->eKind = Selector::WILDCARD;
			return true;
		}
		if( Eat( "?" ) )
		{
			psSel->eKind = Selector::FILTER;
			SkipBlank();
			psSel->psFilter = ParseOr();
			return psSel->psFilter != NULL;
		}
		if( c == '-' || ( c >= '0' && c <= '9' ) || c == ':' )
		{
			int n = 0;
			bool bHaveFirst = false;
			if( c != ':' )
			{
				if( !ParseInt( &n ) )
					return false;
				bHaveFirst = true;
			}
			SkipBlank();
			if( Peek() != ':' )
			{
				psSel->eKind = Selector::INDEX;
				psSel->nIndex = n;
				return true;
			}
			psSel->eKind = Selector::SLICE;
			psSel->bHasStart = bHaveFirst;
			psSel->nStart = n;
			m_nPos++;	// :
			SkipBlank();
			if( Peek() == '-' || ( Peek() >= '0' && Peek() <= '9' ) )
			{
				if( !ParseInt( &psSel->nEnd ) )
					return false;
				psSel->bHasEnd = true;
				SkipBlank();
			}
			if( Eat( ":" ) )
			{
				SkipBlank();
				if( Peek() == '-' || ( Peek() >= '0' && Peek() <= '9' ) )
				{
					if( !ParseInt( &psSel->nStep ) )
						return false;
				}
			}
			return true;
		}
		return Fail( "expected a name, *, an index, a slice or a filter" ), false;
	}

	static void AppendUtf8( std::string* pc, unsigned long n )
	{
		if( n < 0x80 )
			*pc += (char)n;
		else if( n < 0x800 )
		{
			*pc += (char)( 0xC0 | ( n >> 6 ) );
			*pc += (char)( 0x80 | ( n & 0x3F ) );
		}
		else if( n < 0x10000 )
		{
			*pc += (char)( 0xE0 | ( n >> 12 ) );
			*pc += (char)( 0x80 | ( ( n >> 6 ) & 0x3F ) );
			*pc += (char)( 0x80 | ( n & 0x3F ) );
		}
		else
		{
			*pc += (char)( 0xF0 | ( n >> 18 ) );
			*pc += (char)( 0x80 | ( ( n >> 12 ) & 0x3F ) );
			*pc += (char)( 0x80 | ( ( n >> 6 ) & 0x3F ) );
			*pc += (char)( 0x80 | ( n & 0x3F ) );
		}
	}

	bool ParseHex4( unsigned long* pn )
	{
		*pn = 0;
		for( int i = 0; i < 4; i++ )
		{
			char c = m_pz[m_nPos];
			int d = ( c >= '0' && c <= '9' ) ? c - '0' : ( c >= 'a' && c <= 'f' ) ? c - 'a' + 10 : ( c >= 'A' && c <= 'F' ) ? c - 'A' + 10 : -1;
			if( d < 0 )
				return Fail( "expected 4 hex digits after \\u" ), false;
			*pn = *pn * 16 + d;
			m_nPos++;
		}
		return true;
	}

	/* 'string' or "string", with JSON's escapes (and \' in '...') */
	bool ParseString( std::string* pcOut )
	{
		char cQuote = m_pz[m_nPos++];
		for( ;; )
		{
			char c = m_pz[m_nPos];
			if( c == '\0' )
				return Fail( "unterminated string" ), false;
			m_nPos++;
			if( c == cQuote )
				return true;
			if( c != '\\' )
			{
				*pcOut += c;
				continue;
			}
			char e = m_pz[m_nPos++];
			switch( e )
			{
				case 'b': *pcOut += '\b'; break;
				case 'f': *pcOut += '\f'; break;
				case 'n': *pcOut += '\n'; break;
				case 'r': *pcOut += '\r'; break;
				case 't': *pcOut += '\t'; break;
				case '/': case '\\': case '\'': case '"': *pcOut += e; break;
				case 'u':
				{
					unsigned long n;
					if( !ParseHex4( &n ) )
						return false;
					if( n >= 0xD800 && n <= 0xDBFF )	// a surrogate pair
					{
						unsigned long nLow;
						if( !Eat( "\\u" ) || !ParseHex4( &nLow ) || nLow < 0xDC00 || nLow > 0xDFFF )
							return Fail( "expected the second half of a surrogate pair" ), false;
						n = 0x10000 + ( ( n - 0xD800 ) << 10 ) + ( nLow - 0xDC00 );
					}
					AppendUtf8( pcOut, n );
					break;
				}
				default:
					m_nPos--;
					return Fail( "unknown escape" ), false;
			}
		}
	}

	Expr* ParseOr()
	{
		Expr* psLeft = ParseAnd();
		while( psLeft != NULL )
		{
			SkipBlank();
			if( !Eat( "||" ) )
				break;
			SkipBlank();
			Expr* psRight = ParseAnd();
			if( psRight == NULL )
				return NULL;
			Expr* psOr = m_pcArena->NewExpr();
			psOr->eKind = Expr::OR;
			psOr->psA = psLeft;
			psOr->psB = psRight;
			psLeft = psOr;
		}
		return psLeft;
	}

	Expr* ParseAnd()
	{
		Expr* psLeft = ParseBasic();
		while( psLeft != NULL )
		{
			SkipBlank();
			if( !Eat( "&&" ) )
				break;
			SkipBlank();
			Expr* psRight = ParseBasic();
			if( psRight == NULL )
				return NULL;
			Expr* psAnd = m_pcArena->NewExpr();
			psAnd->eKind = Expr::AND;
			psAnd->psA = psLeft;
			psAnd->psB = psRight;
			psLeft = psAnd;
		}
		return psLeft;
	}

	bool ParseCompareOp( CompareOp* pe )
	{
		if( Eat( "==" ) ) { *pe = EQ; return true; }
		if( Eat( "!=" ) ) { *pe = NE; return true; }
		if( Eat( "<=" ) ) { *pe = LE; return true; }
		if( Eat( ">=" ) ) { *pe = GE; return true; }
		if( Eat( "<" ) ) { *pe = LT; return true; }
		if( Eat( ">" ) ) { *pe = GT; return true; }
		return false;
	}

	Expr* ParseBasic()
	{
		if( Peek() == '!' && Peek( 1 ) != '=' )
		{
			m_nPos++;
			SkipBlank();
			Expr* psInner = ParseBasic();
			if( psInner == NULL )
				return NULL;
			Expr* psNot = m_pcArena->NewExpr();
			psNot->eKind = Expr::NOT;
			psNot->psA = psInner;
			return psNot;
		}
		if( Eat( "(" ) )
		{
			SkipBlank();
			Expr* psInner = ParseOr();
			if( psInner == NULL )
				return NULL;
			SkipBlank();
			if( !Eat( ")" ) )
				return (Expr*)Fail( "expected )" );
			return psInner;
		}
		// match() and search() are tests of their own
		bool bMatch = Eat( "match(" );
		if( bMatch || Eat( "search(" ) )
		{
			Expr* psExpr = m_pcArena->NewExpr();
			psExpr->eKind = bMatch ? Expr::MATCH : Expr::SEARCH;
			SkipBlank();
			psExpr->psLeft = ParseComparable();
			if( psExpr->psLeft == NULL )
				return NULL;
			SkipBlank();
			if( !Eat( "," ) )
				return (Expr*)Fail( "expected , between the arguments" );
			SkipBlank();
			psExpr->psRight = ParseComparable();
			if( psExpr->psRight == NULL )
				return NULL;
			SkipBlank();
			if( !Eat( ")" ) )
				return (Expr*)Fail( "expected )" );
			return psExpr;
		}
		size_t nStart = m_nPos;
		Comparable* psLeft = ParseComparable();
		if( psLeft == NULL )
			return NULL;
		SkipBlank();
		CompareOp eOp;
		if( ParseCompareOp( &eOp ) )
		{
			SkipBlank();
			Comparable* psRight = ParseComparable();
			if( psRight == NULL )
				return NULL;
			Expr* psCmp = m_pcArena->NewExpr();
			psCmp->eKind = Expr::COMPARE;
			psCmp->eOp = eOp;
			psCmp->psLeft = psLeft;
			psCmp->psRight = psRight;
			return psCmp;
		}
		if( psLeft->eKind != Comparable::QUERY )
		{
			m_nPos = nStart;
			return (Expr*)Fail( "expected a comparison after this value" );
		}
		Expr* psExists = m_pcArena->NewExpr();
		psExists->eKind = Expr::EXISTS;
		psExists->psQuery = psLeft->psQuery;
		return psExists;
	}

	Comparable* ParseComparable()
	{
		Comparable* ps = m_pcArena->NewComparable();
		char c = Peek();
		if( c == '@' || c == '$' )
		{
			ps->eKind = Comparable::QUERY;
			ps->psQuery = ParseQuery( true );
			return ps->psQuery ? ps : NULL;
		}
		if( c == '\'' || c == '"' )
		{
			std::string cText;
			if( !ParseString( &cText ) )
				return NULL;
			ps->psLiteral = m_pcArena->Keep( cJSON_CreateString( cText.c_str() ) );
			return ps;
		}
		if( Eat( "true" ) ) { ps->psLiteral = m_pcArena->Keep( cJSON_CreateTrue() ); return ps; }
		if( Eat( "false" ) ) { ps->psLiteral = m_pcArena->Keep( cJSON_CreateFalse() ); return ps; }
		if( Eat( "null" ) ) { ps->psLiteral = m_pcArena->Keep( cJSON_CreateNull() ); return ps; }
		if( c == '-' || ( c >= '0' && c <= '9' ) )
		{
			char* pzEnd;
			double v = strtod( m_pz + m_nPos, &pzEnd );
			if( pzEnd == m_pz + m_nPos )
				return (Comparable*)Fail( "expected a number" );
			m_nPos = pzEnd - m_pz;
			ps->psLiteral = m_pcArena->Keep( cJSON_CreateNumber( v ) );
			return ps;
		}
		bool bLength = Eat( "length(" );
		if( bLength || Eat( "value(" ) )
		{
			ps->eKind = bLength ? Comparable::LENGTH : Comparable::VALUE;
			SkipBlank();
			ps->psArg = ParseComparable();
			if( ps->psArg == NULL )
				return NULL;
			SkipBlank();
			if( !Eat( ")" ) )
				return (Comparable*)Fail( "expected )" );
			return ps;
		}
		if( Eat( "count(" ) )
		{
			ps->eKind = Comparable::COUNT;
			SkipBlank();
			if( Peek() != '@' && Peek() != '$' )
				return (Comparable*)Fail( "count() takes a query" );
			ps->psQuery = ParseQuery( true );
			if( ps->psQuery == NULL )
				return NULL;
			SkipBlank();
			if( !Eat( ")" ) )
				return (Comparable*)Fail( "expected )" );
			return ps;
		}
		return (Comparable*)Fail( "expected a query, a literal or a function" );
	}

	const char* m_pz;
	size_t m_nPos;
	Arena* m_pcArena;
	bool m_bFailed;
	std::string m_cError;
};

// ---- Evaluation ------------------------------------------------------------------

class Evaluator
{
public:
	Evaluator( cJSON* psRoot, Arena* pcArena ) : m_psRoot( psRoot ), m_pcArena( pcArena ) {}
	~Evaluator()
	{
		for( std::map<std::string, regex_t*>::iterator i = m_cRegex.begin(); i != m_cRegex.end(); ++i )
		{
			if( i->second != NULL )
			{
				regfree( i->second );
				delete i->second;
			}
		}
	}

	void Run( const QueryAst* psQuery, cJSON* psCurrent, std::vector<cJSON*>* pcOut )
	{
		std::vector<cJSON*> cNodes( 1, psQuery->bAbsolute ? m_psRoot : psCurrent );
		for( size_t i = 0; i < psQuery->asSegments.size(); i++ )
		{
			const Segment& sSeg = psQuery->asSegments[i];
			std::vector<cJSON*> cNext;
			for( size_t j = 0; j < cNodes.size(); j++ )
			{
				if( sSeg.bDescendant )
				{
					std::vector<cJSON*> cAll;
					Descendants( cNodes[j], &cAll );
					for( size_t k = 0; k < cAll.size(); k++ )
						Select( sSeg, cAll[k], &cNext );
				}
				else
					Select( sSeg, cNodes[j], &cNext );
			}
			cNodes.swap( cNext );
		}
		pcOut->insert( pcOut->end(), cNodes.begin(), cNodes.end() );
	}

private:
	static void Descendants( cJSON* psNode, std::vector<cJSON*>* pcOut )
	{
		pcOut->push_back( psNode );
		if( cJSON_IsObject( psNode ) || cJSON_IsArray( psNode ) )
			for( cJSON* ps = psNode->child; ps != NULL; ps = ps->next )
				Descendants( ps, pcOut );
	}

	void Select( const Segment& sSeg, cJSON* psNode, std::vector<cJSON*>* pcOut )
	{
		for( size_t i = 0; i < sSeg.asSelectors.size(); i++ )
			Select( sSeg.asSelectors[i], psNode, pcOut );
	}

	void Select( const Selector& sSel, cJSON* psNode, std::vector<cJSON*>* pcOut )
	{
		bool bObject = cJSON_IsObject( psNode ), bArray = cJSON_IsArray( psNode );
		switch( sSel.eKind )
		{
			case Selector::NAME:
				if( bObject )
				{
					cJSON* ps = cJSON_GetObjectItemCaseSensitive( psNode, sSel.cName.c_str() );
					if( ps != NULL )
						pcOut->push_back( ps );
				}
				break;
			case Selector::WILDCARD:
				if( bObject || bArray )
					for( cJSON* ps = psNode->child; ps != NULL; ps = ps->next )
						pcOut->push_back( ps );
				break;
			case Selector::INDEX:
				if( bArray )
				{
					int nLen = cJSON_GetArraySize( psNode );
					int n = sSel.nIndex < 0 ? nLen + sSel.nIndex : sSel.nIndex;
					if( n >= 0 && n < nLen )
						pcOut->push_back( cJSON_GetArrayItem( psNode, n ) );
				}
				break;
			case Selector::SLICE:
				if( bArray )
					Slice( sSel, psNode, pcOut );
				break;
			case Selector::FILTER:
				if( bObject || bArray )
					for( cJSON* ps = psNode->child; ps != NULL; ps = ps->next )
						if( Test( sSel.psFilter, ps ) )
							pcOut->push_back( ps );
				break;
		}
	}

	static void Slice( const Selector& sSel, cJSON* psArray, std::vector<cJSON*>* pcOut )
	{
		int nStep = sSel.nStep;
		if( nStep == 0 )
			return;
		std::vector<cJSON*> cItems;
		for( cJSON* ps = psArray->child; ps != NULL; ps = ps->next )
			cItems.push_back( ps );
		int nLen = cItems.size();
		int nStart = sSel.bHasStart ? sSel.nStart : ( nStep > 0 ? 0 : nLen - 1 );
		int nEnd = sSel.bHasEnd ? sSel.nEnd : ( nStep > 0 ? nLen : -nLen - 1 );
		if( nStart < 0 ) nStart += nLen;
		if( nEnd < 0 ) nEnd += nLen;
		if( nStep > 0 )
		{
			int nLower = std::min( std::max( nStart, 0 ), nLen );
			int nUpper = std::min( std::max( nEnd, 0 ), nLen );
			for( int i = nLower; i < nUpper; i += nStep )
				pcOut->push_back( cItems[i] );
		}
		else
		{
			int nUpper = std::min( std::max( nStart, -1 ), nLen - 1 );
			int nLower = std::min( std::max( nEnd, -1 ), nLen - 1 );
			for( int i = nUpper; i > nLower; i += nStep )
				pcOut->push_back( cItems[i] );
		}
	}

	/* A value, or NULL for "nothing" */
	cJSON* Value( const Comparable* ps, cJSON* psCurrent )
	{
		switch( ps->eKind )
		{
			case Comparable::LITERAL:
				return ps->psLiteral;
			case Comparable::QUERY:
			{
				std::vector<cJSON*> c;
				Run( ps->psQuery, psCurrent, &c );
				return c.size() == 1 ? c[0] : NULL;
			}
			case Comparable::VALUE:
				return Value( ps->psArg, psCurrent );
			case Comparable::COUNT:
			{
				std::vector<cJSON*> c;
				Run( ps->psQuery, psCurrent, &c );
				return m_pcArena->Keep( cJSON_CreateNumber( (double)c.size() ) );
			}
			case Comparable::LENGTH:
			{
				cJSON* psArg = Value( ps->psArg, psCurrent );
				if( cJSON_IsString( psArg ) )
				{
					int n = 0;	// characters, not bytes
					for( const unsigned char* p = (const unsigned char*)psArg->valuestring; *p; p++ )
						if( ( *p & 0xC0 ) != 0x80 )
							n++;
					return m_pcArena->Keep( cJSON_CreateNumber( n ) );
				}
				if( cJSON_IsArray( psArg ) || cJSON_IsObject( psArg ) )
					return m_pcArena->Keep( cJSON_CreateNumber( cJSON_GetArraySize( psArg ) ) );
				return NULL;
			}
		}
		return NULL;
	}

	static bool Equal( const cJSON* a, const cJSON* b )
	{
		if( a == NULL || b == NULL )
			return a == b;
		if( cJSON_IsNumber( a ) && cJSON_IsNumber( b ) )
			return a->valuedouble == b->valuedouble;
		return cJSON_Compare( a, b, 1 );
	}

	static bool Less( const cJSON* a, const cJSON* b )
	{
		if( cJSON_IsNumber( a ) && cJSON_IsNumber( b ) )
			return a->valuedouble < b->valuedouble;
		if( cJSON_IsString( a ) && cJSON_IsString( b ) )
			return strcmp( a->valuestring, b->valuestring ) < 0;
		return false;
	}

	bool Compare( CompareOp eOp, const cJSON* a, const cJSON* b )
	{
		switch( eOp )
		{
			case EQ: return Equal( a, b );
			case NE: return !Equal( a, b );
			case LT: return Less( a, b );
			case LE: return Less( a, b ) || Equal( a, b );
			case GT: return Less( b, a );
			case GE: return Less( b, a ) || Equal( a, b );
		}
		return false;
	}

	regex_t* Regex( const std::string& cPattern )
	{
		std::map<std::string, regex_t*>::iterator i = m_cRegex.find( cPattern );
		if( i != m_cRegex.end() )
			return i->second;
		regex_t* psRegex = new regex_t;
		if( regcomp( psRegex, cPattern.c_str(), REG_EXTENDED | REG_NOSUB ) != 0 )
		{
			delete psRegex;
			psRegex = NULL;		// not valid: matches nothing
		}
		m_cRegex[cPattern] = psRegex;
		return psRegex;
	}

	bool Test( const Expr* ps, cJSON* psCurrent )
	{
		switch( ps->eKind )
		{
			case Expr::OR: return Test( ps->psA, psCurrent ) || Test( ps->psB, psCurrent );
			case Expr::AND: return Test( ps->psA, psCurrent ) && Test( ps->psB, psCurrent );
			case Expr::NOT: return !Test( ps->psA, psCurrent );
			case Expr::COMPARE:
				return Compare( ps->eOp, Value( ps->psLeft, psCurrent ), Value( ps->psRight, psCurrent ) );
			case Expr::EXISTS:
			{
				std::vector<cJSON*> c;
				Run( ps->psQuery, psCurrent, &c );
				return !c.empty();
			}
			case Expr::MATCH:
			case Expr::SEARCH:
			{
				cJSON* psText = Value( ps->psLeft, psCurrent );
				cJSON* psPattern = Value( ps->psRight, psCurrent );
				if( !cJSON_IsString( psText ) || !cJSON_IsString( psPattern ) )
					return false;
				std::string cPattern = psPattern->valuestring;
				if( ps->eKind == Expr::MATCH )
					cPattern = "^(" + cPattern + ")$";
				regex_t* psRegex = Regex( cPattern );
				return psRegex != NULL && regexec( psRegex, psText->valuestring, 0, NULL, 0 ) == 0;
			}
		}
		return false;
	}

	cJSON* m_psRoot;
	Arena* m_pcArena;
	std::map<std::string, regex_t*> m_cRegex;
};

} // namespace

bool os::jsonpath::IsQuery( const char* pzName )
{
	if( pzName == NULL || pzName[0] != '$' )
		return false;
	char c = pzName[1];
	return c == '\0' || c == '.' || c == '[' || c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

bool os::jsonpath::Query( cJSON* psRoot, const char* pzQuery, std::vector<cJSON*>* pcResult, std::string* pcError )
{
	Arena cArena;
	Parser cParser( pzQuery, &cArena );
	QueryAst* psQuery = cParser.ParseTop();
	if( psQuery == NULL )
	{
		if( pcError != NULL )
			*pcError = cParser.Error();
		return false;
	}
	if( psRoot != NULL )
	{
		Evaluator cEval( psRoot, &cArena );
		cEval.Run( psQuery, psRoot, pcResult );
	}
	return true;
}

#pragma GCC visibility pop
