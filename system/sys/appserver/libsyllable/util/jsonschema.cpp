/*  JSON Schema validation on cJSON trees, for os::JSON.
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
 *  The validation keywords of JSON Schema 2020-12, and draft-07's forms
 *  of the ones that changed (items as an array, additionalItems,
 *  dependencies, definitions; draft-04's boolean exclusiveMinimum and
 *  exclusiveMaximum):
 *    any      type enum const allOf anyOf oneOf not if/then/else
 *             $ref ("#", "#/json/pointer", "#anchor"; $defs, definitions)
 *    numbers  minimum maximum exclusiveMinimum exclusiveMaximum multipleOf
 *    strings  minLength maxLength pattern
 *    arrays   items prefixItems additionalItems minItems maxItems
 *             uniqueItems contains minContains maxContains
 *    objects  properties patternProperties additionalProperties required
 *             minProperties maxProperties propertyNames dependentRequired
 *             dependentSchemas dependencies
 *  Not done: $ref to other files or URLs, $dynamicRef,
 *  unevaluatedProperties and unevaluatedItems, and format (an annotation
 *  only, by default).  pattern takes POSIX extended regular expressions,
 *  with \d \w \s (and \D \W \S) of ECMA-262's.
 */

#include "jsonschema.h"
#include <cjson/cJSON.h>

#include <map>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <regex.h>

// GCC 4.1.2 exports anonymous-namespace functions from a shared library;
// these helpers stay out of libsyllable's ABI
#pragma GCC visibility push(hidden)

namespace
{

std::string number_text( double v )
{
	char zBuffer[32];
	snprintf( zBuffer, sizeof( zBuffer ), "%.15g", v );
	return zBuffer;
}

bool is_integer( const cJSON* ps )
{
	return cJSON_IsNumber( ps ) && floor( ps->valuedouble ) == ps->valuedouble;
}

int utf8_length( const char* pz )
{
	int n = 0;
	for( const unsigned char* p = (const unsigned char*)pz; *p; p++ )
		if( ( *p & 0xC0 ) != 0x80 )
			n++;
	return n;
}

/* Deep equality, numbers by value (1 and 1.0 are the same) */
bool equal( const cJSON* a, const cJSON* b )
{
	if( cJSON_IsNumber( a ) && cJSON_IsNumber( b ) )
		return a->valuedouble == b->valuedouble;
	if( cJSON_IsArray( a ) && cJSON_IsArray( b ) )
	{
		const cJSON* x = a->child;
		const cJSON* y = b->child;
		for( ; x && y; x = x->next, y = y->next )
			if( !equal( x, y ) )
				return false;
		return x == NULL && y == NULL;
	}
	if( cJSON_IsObject( a ) && cJSON_IsObject( b ) )
	{
		if( cJSON_GetArraySize( a ) != cJSON_GetArraySize( b ) )
			return false;
		for( const cJSON* x = a->child; x; x = x->next )
		{
			const cJSON* y = cJSON_GetObjectItemCaseSensitive( b, x->string );
			if( y == NULL || !equal( x, y ) )
				return false;
		}
		return true;
	}
	return cJSON_Compare( a, b, 1 ) != 0;
}

const char* type_name( const cJSON* ps )
{
	if( cJSON_IsNull( ps ) ) return "null";
	if( cJSON_IsBool( ps ) ) return "boolean";
	if( is_integer( ps ) ) return "integer";
	if( cJSON_IsNumber( ps ) ) return "number";
	if( cJSON_IsString( ps ) ) return "string";
	if( cJSON_IsArray( ps ) ) return "array";
	if( cJSON_IsObject( ps ) ) return "object";
	return "unknown";
}

bool has_type( const cJSON* ps, const char* pzType )
{
	if( strcmp( pzType, "number" ) == 0 ) return cJSON_IsNumber( ps );
	if( strcmp( pzType, "integer" ) == 0 ) return is_integer( ps );
	if( strcmp( pzType, "string" ) == 0 ) return cJSON_IsString( ps );
	if( strcmp( pzType, "boolean" ) == 0 ) return cJSON_IsBool( ps );
	if( strcmp( pzType, "null" ) == 0 ) return cJSON_IsNull( ps );
	if( strcmp( pzType, "array" ) == 0 ) return cJSON_IsArray( ps );
	if( strcmp( pzType, "object" ) == 0 ) return cJSON_IsObject( ps );
	return false;
}

/* $.a.b, $.list[2], $['odd name'] */
std::string member_path( const std::string& cPath, const char* pzName )
{
	bool bPlain = ( pzName[0] >= 'a' && pzName[0] <= 'z' ) || ( pzName[0] >= 'A' && pzName[0] <= 'Z' ) || pzName[0] == '_';
	for( const char* p = pzName; bPlain && *p; p++ )
		bPlain = ( *p >= 'a' && *p <= 'z' ) || ( *p >= 'A' && *p <= 'Z' ) || ( *p >= '0' && *p <= '9' ) || *p == '_';
	if( bPlain )
		return cPath + "." + pzName;
	std::string cQuoted;
	for( const char* p = pzName; *p; p++ )
	{
		if( *p == '\'' || *p == '\\' )
			cQuoted += '\\';
		cQuoted += *p;
	}
	return cPath + "['" + cQuoted + "']";
}

std::string element_path( const std::string& cPath, int nIndex )
{
	char zBuffer[16];
	snprintf( zBuffer, sizeof( zBuffer ), "[%d]", nIndex );
	return cPath + zBuffer;
}

/* ECMA-262's \d \w \s (and \D \W \S) in POSIX extended regular expression terms */
std::string posix_pattern( const char* pz )
{
	std::string c;
	bool bInClass = false;
	for( ; *pz; pz++ )
	{
		if( *pz == '\\' && pz[1] != '\0' )
		{
			char e = *++pz;
			const char* pzClass = NULL;
			bool bNegated = false;
			switch( e )
			{
				case 'd': pzClass = "0-9"; break;
				case 'w': pzClass = "A-Za-z0-9_"; break;
				case 's': pzClass = "[:space:]"; break;
				case 'D': pzClass = "0-9"; bNegated = true; break;
				case 'W': pzClass = "A-Za-z0-9_"; bNegated = true; break;
				case 'S': pzClass = "[:space:]"; bNegated = true; break;
			}
			if( pzClass != NULL && bInClass && !bNegated )
				c += pzClass;
			else if( pzClass != NULL && !bInClass )
				c += std::string( bNegated ? "[^" : "[" ) + pzClass + "]";
			else
			{
				c += '\\';
				c += e;
			}
			continue;
		}
		if( *pz == '[' && !bInClass )
			bInClass = true;
		else if( *pz == ']' && bInClass )
			bInClass = false;
		c += *pz;
	}
	return c;
}

class Validator
{
public:
	Validator( const cJSON* psRoot, std::vector<std::string>* pcErrors ) : m_psRoot( psRoot ), m_pcErrors( pcErrors ) {}
	~Validator()
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

	/* bReport: record why it fails (not inside anyOf, oneOf, not, if and
	 * contains, where failing is how they work) */
	bool Check( const cJSON* psSchema, const cJSON* psValue, const std::string& cPath, bool bReport, int nDepth = 0 )
	{
		if( cJSON_IsTrue( psSchema ) )
			return true;
		if( cJSON_IsFalse( psSchema ) )
			return Fail( bReport, cPath, "no value is allowed here" );
		if( !cJSON_IsObject( psSchema ) )
			return true;
		if( nDepth > 64 )
			return Fail( bReport, cPath, "the schema's $ref go round in circles" );
		bool bOk = true;

		const cJSON* psRef = Get( psSchema, "$ref" );
		if( cJSON_IsString( psRef ) )
		{
			const cJSON* psTarget = Resolve( psRef->valuestring );
			if( psTarget == NULL )
				bOk = Fail( bReport, cPath, std::string( "the schema's $ref \"" ) + psRef->valuestring + "\" can't be found (only references inside the schema are supported)" );
			else
				bOk = Check( psTarget, psValue, cPath, bReport, nDepth + 1 ) && bOk;
		}

		bOk = CheckAny( psSchema, psValue, cPath, bReport, nDepth ) && bOk;
		if( cJSON_IsNumber( psValue ) )
			bOk = CheckNumber( psSchema, psValue, cPath, bReport ) && bOk;
		else if( cJSON_IsString( psValue ) )
			bOk = CheckString( psSchema, psValue, cPath, bReport ) && bOk;
		else if( cJSON_IsArray( psValue ) )
			bOk = CheckArray( psSchema, psValue, cPath, bReport, nDepth ) && bOk;
		else if( cJSON_IsObject( psValue ) )
			bOk = CheckObject( psSchema, psValue, cPath, bReport, nDepth ) && bOk;
		return bOk;
	}

private:
	static const cJSON* Get( const cJSON* psSchema, const char* pzName )
	{
		return cJSON_GetObjectItemCaseSensitive( psSchema, pzName );
	}

	bool Fail( bool bReport, const std::string& cPath, const std::string& cWhat )
	{
		if( bReport && m_pcErrors != NULL )
			m_pcErrors->push_back( cPath + ": " + cWhat );
		return false;
	}

	/* "#", "#/json/pointer" or "#anchor" */
	const cJSON* Resolve( const char* pzRef )
	{
		if( pzRef[0] != '#' )
			return NULL;
		if( pzRef[1] == '\0' )
			return m_psRoot;
		if( pzRef[1] != '/' )
			return FindAnchor( m_psRoot, pzRef + 1 );
		const cJSON* ps = m_psRoot;
		const char* p = pzRef + 2;
		for( ;; )
		{
			std::string cPart;
			for( ; *p && *p != '/'; p++ )
			{
				if( p[0] == '~' && p[1] == '0' ) { cPart += '~'; p++; }
				else if( p[0] == '~' && p[1] == '1' ) { cPart += '/'; p++; }
				else if( p[0] == '%' && p[1] && p[2] )		// URI escapes (%25 is %)
				{
					char zHex[3] = { p[1], p[2], 0 };
					cPart += (char)strtol( zHex, NULL, 16 );
					p += 2;
				}
				else
					cPart += *p;
			}
			if( cJSON_IsArray( ps ) )
				ps = cJSON_GetArrayItem( ps, atoi( cPart.c_str() ) );
			else
				ps = cJSON_GetObjectItemCaseSensitive( ps, cPart.c_str() );
			if( ps == NULL )
				return NULL;
			if( *p == '\0' )
				return ps;
			p++;
		}
	}

	static const cJSON* FindAnchor( const cJSON* ps, const char* pzName )
	{
		if( cJSON_IsObject( ps ) )
		{
			const cJSON* psAnchor = cJSON_GetObjectItemCaseSensitive( ps, "$anchor" );
			if( cJSON_IsString( psAnchor ) && strcmp( psAnchor->valuestring, pzName ) == 0 )
				return ps;
		}
		if( cJSON_IsObject( ps ) || cJSON_IsArray( ps ) )
		{
			for( const cJSON* psChild = ps->child; psChild; psChild = psChild->next )
			{
				const cJSON* psFound = FindAnchor( psChild, pzName );
				if( psFound != NULL )
					return psFound;
			}
		}
		return NULL;
	}

	regex_t* Regex( const char* pzPattern )
	{
		std::string cPattern = posix_pattern( pzPattern );
		std::map<std::string, regex_t*>::iterator i = m_cRegex.find( cPattern );
		if( i != m_cRegex.end() )
			return i->second;
		regex_t* psRegex = new regex_t;
		if( regcomp( psRegex, cPattern.c_str(), REG_EXTENDED | REG_NOSUB ) != 0 )
		{
			delete psRegex;
			psRegex = NULL;
		}
		m_cRegex[cPattern] = psRegex;
		return psRegex;
	}

	bool Matches( const char* pzPattern, const char* pzText )
	{
		regex_t* psRegex = Regex( pzPattern );
		return psRegex != NULL && regexec( psRegex, pzText, 0, NULL, 0 ) == 0;
	}

	bool CheckAny( const cJSON* psSchema, const cJSON* psValue, const std::string& cPath, bool bReport, int nDepth )
	{
		bool bOk = true;
		const cJSON* ps;

		if( ( ps = Get( psSchema, "type" ) ) != NULL )
		{
			bool bMatch = false;
			std::string cWanted;
			if( cJSON_IsString( ps ) )
			{
				bMatch = has_type( psValue, ps->valuestring );
				cWanted = ps->valuestring;
			}
			else if( cJSON_IsArray( ps ) )
			{
				for( const cJSON* psType = ps->child; psType; psType = psType->next )
				{
					if( !cJSON_IsString( psType ) )
						continue;
					bMatch = bMatch || has_type( psValue, psType->valuestring );
					cWanted += ( cWanted.empty() ? "" : " or " ) + std::string( psType->valuestring );
				}
			}
			else
				bMatch = true;
			if( !bMatch )
				bOk = Fail( bReport, cPath, std::string( "is " ) + type_name( psValue ) + ", not " + cWanted );
		}
		if( cJSON_IsArray( ps = Get( psSchema, "enum" ) ) )
		{
			bool bFound = false;
			for( const cJSON* psOption = ps->child; psOption && !bFound; psOption = psOption->next )
				bFound = equal( psValue, psOption );
			if( !bFound )
				bOk = Fail( bReport, cPath, "is not one of the values the schema allows (enum)" );
		}
		if( ( ps = Get( psSchema, "const" ) ) != NULL && !equal( psValue, ps ) )
			bOk = Fail( bReport, cPath, "is not the value the schema requires (const)" );

		if( cJSON_IsArray( ps = Get( psSchema, "allOf" ) ) )
			for( const cJSON* psSub = ps->child; psSub; psSub = psSub->next )
				bOk = Check( psSub, psValue, cPath, bReport, nDepth + 1 ) && bOk;
		if( cJSON_IsArray( ps = Get( psSchema, "anyOf" ) ) )
		{
			bool bAny = false;
			for( const cJSON* psSub = ps->child; psSub && !bAny; psSub = psSub->next )
				bAny = Check( psSub, psValue, cPath, false, nDepth + 1 );
			if( !bAny )
				bOk = Fail( bReport, cPath, "matches none of the schemas of anyOf" );
		}
		if( cJSON_IsArray( ps = Get( psSchema, "oneOf" ) ) )
		{
			int nMatches = 0;
			for( const cJSON* psSub = ps->child; psSub; psSub = psSub->next )
				if( Check( psSub, psValue, cPath, false, nDepth + 1 ) )
					nMatches++;
			if( nMatches != 1 )
				bOk = Fail( bReport, cPath, nMatches == 0 ? "matches none of the schemas of oneOf"
				                                          : "matches more than one of the schemas of oneOf" );
		}
		if( ( ps = Get( psSchema, "not" ) ) != NULL && Check( ps, psValue, cPath, false, nDepth + 1 ) )
			bOk = Fail( bReport, cPath, "matches the schema of not" );
		if( ( ps = Get( psSchema, "if" ) ) != NULL )
		{
			const cJSON* psBranch = Get( psSchema, Check( ps, psValue, cPath, false, nDepth + 1 ) ? "then" : "else" );
			if( psBranch != NULL )
				bOk = Check( psBranch, psValue, cPath, bReport, nDepth + 1 ) && bOk;
		}
		return bOk;
	}

	bool CheckNumber( const cJSON* psSchema, const cJSON* psValue, const std::string& cPath, bool bReport )
	{
		bool bOk = true;
		double v = psValue->valuedouble;
		const cJSON* ps;
		const cJSON* psExMin = Get( psSchema, "exclusiveMinimum" );
		const cJSON* psExMax = Get( psSchema, "exclusiveMaximum" );

		if( cJSON_IsNumber( ps = Get( psSchema, "minimum" ) ) )
		{
			bool bExclusive = cJSON_IsTrue( psExMin );	// draft-04
			if( bExclusive ? !( v > ps->valuedouble ) : !( v >= ps->valuedouble ) )
				bOk = Fail( bReport, cPath, number_text( v ) + ( bExclusive ? " is not more than " : " is less than the minimum " ) + number_text( ps->valuedouble ) );
		}
		if( cJSON_IsNumber( ps = Get( psSchema, "maximum" ) ) )
		{
			bool bExclusive = cJSON_IsTrue( psExMax );
			if( bExclusive ? !( v < ps->valuedouble ) : !( v <= ps->valuedouble ) )
				bOk = Fail( bReport, cPath, number_text( v ) + ( bExclusive ? " is not less than " : " is more than the maximum " ) + number_text( ps->valuedouble ) );
		}
		if( cJSON_IsNumber( psExMin ) && !( v > psExMin->valuedouble ) )
			bOk = Fail( bReport, cPath, number_text( v ) + " is not more than " + number_text( psExMin->valuedouble ) );
		if( cJSON_IsNumber( psExMax ) && !( v < psExMax->valuedouble ) )
			bOk = Fail( bReport, cPath, number_text( v ) + " is not less than " + number_text( psExMax->valuedouble ) );
		if( cJSON_IsNumber( ps = Get( psSchema, "multipleOf" ) ) && ps->valuedouble > 0 )
		{
			double q = v / ps->valuedouble;
			if( fabs( q - floor( q + 0.5 ) ) > 1e-9 * ( fabs( q ) > 1 ? fabs( q ) : 1 ) )
				bOk = Fail( bReport, cPath, number_text( v ) + " is not a multiple of " + number_text( ps->valuedouble ) );
		}
		return bOk;
	}

	bool CheckString( const cJSON* psSchema, const cJSON* psValue, const std::string& cPath, bool bReport )
	{
		bool bOk = true;
		int nLen = utf8_length( psValue->valuestring );
		const cJSON* ps;
		if( cJSON_IsNumber( ps = Get( psSchema, "minLength" ) ) && nLen < ps->valuedouble )
			bOk = Fail( bReport, cPath, "is shorter than " + number_text( ps->valuedouble ) + " characters" );
		if( cJSON_IsNumber( ps = Get( psSchema, "maxLength" ) ) && nLen > ps->valuedouble )
			bOk = Fail( bReport, cPath, "is longer than " + number_text( ps->valuedouble ) + " characters" );
		if( cJSON_IsString( ps = Get( psSchema, "pattern" ) ) && !Matches( ps->valuestring, psValue->valuestring ) )
			bOk = Fail( bReport, cPath, std::string( "doesn't match the pattern " ) + ps->valuestring );
		return bOk;
	}

	bool CheckArray( const cJSON* psSchema, const cJSON* psValue, const std::string& cPath, bool bReport, int nDepth )
	{
		bool bOk = true;
		int nCount = cJSON_GetArraySize( psValue );
		const cJSON* ps;

		// The elements a tuple form covers: prefixItems (2020-12), or items
		// as an array (draft-07); the rest take items or additionalItems
		const cJSON* psPrefix = Get( psSchema, "prefixItems" );
		const cJSON* psItems = Get( psSchema, "items" );
		const cJSON* psRest = psItems;
		if( cJSON_IsArray( psItems ) )
		{
			psPrefix = psItems;
			psRest = Get( psSchema, "additionalItems" );
		}
		int nPrefix = cJSON_IsArray( psPrefix ) ? cJSON_GetArraySize( psPrefix ) : 0;
		int i = 0;
		for( const cJSON* psItem = psValue->child; psItem; psItem = psItem->next, i++ )
		{
			if( i < nPrefix )
				bOk = Check( cJSON_GetArrayItem( psPrefix, i ), psItem, element_path( cPath, i ), bReport, nDepth + 1 ) && bOk;
			else if( psRest != NULL )
				bOk = Check( psRest, psItem, element_path( cPath, i ), bReport, nDepth + 1 ) && bOk;
		}

		if( cJSON_IsNumber( ps = Get( psSchema, "minItems" ) ) && nCount < ps->valuedouble )
			bOk = Fail( bReport, cPath, "has fewer than " + number_text( ps->valuedouble ) + " items" );
		if( cJSON_IsNumber( ps = Get( psSchema, "maxItems" ) ) && nCount > ps->valuedouble )
			bOk = Fail( bReport, cPath, "has more than " + number_text( ps->valuedouble ) + " items" );
		if( cJSON_IsTrue( Get( psSchema, "uniqueItems" ) ) )
		{
			bool bUnique = true;
			for( const cJSON* a = psValue->child; a && bUnique; a = a->next )
				for( const cJSON* b = a->next; b && bUnique; b = b->next )
					bUnique = !equal( a, b );
			if( !bUnique )
				bOk = Fail( bReport, cPath, "has items that are the same (uniqueItems)" );
		}
		if( ( ps = Get( psSchema, "contains" ) ) != NULL )
		{
			int nMatches = 0;
			for( const cJSON* psItem = psValue->child; psItem; psItem = psItem->next )
				if( Check( ps, psItem, cPath, false, nDepth + 1 ) )
					nMatches++;
			const cJSON* psMin = Get( psSchema, "minContains" );
			const cJSON* psMax = Get( psSchema, "maxContains" );
			double vMin = cJSON_IsNumber( psMin ) ? psMin->valuedouble : 1;
			if( nMatches < vMin )
				bOk = Fail( bReport, cPath, vMin == 1 ? std::string( "has no item that matches contains" )
				                                      : "has fewer than " + number_text( vMin ) + " items that match contains" );
			if( cJSON_IsNumber( psMax ) && nMatches > psMax->valuedouble )
				bOk = Fail( bReport, cPath, "has more than " + number_text( psMax->valuedouble ) + " items that match contains" );
		}
		return bOk;
	}

	bool CheckObject( const cJSON* psSchema, const cJSON* psValue, const std::string& cPath, bool bReport, int nDepth )
	{
		bool bOk = true;
		const cJSON* ps;
		const cJSON* psProps = Get( psSchema, "properties" );
		const cJSON* psPatterns = Get( psSchema, "patternProperties" );
		const cJSON* psAdditional = Get( psSchema, "additionalProperties" );
		const cJSON* psNames = Get( psSchema, "propertyNames" );

		for( const cJSON* psMember = psValue->child; psMember; psMember = psMember->next )
		{
			std::string cMemberPath = member_path( cPath, psMember->string );
			bool bCovered = false;
			const cJSON* psSub = cJSON_IsObject( psProps ) ? cJSON_GetObjectItemCaseSensitive( psProps, psMember->string ) : NULL;
			if( psSub != NULL )
			{
				bCovered = true;
				bOk = Check( psSub, psMember, cMemberPath, bReport, nDepth + 1 ) && bOk;
			}
			if( cJSON_IsObject( psPatterns ) )
			{
				for( const cJSON* psPattern = psPatterns->child; psPattern; psPattern = psPattern->next )
				{
					if( Matches( psPattern->string, psMember->string ) )
					{
						bCovered = true;
						bOk = Check( psPattern, psMember, cMemberPath, bReport, nDepth + 1 ) && bOk;
					}
				}
			}
			if( !bCovered && psAdditional != NULL )
			{
				if( cJSON_IsFalse( psAdditional ) )
					bOk = Fail( bReport, cMemberPath, "is not a property the schema allows" );
				else
					bOk = Check( psAdditional, psMember, cMemberPath, bReport, nDepth + 1 ) && bOk;
			}
			if( psNames != NULL )
			{
				cJSON* psName = cJSON_CreateString( psMember->string );
				if( !Check( psNames, psName, cMemberPath, false, nDepth + 1 ) )
					bOk = Fail( bReport, cMemberPath, "is not a name the schema allows (propertyNames)" );
				cJSON_Delete( psName );
			}
		}

		if( cJSON_IsArray( ps = Get( psSchema, "required" ) ) )
			for( const cJSON* psName = ps->child; psName; psName = psName->next )
				if( cJSON_IsString( psName ) && cJSON_GetObjectItemCaseSensitive( psValue, psName->valuestring ) == NULL )
					bOk = Fail( bReport, cPath, std::string( "is missing the required property \"" ) + psName->valuestring + "\"" );

		int nCount = cJSON_GetArraySize( psValue );
		if( cJSON_IsNumber( ps = Get( psSchema, "minProperties" ) ) && nCount < ps->valuedouble )
			bOk = Fail( bReport, cPath, "has fewer than " + number_text( ps->valuedouble ) + " properties" );
		if( cJSON_IsNumber( ps = Get( psSchema, "maxProperties" ) ) && nCount > ps->valuedouble )
			bOk = Fail( bReport, cPath, "has more than " + number_text( ps->valuedouble ) + " properties" );

		// dependentRequired, dependentSchemas, and draft-07's dependencies (either)
		const char* const apzDeps[] = { "dependentRequired", "dependentSchemas", "dependencies" };
		for( int d = 0; d < 3; d++ )
		{
			if( !cJSON_IsObject( ps = Get( psSchema, apzDeps[d] ) ) )
				continue;
			for( const cJSON* psDep = ps->child; psDep; psDep = psDep->next )
			{
				if( cJSON_GetObjectItemCaseSensitive( psValue, psDep->string ) == NULL )
					continue;
				if( cJSON_IsArray( psDep ) )
				{
					for( const cJSON* psName = psDep->child; psName; psName = psName->next )
						if( cJSON_IsString( psName ) && cJSON_GetObjectItemCaseSensitive( psValue, psName->valuestring ) == NULL )
							bOk = Fail( bReport, cPath, std::string( "has \"" ) + psDep->string + "\" but not \"" + psName->valuestring + "\", which it requires" );
				}
				else
					bOk = Check( psDep, psValue, cPath, bReport, nDepth + 1 ) && bOk;
			}
		}
		return bOk;
	}

	const cJSON* m_psRoot;
	std::vector<std::string>* m_pcErrors;
	std::map<std::string, regex_t*> m_cRegex;
};

} // namespace

bool os::jsonschema::Validate( const cJSON* psSchema, const cJSON* psInstance, std::vector<std::string>* pcErrors )
{
	Validator cValidator( psSchema, pcErrors );
	return cValidator.Check( psSchema, psInstance, "$", true );
}

#pragma GCC visibility pop
