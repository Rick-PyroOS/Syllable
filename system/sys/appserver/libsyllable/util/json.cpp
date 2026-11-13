/*  JSON - JSON documents with the Get/Set methods of os::Settings, on cJSON.
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

#include <util/json.h>
#include <util/message.h>
#include <cjson/cJSON.h>
#include "jsonpath.h"
#include "jsonschema.h"

#include <string>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>

using namespace os;

// JSON::Private stays out of libsyllable's ABI (a nested class has the
// visibility of the class it is in unless it says otherwise)

/** \internal The implementation of JSON: the document a handle refers into,
 * the node it refers to, and the helpers */
class __attribute__((visibility("hidden"))) JSON::Private
{
public:
	/* The tree and its schema, shared by the handles into it and freed with
	 * the last of them */
	class __attribute__((visibility("hidden"))) Document
	{
	public:
		Document( cJSON* psRoot ) : m_psRoot( psRoot ), m_psSchema( NULL ), m_nRefs( 0 ), m_bError( false ) {}
		~Document() { cJSON_Delete( m_psRoot ); cJSON_Delete( m_psSchema ); }

		cJSON* m_psRoot;
		cJSON* m_psSchema;		// NULL: none
		std::vector<std::string> m_cValidationErrors;
		int m_nRefs;
		bool m_bError;
		std::string m_cError;
	};

	/* A new document, with an empty object at the top */
	Private() : m_pcDocument( NULL ), m_psNode( NULL )
	{
		Refer( new Document( cJSON_CreateObject() ), NULL );
		m_psNode = m_pcDocument->m_psRoot;
	}
	Private( Document* pcDocument, cJSON* psNode ) : m_pcDocument( NULL ), m_psNode( NULL ) { Refer( pcDocument, psNode ); }
	~Private() { Release(); }

	/* Refers to psNode in pcDocument (in place of what it referred to) */
	void Refer( Document* pcDocument, cJSON* psNode )
	{
		pcDocument->m_nRefs++;
		Release();
		m_pcDocument = pcDocument;
		m_psNode = psNode;
	}

	void Release()
	{
		if( m_pcDocument != NULL && --m_pcDocument->m_nRefs == 0 )
			delete m_pcDocument;
		m_pcDocument = NULL;
	}

	bool IsObject() const { return m_psNode != NULL && cJSON_IsObject( m_psNode ); }

	/* Records an error (GetLastError()) */
	void Fail( const char* pzWhat, const char* pzName )
	{
		m_pcDocument->m_bError = true;
		m_pcDocument->m_cError = std::string( pzWhat ) + ( pzName ? std::string( " \"" ) + pzName + "\"" : std::string() );
	}

	/* Stores psValue (and owns it) as pzName in this object, or records why
	 * it can't */
	void Store( const char* pzName, int nIndex, cJSON* psValue, bool bArray = false )
	{
		if( !IsObject() )
		{
			cJSON_Delete( psValue );
			Fail( "set on a JSON that refers to nothing:", pzName );
		}
		else if( jsonpath::IsQuery( pzName ) )
		{
			cJSON_Delete( psValue );
			Fail( "a JSONPath query can't be set (use a name or a dotted path):", pzName );
		}
		else if( store( m_psNode, pzName, nIndex, psValue, bArray ) < 0 )
			Fail( "can't set (a part of the path isn't an object, or out of memory):", pzName );
	}

	Document*	m_pcDocument;
	cJSON*		m_psNode;		// NULL: refers to nothing

	// -- Helpers --

	/* The object holding the last part of a path ("a.b.c": the object "b" in "a"),
	 * and that part's name.  bCreate makes the missing objects on the way; a part
	 * that is there but isn't an object ends the walk (NULL). */
	static cJSON* walk( cJSON* psObject, const char* pzPath, bool bCreate, std::string* pcLast )
	{
		if( psObject == NULL || pzPath == NULL || !cJSON_IsObject( psObject ) )
			return NULL;
		const char* pzPart = pzPath;
		for( ;; )
		{
			const char* pzDot = strchr( pzPart, '.' );
			if( pzDot == NULL )
				break;
			std::string cPart( pzPart, pzDot - pzPart );
			cJSON* psChild = cJSON_GetObjectItemCaseSensitive( psObject, cPart.c_str() );
			if( psChild == NULL )
			{
				if( !bCreate )
					return NULL;
				psChild = cJSON_CreateObject();
				if( psChild == NULL || !cJSON_AddItemToObject( psObject, cPart.c_str(), psChild ) )
				{
					cJSON_Delete( psChild );
					return NULL;
				}
			}
			else if( !cJSON_IsObject( psChild ) )
				return NULL;
			psObject = psChild;
			pzPart = pzDot + 1;
		}
		*pcLast = pzPart;
		return psObject;
	}

	/* The value nIndex of a name: an element of an array, or (index 0) the value */
	static cJSON* lookup( cJSON* psObject, const char* pzName, int nIndex )
	{
		if( os::jsonpath::IsQuery( pzName ) )	// match nIndex of a JSONPath query
		{
			std::vector<cJSON*> cMatches;
			if( !os::jsonpath::Query( psObject, pzName, &cMatches, NULL ) || nIndex < 0 || nIndex >= (int)cMatches.size() )
				return NULL;
			return cMatches[nIndex];
		}
		std::string cLast;
		cJSON* psParent = walk( psObject, pzName, false, &cLast );
		if( psParent == NULL || nIndex < 0 )
			return NULL;
		cJSON* psItem = cJSON_GetObjectItemCaseSensitive( psParent, cLast.c_str() );
		if( psItem == NULL )
			return NULL;
		if( cJSON_IsArray( psItem ) )
			return cJSON_GetArrayItem( psItem, nIndex );
		return nIndex == 0 ? psItem : NULL;
	}

	static bool pad( cJSON* psArray, int nCount )
	{
		while( cJSON_GetArraySize( psArray ) < nCount )
		{
			cJSON* psNull = cJSON_CreateNull();
			if( psNull == NULL || !cJSON_AddItemToArray( psArray, psNull ) )
			{
				cJSON_Delete( psNull );
				return false;
			}
		}
		return true;
	}

	/* Puts psValue at nIndex of a name (see JSONObject); takes psValue over.
	 * bArray: the name is always an array, even of one element (AddObject()). */
	static status_t store( cJSON* psObject, const char* pzName, int nIndex, cJSON* psValue, bool bArray = false )
	{
		std::string cLast;
		cJSON* psParent = NULL;
		if( psValue != NULL && nIndex >= 0 && !os::jsonpath::IsQuery( pzName ) )
			psParent = walk( psObject, pzName, true, &cLast );
		if( psParent == NULL )
		{
			cJSON_Delete( psValue );
			errno = EINVAL;
			return -1;
		}
		const char* pzLast = cLast.c_str();
		cJSON* psItem = cJSON_GetObjectItemCaseSensitive( psParent, pzLast );

		if( psItem != NULL && cJSON_IsArray( psItem ) )
		{
			if( nIndex < cJSON_GetArraySize( psItem ) )
				return cJSON_ReplaceItemInArray( psItem, nIndex, psValue ) ? 0 : -1;
			if( pad( psItem, nIndex ) && cJSON_AddItemToArray( psItem, psValue ) )
				return 0;
			cJSON_Delete( psValue );
			return -1;
		}
		if( nIndex == 0 && !bArray )
		{
			if( psItem != NULL )
				return cJSON_ReplaceItemInObjectCaseSensitive( psParent, pzLast, psValue ) ? 0 : -1;
			if( cJSON_AddItemToObject( psParent, pzLast, psValue ) )
				return 0;
			cJSON_Delete( psValue );
			return -1;
		}
		// A new array: the old value (if any) is element 0
		cJSON* psArray = cJSON_CreateArray();
		if( psArray == NULL )
		{
			cJSON_Delete( psValue );
			return -1;
		}
		if( psItem != NULL )
			cJSON_AddItemToArray( psArray, cJSON_DetachItemFromObjectCaseSensitive( psParent, pzLast ) );
		if( !pad( psArray, nIndex ) || !cJSON_AddItemToArray( psArray, psValue ) )
		{
			cJSON_Delete( psValue );
			if( psItem != NULL )		// put the old value back
				cJSON_AddItemToObject( psParent, pzLast, cJSON_DetachItemFromArray( psArray, 0 ) );
			cJSON_Delete( psArray );
			return -1;
		}
		if( !cJSON_AddItemToObject( psParent, pzLast, psArray ) )
		{
			cJSON_Delete( psArray );
			return -1;
		}
		return 0;
	}

	static bool get_number( const cJSON* psItem, const char* pzName, double* pvValue )
	{
		const cJSON* psNumber = cJSON_GetObjectItemCaseSensitive( psItem, pzName );
		if( !cJSON_IsNumber( psNumber ) )
			return false;
		*pvValue = psNumber->valuedouble;
		return true;
	}

	/* Up to four numbers from an object's names, or from an array */
	static bool get_numbers( const cJSON* psItem, const char* const* apzNames, int nCount, double* avValues )
	{
		if( cJSON_IsArray( psItem ) )
		{
			if( cJSON_GetArraySize( psItem ) != nCount )
				return false;
			for( int i = 0; i < nCount; i++ )
			{
				cJSON* psNumber = cJSON_GetArrayItem( psItem, i );
				if( !cJSON_IsNumber( psNumber ) )
					return false;
				avValues[i] = psNumber->valuedouble;
			}
			return true;
		}
		if( !cJSON_IsObject( psItem ) )
			return false;
		for( int i = 0; i < nCount; i++ )
			if( !get_number( psItem, apzNames[i], &avValues[i] ) )
				return false;
		return true;
	}

	static cJSON* make_numbers( const char* const* apzNames, int nCount, const double* avValues )
	{
		cJSON* psObject = cJSON_CreateObject();
		if( psObject == NULL )
			return NULL;
		for( int i = 0; i < nCount; i++ )
		{
			if( cJSON_AddNumberToObject( psObject, apzNames[i], avValues[i] ) == NULL )
			{
				cJSON_Delete( psObject );
				return NULL;
			}
		}
		return psObject;
	}

	static const char* const g_apzRect[];
	static const char* const g_apzPoint[];
	static const char* const g_apzColor[];

	/* A float as the shortest decimal that reads back as the same float, so 0.1f
	 * is saved as 0.1 rather than 0.10000000149011612 */
	static double float_to_double( float vValue )
	{
		char zBuffer[32];
		for( int nDigits = 6; nDigits <= 9; nDigits++ )
		{
			snprintf( zBuffer, sizeof( zBuffer ), "%.*g", nDigits, vValue );
			double vRead = strtod( zBuffer, NULL );
			if( (float)vRead == vValue )
				return vRead;
		}
		return vValue;
	}

	static const int64 MAX_SAFE_INTEGER;

	static int64 to_int64( const cJSON* psItem, int64 nDefault )
	{
		if( cJSON_IsNumber( psItem ) )
		{
			double v = psItem->valuedouble;
			if( v >= 9223372036854775807.0 || v < -9223372036854775807.0 || v != v )
				return nDefault;
			return (int64)v;
		}
		if( cJSON_IsString( psItem ) && psItem->valuestring[0] != '\0' )
		{
			char* pzEnd;
			errno = 0;
			long long n = strtoll( psItem->valuestring, &pzEnd, 10 );
			if( *pzEnd == '\0' && errno == 0 )
				return n;
		}
		if( cJSON_IsBool( psItem ) )
			return cJSON_IsTrue( psItem ) ? 1 : 0;
		return nDefault;
	}

	static int hex_digit( char c )
	{
		if( c >= '0' && c <= '9' ) return c - '0';
		if( c >= 'a' && c <= 'f' ) return c - 'a' + 10;
		if( c >= 'A' && c <= 'F' ) return c - 'A' + 10;
		return -1;
	}

	/* "#rgb", "#rrggbb" or "#rrggbbaa" */
	static bool parse_color_hex( const char* pz, Color32_s* psColor )
	{
		if( pz[0] != '#' )
			return false;
		pz++;
		int nLen = strlen( pz );
		int anValues[4] = { 0, 0, 0, 255 };
		for( int i = 0; i < nLen; i++ )
			if( hex_digit( pz[i] ) < 0 )
				return false;
		if( nLen == 3 )
		{
			for( int i = 0; i < 3; i++ )
				anValues[i] = hex_digit( pz[i] ) * 17;
		}
		else if( nLen == 6 || nLen == 8 )
		{
			for( int i = 0; i < nLen / 2; i++ )
				anValues[i] = hex_digit( pz[i * 2] ) * 16 + hex_digit( pz[i * 2 + 1] );
		}
		else
			return false;
		*psColor = Color32_s( anValues[0], anValues[1], anValues[2], anValues[3] );
		return true;
	}

	/* "line 3, column 7" of a position in a text */
	static std::string position( const char* pzText, const char* pzAt )
	{
		int nLine = 1, nColumn = 1;
		for( const char* p = pzText; p < pzAt && *p != '\0'; p++ )
		{
			if( *p == '\n' )
			{
				nLine++;
				nColumn = 1;
			}
			else
				nColumn++;
		}
		char zBuffer[64];
		snprintf( zBuffer, sizeof( zBuffer ), "line %d, column %d", nLine, nColumn );
		return zBuffer;
	}

	// -- Message conversion helpers --

	static bool is_integer( double v )
	{
		return v == v && v > -9223372036854775807.0 && v < 9223372036854775807.0 && v == (double)(int64)v;
	}

	/* Exactly the given names, all numbers (a rect or a point) */
	static bool has_only_numbers( const cJSON* psObject, const char* const* apzNames, int nCount )
	{
		if( cJSON_GetArraySize( psObject ) != nCount )
			return false;
		double v;
		for( int i = 0; i < nCount; i++ )
			if( !get_number( psObject, apzNames[i], &v ) )
				return false;
		return true;
	}

	static cJSON* number_or_string( int64 n )
	{
		if( n <= MAX_SAFE_INTEGER && n >= -MAX_SAFE_INTEGER )
			return cJSON_CreateNumber( (double)n );
		char zBuffer[32];
		snprintf( zBuffer, sizeof( zBuffer ), "%lld", (long long)n );
		return cJSON_CreateString( zBuffer );
	}

	static cJSON* color_string( const Color32_s& sColor )
	{
		char zBuffer[16];
		if( sColor.alpha == 255 )
			snprintf( zBuffer, sizeof( zBuffer ), "#%02x%02x%02x", sColor.red, sColor.green, sColor.blue );
		else
			snprintf( zBuffer, sizeof( zBuffer ), "#%02x%02x%02x%02x", sColor.red, sColor.green, sColor.blue, sColor.alpha );
		return cJSON_CreateString( zBuffer );
	}

	/* Value nIndex of a name as JSON, or NULL for a type JSON has no form for
	 * (pointers, raw data, variants...) */
	static cJSON* message_value( const Message& cMessage, const char* pzName, int nType, int nIndex )
	{
		switch( nType )
		{
			case T_STRING:
			{
				const char* pz = NULL;
				return cMessage.FindString( pzName, &pz, nIndex ) == 0 && pz ? cJSON_CreateString( pz ) : NULL;
			}
			case T_INT8:	{ int8 n;  return cMessage.FindInt8( pzName, &n, nIndex ) == 0 ? cJSON_CreateNumber( n ) : NULL; }
			case T_INT16:	{ int16 n; return cMessage.FindInt16( pzName, &n, nIndex ) == 0 ? cJSON_CreateNumber( n ) : NULL; }
			case T_INT32:	{ int32 n; return cMessage.FindInt32( pzName, &n, nIndex ) == 0 ? cJSON_CreateNumber( n ) : NULL; }
			case T_INT64:	{ int64 n; return cMessage.FindInt64( pzName, &n, nIndex ) == 0 ? number_or_string( n ) : NULL; }
			case T_BOOL:	{ bool b;  return cMessage.FindBool( pzName, &b, nIndex ) == 0 ? cJSON_CreateBool( b ) : NULL; }
			case T_FLOAT:	{ float v; return cMessage.FindFloat( pzName, &v, nIndex ) == 0 ? cJSON_CreateNumber( float_to_double( v ) ) : NULL; }
			case T_DOUBLE:	{ double v; return cMessage.FindDouble( pzName, &v, nIndex ) == 0 ? cJSON_CreateNumber( v ) : NULL; }
			case T_RECT:
			{
				Rect r( 0, 0, 0, 0 );
				if( cMessage.FindRect( pzName, &r, nIndex ) != 0 )
					return NULL;
				double av[4] = { float_to_double( r.left ), float_to_double( r.top ), float_to_double( r.right ), float_to_double( r.bottom ) };
				return make_numbers( g_apzRect, 4, av );
			}
			case T_IRECT:
			{
				IRect r( 0, 0, 0, 0 );
				if( cMessage.FindIRect( pzName, &r, nIndex ) != 0 )
					return NULL;
				double av[4] = { (double)r.left, (double)r.top, (double)r.right, (double)r.bottom };
				return make_numbers( g_apzRect, 4, av );
			}
			case T_POINT:
			{
				Point p;
				if( cMessage.FindPoint( pzName, &p, nIndex ) != 0 )
					return NULL;
				double av[2] = { float_to_double( p.x ), float_to_double( p.y ) };
				return make_numbers( g_apzPoint, 2, av );
			}
			case T_IPOINT:
			{
				IPoint p;
				if( cMessage.FindIPoint( pzName, &p, nIndex ) != 0 )
					return NULL;
				double av[2] = { (double)p.x, (double)p.y };
				return make_numbers( g_apzPoint, 2, av );
			}
			case T_COLOR32:
			{
				Color32_s s( 0, 0, 0 );
				return cMessage.FindColor32( pzName, &s, nIndex ) == 0 ? color_string( s ) : NULL;
			}
			case T_MESSAGE:
			{
				Message cChild;
				return cMessage.FindMessage( pzName, &cChild, nIndex ) == 0 ? message_to_json( cChild ) : NULL;
			}
			case T_JSON:
			{
				JSON cValue;
				if( cMessage.FindObject( pzName, cValue, nIndex ) != 0 )
					return NULL;
				return cJSON_Parse( cValue.ToString( false ).c_str() );
			}
			default:
				return NULL;
		}
	}

	static cJSON* message_to_json( const Message& cMessage )
	{
		cJSON* psObject = cJSON_CreateObject();
		if( psObject == NULL )
			return NULL;
		int nNames = cMessage.CountNames();
		for( int i = 0; i < nNames; i++ )
		{
			String cName = cMessage.GetName( i );
			int nType = 0, nCount = 0;
			if( cMessage.GetNameInfo( cName.c_str(), &nType, &nCount ) != 0 || nCount < 1 )
				continue;
			if( cJSON_GetObjectItemCaseSensitive( psObject, cName.c_str() ) != NULL )
				continue;	// a second type under the same name: the first wins
			if( nCount == 1 )
			{
				cJSON* psValue = message_value( cMessage, cName.c_str(), nType, 0 );
				if( psValue != NULL )
					cJSON_AddItemToObject( psObject, cName.c_str(), psValue );
				continue;
			}
			cJSON* psArray = cJSON_CreateArray();
			for( int j = 0; psArray != NULL && j < nCount; j++ )
			{
				cJSON* psValue = message_value( cMessage, cName.c_str(), nType, j );
				cJSON_AddItemToArray( psArray, psValue != NULL ? psValue : cJSON_CreateNull() );
			}
			if( psArray != NULL )
				cJSON_AddItemToObject( psObject, cName.c_str(), psArray );
		}
		return psObject;
	}

	/* One JSON value added to a name: integers as int32 (or int64 when larger),
	 * other numbers as doubles, rect and point objects as Rect and Point, other
	 * objects as messages (Message::Find...() asks for those types). */
	static void add_value( Message* pcMessage, const char* pzName, const cJSON* psValue )
	{
		if( cJSON_IsString( psValue ) )
			pcMessage->AddString( pzName, psValue->valuestring );
		else if( cJSON_IsBool( psValue ) )
			pcMessage->AddBool( pzName, cJSON_IsTrue( psValue ) );
		else if( cJSON_IsNumber( psValue ) )
		{
			double v = psValue->valuedouble;
			if( is_integer( v ) && v >= -2147483648.0 && v <= 2147483647.0 )
				pcMessage->AddInt32( pzName, (int32)v );
			else if( is_integer( v ) )
				pcMessage->AddInt64( pzName, (int64)v );
			else
				pcMessage->AddDouble( pzName, v );
		}
		else if( cJSON_IsObject( psValue ) )
		{
			double av[4];
			if( has_only_numbers( psValue, g_apzRect, 4 ) && get_numbers( psValue, g_apzRect, 4, av ) )
				pcMessage->AddRect( pzName, Rect( av[0], av[1], av[2], av[3] ) );
			else if( has_only_numbers( psValue, g_apzPoint, 2 ) && get_numbers( psValue, g_apzPoint, 2, av ) )
				pcMessage->AddPoint( pzName, Point( av[0], av[1] ) );
			else
			{
				Message cChild;
				json_to_message( psValue, &cChild );
				pcMessage->AddMessage( pzName, &cChild );
			}
		}
		// null, and arrays in arrays: nothing
	}

	static void json_to_message( const cJSON* psObject, Message* pcMessage )
	{
		for( const cJSON* psItem = psObject->child; psItem != NULL; psItem = psItem->next )
		{
			if( psItem->string == NULL )
				continue;
			if( cJSON_IsArray( psItem ) )
			{
				for( const cJSON* psElement = psItem->child; psElement != NULL; psElement = psElement->next )
					add_value( pcMessage, psItem->string, psElement );
			}
			else
				add_value( pcMessage, psItem->string, psItem );
		}
	}

	/* The object a node is in (through arrays), or NULL */
	static cJSON* find_owner( cJSON* psNode, cJSON* psOwner, const cJSON* psTarget )
	{
		for( cJSON* psChild = psNode->child; psChild != NULL; psChild = psChild->next )
		{
			if( psChild == psTarget )
				return psOwner;
			cJSON* psFound = NULL;
			if( cJSON_IsObject( psChild ) )
				psFound = find_owner( psChild, psChild, psTarget );
			else if( cJSON_IsArray( psChild ) )
				psFound = find_owner( psChild, psOwner, psTarget );
			if( psFound != NULL )
				return psFound;
		}
		return NULL;
	}

	/* The object or array a node is in, or NULL */
	static cJSON* find_container( cJSON* psNode, const cJSON* psTarget )
	{
		for( cJSON* psChild = psNode->child; psChild != NULL; psChild = psChild->next )
		{
			if( psChild == psTarget )
				return psNode;
			if( cJSON_IsObject( psChild ) || cJSON_IsArray( psChild ) )
			{
				cJSON* psFound = find_container( psChild, psTarget );
				if( psFound != NULL )
					return psFound;
			}
		}
		return NULL;
	}

	/* A string, or a number or bool as text */
	static String value_string( const cJSON* psItem, const char* pzDefault )
	{
		if( cJSON_IsString( psItem ) )
			return String( psItem->valuestring );
		if( cJSON_IsBool( psItem ) )
			return String( cJSON_IsTrue( psItem ) ? "true" : "false" );
		if( cJSON_IsNumber( psItem ) )
		{
			char zBuffer[32];
			double v = psItem->valuedouble;
			if( v < (double)MAX_SAFE_INTEGER && v > -(double)MAX_SAFE_INTEGER && v == (double)(int64)v )
				snprintf( zBuffer, sizeof( zBuffer ), "%lld", (long long)v );
			else
			{
				// the shortest of 15 or 17 digits that reads back the same (8.95, not 8.9499999999999993)
				snprintf( zBuffer, sizeof( zBuffer ), "%.15g", v );
				if( strtod( zBuffer, NULL ) != v )
					snprintf( zBuffer, sizeof( zBuffer ), "%.17g", v );
			}
			return String( zBuffer );
		}
		return String( pzDefault ? pzDefault : "" );
	}

	static JSON::Type type_of( const cJSON* psItem )
	{
		if( psItem == NULL )		return JSON::JSON_NONE;
		if( cJSON_IsNull( psItem ) )	return JSON::JSON_NULL;
		if( cJSON_IsBool( psItem ) )	return JSON::JSON_BOOL;
		if( cJSON_IsNumber( psItem ) )	return JSON::JSON_NUMBER;
		if( cJSON_IsString( psItem ) )	return JSON::JSON_STRING;
		if( cJSON_IsArray( psItem ) )	return JSON::JSON_ARRAY;
		if( cJSON_IsObject( psItem ) )	return JSON::JSON_OBJECT;
		return JSON::JSON_NONE;
	}
};

const char* const JSON::Private::g_apzRect[] = { "left", "top", "right", "bottom" };
const char* const JSON::Private::g_apzPoint[] = { "x", "y" };
const char* const JSON::Private::g_apzColor[] = { "red", "green", "blue", "alpha" };
const int64 JSON::Private::MAX_SAFE_INTEGER = ( (int64)1 << 53 );

// ---- JSON ---------------------------------------------------------------------

/** A new document, with an empty object at the top */
JSON::JSON()
{
	m = new Private;
}

JSON::JSON( Private* pcPrivate ) : m( pcPrivate )
{
}

/** The same object (not a copy of it: see Clone()) */
JSON::JSON( const JSON& cOther ) : Flattenable()
{
	m = new Private( cOther.m->m_pcDocument, cOther.m->m_psNode );
}

JSON::~JSON()
{
	delete m;
}

JSON& JSON::operator=( const JSON& cOther )
{
	m->Refer( cOther.m->m_pcDocument, cOther.m->m_psNode );
	return *this;
}

/** A new document, with an empty object at the top, checked against a JSON
 * Schema file by IsValid() (see SetSchema()) */
JSON::JSON( const File& cSchema )
{
	m = new Private;
	SetSchema( cSchema );
}

/** The JSON Schema IsValid() checks the document against, from a file.  A
 * file that can't be read or isn't JSON leaves the schema as it was, and is
 * recorded (GetLastError()). */
status_t JSON::SetSchema( const File& cSchema )
{
	File cFile( cSchema );
	off_t nSize = cFile.GetSize();
	if( nSize < 0 || nSize > 0x800000 )
	{
		m->Fail( "the schema file can't be read (or is larger than 8 MB)", NULL );
		return -1;
	}
	std::string cText( (size_t)nSize, '\0' );
	if( nSize > 0 && cFile.ReadPos( 0, &cText[0], nSize ) != nSize )
	{
		m->Fail( "the schema file can't be read", NULL );
		return -1;
	}
	const char* pzEnd = NULL;
	cJSON* psSchema = cJSON_ParseWithOpts( cText.c_str(), &pzEnd, 1 );
	if( psSchema == NULL || !( cJSON_IsObject( psSchema ) || cJSON_IsBool( psSchema ) ) )
	{
		cJSON_Delete( psSchema );
		m->Fail( ( "the schema: " + Private::position( cText.c_str(), pzEnd ? pzEnd : cText.c_str() ) + ": not a JSON object" ).c_str(), NULL );
		return -1;
	}
	cJSON_Delete( m->m_pcDocument->m_psSchema );
	m->m_pcDocument->m_psSchema = psSchema;
	return 0;
}

/** The JSON Schema IsValid() checks the document against (a copy of cSchema) */
status_t JSON::SetSchema( const JSON& cSchema )
{
	cJSON* psSchema = cSchema.m->m_psNode ? cJSON_Duplicate( cSchema.m->m_psNode, 1 ) : NULL;
	if( psSchema == NULL )
	{
		m->Fail( "the schema is empty, or out of memory", NULL );
		return -1;
	}
	cJSON_Delete( m->m_pcDocument->m_psSchema );
	m->m_pcDocument->m_psSchema = psSchema;
	return 0;
}

void JSON::ClearSchema()
{
	cJSON_Delete( m->m_pcDocument->m_psSchema );
	m->m_pcDocument->m_psSchema = NULL;
	m->m_pcDocument->m_cValidationErrors.clear();
}

bool JSON::HasSchema() const
{
	return m->m_pcDocument->m_psSchema != NULL;
}

/** true when the document has no schema, or matches it.  The whole document
 * is checked, whichever of its objects this JSON refers to. */
bool JSON::IsValid() const
{
	m->m_pcDocument->m_cValidationErrors.clear();
	if( m->m_pcDocument->m_psSchema == NULL )
		return true;
	return jsonschema::Validate( m->m_pcDocument->m_psSchema, m->m_pcDocument->m_psRoot, &m->m_pcDocument->m_cValidationErrors );
}

/** Why the last IsValid() was false, one line per problem:
 * "$.window.width: -1 is less than the minimum 0" */
std::vector<String> JSON::GetValidationErrors() const
{
	std::vector<String> cErrors;
	for( size_t i = 0; i < m->m_pcDocument->m_cValidationErrors.size(); i++ )
		cErrors.push_back( String( m->m_pcDocument->m_cValidationErrors[i].c_str() ) );
	return cErrors;
}

/** A copy of this object, as a new document (with the schema) */
JSON JSON::Clone() const
{
	JSON cCopy;
	if( m->m_pcDocument->m_psSchema != NULL )
		cCopy.m->m_pcDocument->m_psSchema = cJSON_Duplicate( m->m_pcDocument->m_psSchema, 1 );
	if( m->m_psNode != NULL )
	{
		cJSON* psCopy = cJSON_Duplicate( m->m_psNode, 1 );
		if( psCopy != NULL )
		{
			cJSON_Delete( cCopy.m->m_pcDocument->m_psRoot );
			cCopy.m->m_pcDocument->m_psRoot = cCopy.m->m_psNode = psCopy;
		}
	}
	return cCopy;
}

/** false for a JSON that refers to nothing (GetObject() of a name that isn't
 * an object, End() of the top-level object) */
bool JSON::IsObject() const
{
	return m->m_psNode != NULL && cJSON_IsObject( m->m_psNode );
}

/** Replaces this object's contents with a JSON text (an object) */
status_t JSON::FromString( const String& cText )
{
	if( !IsObject() )
	{
		m->Fail( "FromString on a JSON that refers to nothing", NULL );
		return -1;
	}
	const char* pzText = cText.c_str();
	const char* pzEnd = NULL;
	cJSON* psParsed = cJSON_ParseWithOpts( pzText, &pzEnd, 1 );
	if( psParsed == NULL )
	{
		m->Fail( ( Private::position( pzText, pzEnd ? pzEnd : pzText ) + ": not valid JSON" ).c_str(), NULL );
		errno = EINVAL;
		return -1;
	}
	if( !cJSON_IsObject( psParsed ) )
	{
		cJSON_Delete( psParsed );
		m->Fail( "the top level is not an object", NULL );
		errno = EINVAL;
		return -1;
	}
	// The parsed members go into this object, so JSONs referring to it stay good
	cJSON_Delete( m->m_psNode->child );
	m->m_psNode->child = psParsed->child;
	psParsed->child = NULL;
	cJSON_Delete( psParsed );
	return 0;
}

/** This object as JSON text, tab-indented or on one line */
String JSON::ToString( bool bFormatted ) const
{
	if( m->m_psNode == NULL )
		return String();
	char* pzText = bFormatted ? cJSON_Print( m->m_psNode ) : cJSON_PrintUnformatted( m->m_psNode );
	if( pzText == NULL )
		return String();
	String cText( pzText );
	cJSON_free( pzText );
	return cText;
}

/** Replaces this object's contents with a JSON file's.  When it isn't there
 * or isn't valid (GetLastError()), the object stays as it was. */
status_t JSON::Load( const String& cPath )
{
	FILE* hFile = fopen( cPath.c_str(), "rb" );
	if( hFile == NULL )
	{
		int nError = errno;
		m->Fail( ( std::string( cPath.c_str() ) + ": " + strerror( nError ) ).c_str(), NULL );
		errno = nError;
		return -1;
	}
	std::string cText;
	char zBuffer[4096];
	size_t nRead;
	while( ( nRead = fread( zBuffer, 1, sizeof( zBuffer ), hFile ) ) > 0 )
		cText.append( zBuffer, nRead );
	bool bError = ferror( hFile );
	fclose( hFile );
	if( bError )
	{
		m->Fail( ( std::string( cPath.c_str() ) + ": read error" ).c_str(), NULL );
		errno = EIO;
		return -1;
	}
	if( FromString( String( cText.c_str() ) ) < 0 )
	{
		m->m_pcDocument->m_cError = std::string( cPath.c_str() ) + ": " + m->m_pcDocument->m_cError;
		return -1;
	}
	return 0;
}

/** Writes this object to a file (making its folders), through a temporary
 * file renamed over it: a crash leaves the old file or the new one */
status_t JSON::Save( const String& cPath ) const
{
	if( m->m_psNode == NULL )
	{
		errno = EINVAL;
		return -1;
	}
	char* pzText = cJSON_Print( m->m_psNode );
	if( pzText == NULL )
	{
		errno = ENOMEM;
		return -1;
	}
	std::string cFile = cPath.c_str();
	for( std::string::size_type n = cFile.find( '/', 1 ); n != std::string::npos; n = cFile.find( '/', n + 1 ) )
	{
		struct stat sStat;
		std::string cDir = cFile.substr( 0, n );
		if( stat( cDir.c_str(), &sStat ) < 0 )
			mkdir( cDir.c_str(), 0700 );
	}
	std::string cTemp = cFile + ".tmp";
	FILE* hFile = fopen( cTemp.c_str(), "wb" );
	if( hFile == NULL )
	{
		cJSON_free( pzText );
		return -1;
	}
	size_t nLen = strlen( pzText );
	bool bOk = fwrite( pzText, 1, nLen, hFile ) == nLen && fputc( '\n', hFile ) != EOF &&
	           fflush( hFile ) == 0 && fsync( fileno( hFile ) ) == 0;
	int nError = errno;
	cJSON_free( pzText );
	if( fclose( hFile ) != 0 && bOk )
	{
		bOk = false;
		nError = errno;
	}
	if( !bOk || rename( cTemp.c_str(), cFile.c_str() ) < 0 )
	{
		if( bOk )
			nError = errno;
		unlink( cTemp.c_str() );
		errno = nError;
		return -1;
	}
	return 0;
}

/** A message as a new document: its names become members (literally: a '.'
 * in a name is kept), several values of a name an array, a message in it an
 * object.  Pointers, raw data and variants are left out, and so are message
 * codes. */
JSON JSON::FromMessage( const Message& cMessage )
{
	JSON cDoc;
	cJSON* psObject = Private::message_to_json( cMessage );
	if( psObject != NULL )
	{
		cJSON_Delete( cDoc.m->m_pcDocument->m_psRoot );
		cDoc.m->m_pcDocument->m_psRoot = cDoc.m->m_psNode = psObject;
	}
	return cDoc;
}

/** Adds this object's members to a message (see FromMessage()): integers
 * become int32 (int64 when larger), other numbers doubles, rect- and
 * point-shaped objects Rect and Point, other objects messages, arrays
 * several values of a name.  Colors stay "#rrggbb" strings
 * (JSON::ParseColor()). */
status_t JSON::ToMessage( Message* pcMessage ) const
{
	if( pcMessage == NULL || !IsObject() )
	{
		errno = EINVAL;
		return -1;
	}
	Private::json_to_message( m->m_psNode, pcMessage );
	return 0;
}

/** The object nIndex of a name, or a JSON that refers to nothing (IsObject()
 * false) when it isn't there or isn't an object */
JSON JSON::GetObject( const char* pzName, int nIndex ) const
{
	cJSON* psItem = Private::lookup( m->m_psNode, pzName, nIndex );
	return JSON( new Private( m->m_pcDocument, cJSON_IsObject( psItem ) ? psItem : NULL ) );
}

/** The object nIndex of a name, made (empty, in place of another value) when
 * it isn't an object yet */
JSON JSON::SetObject( const char* pzName, int nIndex )
{
	cJSON* psItem = Private::lookup( m->m_psNode, pzName, nIndex );
	if( cJSON_IsObject( psItem ) )
		return JSON( new Private( m->m_pcDocument, psItem ) );
	cJSON* psObject = cJSON_CreateObject();
	m->Store( pzName, nIndex, psObject );
	if( Private::lookup( m->m_psNode, pzName, nIndex ) != psObject )
		return JSON( new Private( m->m_pcDocument, NULL ) );
	return JSON( new Private( m->m_pcDocument, psObject ) );
}

/** A new, empty object at the end of the array of a name */
JSON JSON::AddObject( const char* pzName )
{
	int nIndex = GetCount( pzName );
	cJSON* psObject = cJSON_CreateObject();
	m->Store( pzName, nIndex, psObject, true );
	if( Private::lookup( m->m_psNode, pzName, nIndex ) != psObject )
		return JSON( new Private( m->m_pcDocument, NULL ) );
	return JSON( new Private( m->m_pcDocument, psObject ) );
}

/** The object this one is in (for chains: SetObject( "a" ).Set...().End()),
 * or a JSON that refers to nothing at the top */
JSON JSON::End() const
{
	if( m->m_psNode == NULL || m->m_psNode == m->m_pcDocument->m_psRoot )
		return JSON( new Private( m->m_pcDocument, NULL ) );
	return JSON( new Private( m->m_pcDocument, Private::find_owner( m->m_pcDocument->m_psRoot, m->m_pcDocument->m_psRoot, m->m_psNode ) ) );
}

/** The document's top-level object */
JSON JSON::Top() const
{
	return JSON( new Private( m->m_pcDocument, m->m_pcDocument->m_psRoot ) );
}

/** Every value a JSONPath query selects from this object ($), in document
 * order */
std::vector<JSON> JSON::Query( const String& cQuery ) const
{
	std::vector<JSON> cResult;
	std::vector<cJSON*> cMatches;
	std::string cError;
	if( !jsonpath::Query( m->m_psNode, cQuery.c_str(), &cMatches, &cError ) )
	{
		m->m_pcDocument->m_bError = true;
		m->m_pcDocument->m_cError = cError + " in \"" + cQuery.c_str() + "\"";
		return cResult;
	}
	for( size_t i = 0; i < cMatches.size(); i++ )
		cResult.push_back( JSON( new Private( m->m_pcDocument, cMatches[i] ) ) );
	return cResult;
}

/** The first value a JSONPath query selects, or a JSON that refers to
 * nothing (Exists() false) */
JSON JSON::QueryFirst( const String& cQuery ) const
{
	std::vector<JSON> cResult = Query( cQuery );
	return cResult.empty() ? JSON( new Private( m->m_pcDocument, NULL ) ) : cResult[0];
}

/** Whether this JSON refers to a value (of any type) */
bool JSON::Exists() const
{
	return m->m_psNode != NULL;
}

JSON::Type JSON::GetValueType() const
{
	return Private::type_of( m->m_psNode );
}

/** os::Flattenable: T_JSON */
int JSON::GetType() const
{
	return T_JSON;
}

/** The size of this object as compact JSON text, with its NUL */
size_t JSON::GetFlattenedSize() const
{
	if( m->m_psNode == NULL )
		return 3;		// "{}"
	char* pzText = cJSON_PrintUnformatted( m->m_psNode );
	size_t nSize = pzText ? strlen( pzText ) + 1 : 3;
	cJSON_free( pzText );
	return nSize;
}

/** This object as compact JSON text, NUL-terminated */
status_t JSON::Flatten( uint8* pBuffer, size_t nSize ) const
{
	char* pzText = m->m_psNode != NULL ? cJSON_PrintUnformatted( m->m_psNode ) : NULL;
	const char* pzOut = pzText ? pzText : "{}";
	size_t nLen = strlen( pzOut ) + 1;
	status_t nResult = 0;
	if( nLen > nSize )
	{
		errno = ENOBUFS;
		nResult = -1;
	}
	else
		memcpy( pBuffer, pzOut, nLen );
	cJSON_free( pzText );
	return nResult;
}

/** Replaces this object's contents with flattened JSON (as FromString()) */
status_t JSON::Unflatten( const uint8* pBuffer )
{
	if( pBuffer == NULL )
	{
		errno = EINVAL;
		return -1;
	}
	return FromString( String( (const char*)pBuffer ) );
}

/** The member name of this value in its object ("" in an array, or at the top) */
String JSON::GetName() const
{
	return m->m_psNode != NULL && m->m_psNode->string != NULL && m->m_psNode != m->m_pcDocument->m_psRoot ?
	       String( m->m_psNode->string ) : String();
}

/** This value as text: a string, or a number or bool as text */
String JSON::AsString( const char* pzDefault ) const
{
	return Private::value_string( m->m_psNode, pzDefault );
}

/** This value as an integer: a number, a string of digits, or a bool */
int64 JSON::AsInt64( int64 nDefault ) const
{
	return Private::to_int64( m->m_psNode, nDefault );
}

double JSON::AsDouble( double vDefault ) const
{
	return cJSON_IsNumber( m->m_psNode ) ? m->m_psNode->valuedouble : vDefault;
}

/** true or false, or a number (not 0 is true) */
bool JSON::AsBool( bool bDefault ) const
{
	if( cJSON_IsBool( m->m_psNode ) )
		return cJSON_IsTrue( m->m_psNode );
	if( cJSON_IsNumber( m->m_psNode ) )
		return m->m_psNode->valuedouble != 0.0;
	return bDefault;
}

bool JSON::HasValue( const char* pzName, int nIndex ) const
{
	return Private::lookup( m->m_psNode, pzName, nIndex ) != NULL;
}

JSON::Type JSON::GetType( const char* pzName, int nIndex ) const
{
	return Private::type_of( Private::lookup( m->m_psNode, pzName, nIndex ) );
}

/** How many values a name has: the size of its array, 1 for another value,
 * 0 when it isn't there */
int JSON::GetCount( const char* pzName ) const
{
	if( jsonpath::IsQuery( pzName ) )	// the number of matches
	{
		std::vector<cJSON*> cMatches;
		jsonpath::Query( m->m_psNode, pzName, &cMatches, NULL );
		return cMatches.size();
	}
	std::string cLast;
	cJSON* psParent = Private::walk( m->m_psNode, pzName, false, &cLast );
	if( psParent == NULL )
		return 0;
	cJSON* psItem = cJSON_GetObjectItemCaseSensitive( psParent, cLast.c_str() );
	if( psItem == NULL )
		return 0;
	return cJSON_IsArray( psItem ) ? cJSON_GetArraySize( psItem ) : 1;
}

/** The number of names in this object */
int JSON::CountMembers() const
{
	return cJSON_IsObject( m->m_psNode ) ? cJSON_GetArraySize( m->m_psNode ) : 0;
}

/** The name nIndex of this object, in the file's order */
String JSON::GetMemberName( int nIndex ) const
{
	if( !cJSON_IsObject( m->m_psNode ) || nIndex < 0 )
		return String();
	cJSON* psItem = cJSON_GetArrayItem( m->m_psNode, nIndex );
	return psItem != NULL && psItem->string != NULL ? String( psItem->string ) : String();
}

/** Removes a name (nIndex -1), or one element of its array.  Removing what
 * isn't there is not an error. */
JSON& JSON::RemoveValue( const char* pzName, int nIndex )
{
	if( jsonpath::IsQuery( pzName ) )	// every match (nIndex -1), or match nIndex
	{
		std::vector<cJSON*> cMatches;
		std::string cError;
		if( m->m_psNode == NULL || !jsonpath::Query( m->m_psNode, pzName, &cMatches, &cError ) )
			{
			m->Fail( ( cError + " in" ).c_str(), pzName );
			return *this;
		}
		if( nIndex >= 0 )
		{
			if( nIndex >= (int)cMatches.size() )
				return *this;
			cMatches = std::vector<cJSON*>( 1, cMatches[nIndex] );
		}
		// Later matches first (descendants before what they are in); a match
		// inside one already removed is no longer found, and is skipped
		for( size_t i = cMatches.size(); i-- > 0; )
		{
			cJSON* psContainer = Private::find_container( m->m_psNode, cMatches[i] );
			if( psContainer != NULL )
				cJSON_Delete( cJSON_DetachItemViaPointer( psContainer, cMatches[i] ) );
		}
		return *this;
	}
	std::string cLast;
	cJSON* psParent = Private::walk( m->m_psNode, pzName, false, &cLast );
	cJSON* psItem = psParent ? cJSON_GetObjectItemCaseSensitive( psParent, cLast.c_str() ) : NULL;
	if( psItem == NULL )
		return *this;
	if( nIndex < 0 || ( nIndex == 0 && !cJSON_IsArray( psItem ) ) )
		cJSON_DeleteItemFromObjectCaseSensitive( psParent, cLast.c_str() );
	else if( cJSON_IsArray( psItem ) && nIndex < cJSON_GetArraySize( psItem ) )
		cJSON_DeleteItemFromArray( psItem, nIndex );
	return *this;
}

/** Removes every member of this object */
JSON& JSON::MakeEmpty()
{
	if( IsObject() )
	{
		cJSON_Delete( m->m_psNode->child );
		m->m_psNode->child = NULL;
	}
	return *this;
}

/** A string, or a number or bool as text */
String JSON::GetString( const char* pzName, const char* pzDefault, int nIndex ) const
{
	return Private::value_string( Private::lookup( m->m_psNode, pzName, nIndex ), pzDefault );
}

int8 JSON::GetInt8( const char* pzName, int8 nDefault, int nIndex ) const
{
	return (int8)GetInt64( pzName, nDefault, nIndex );
}

int16 JSON::GetInt16( const char* pzName, int16 nDefault, int nIndex ) const
{
	return (int16)GetInt64( pzName, nDefault, nIndex );
}

int32 JSON::GetInt32( const char* pzName, int32 nDefault, int nIndex ) const
{
	return (int32)GetInt64( pzName, nDefault, nIndex );
}

/** A number, a string of digits (large values), or a bool (1 or 0) */
int64 JSON::GetInt64( const char* pzName, int64 nDefault, int nIndex ) const
{
	return Private::to_int64( Private::lookup( m->m_psNode, pzName, nIndex ), nDefault );
}

/** true or false, or a number (not 0 is true) */
bool JSON::GetBool( const char* pzName, bool bDefault, int nIndex ) const
{
	const cJSON* psItem = Private::lookup( m->m_psNode, pzName, nIndex );
	if( cJSON_IsBool( psItem ) )
		return cJSON_IsTrue( psItem );
	if( cJSON_IsNumber( psItem ) )
		return psItem->valuedouble != 0.0;
	return bDefault;
}

float JSON::GetFloat( const char* pzName, float vDefault, int nIndex ) const
{
	return (float)GetDouble( pzName, vDefault, nIndex );
}

double JSON::GetDouble( const char* pzName, double vDefault, int nIndex ) const
{
	const cJSON* psItem = Private::lookup( m->m_psNode, pzName, nIndex );
	return cJSON_IsNumber( psItem ) ? psItem->valuedouble : vDefault;
}

/** { "left", "top", "right", "bottom" }, or [ left, top, right, bottom ] */
Rect JSON::GetRect( const char* pzName, const Rect& cDefault, int nIndex ) const
{
	double av[4];
	if( !Private::get_numbers( Private::lookup( m->m_psNode, pzName, nIndex ), Private::g_apzRect, 4, av ) )
		return cDefault;
	return Rect( av[0], av[1], av[2], av[3] );
}

IRect JSON::GetIRect( const char* pzName, const IRect& cDefault, int nIndex ) const
{
	double av[4];
	if( !Private::get_numbers( Private::lookup( m->m_psNode, pzName, nIndex ), Private::g_apzRect, 4, av ) )
		return cDefault;
	return IRect( (int)av[0], (int)av[1], (int)av[2], (int)av[3] );
}

/** { "x", "y" }, or [ x, y ] */
Point JSON::GetPoint( const char* pzName, const Point& cDefault, int nIndex ) const
{
	double av[2];
	if( !Private::get_numbers( Private::lookup( m->m_psNode, pzName, nIndex ), Private::g_apzPoint, 2, av ) )
		return cDefault;
	return Point( av[0], av[1] );
}

IPoint JSON::GetIPoint( const char* pzName, const IPoint& cDefault, int nIndex ) const
{
	double av[2];
	if( !Private::get_numbers( Private::lookup( m->m_psNode, pzName, nIndex ), Private::g_apzPoint, 2, av ) )
		return cDefault;
	return IPoint( (int)av[0], (int)av[1] );
}

/** "#rrggbb", "#rrggbbaa" or "#rgb", or { "red", "green", "blue"[, "alpha"] } */
Color32_s JSON::GetColor32( const char* pzName, const Color32_s& cDefault, int nIndex ) const
{
	const cJSON* psItem = Private::lookup( m->m_psNode, pzName, nIndex );
	Color32_s sColor;
	if( cJSON_IsString( psItem ) && Private::parse_color_hex( psItem->valuestring, &sColor ) )
		return sColor;
	double av[4];
	if( cJSON_IsObject( psItem ) && Private::get_numbers( psItem, Private::g_apzColor, 3, av ) )
	{
		if( !Private::get_number( psItem, "alpha", &av[3] ) )
			av[3] = 255;
		return Color32_s( (int)av[0], (int)av[1], (int)av[2], (int)av[3] );
	}
	return cDefault;
}

bool JSON::ParseColor( const char* pzText, Color32_s* psColor )
{
	return pzText != NULL && psColor != NULL && Private::parse_color_hex( pzText, psColor );
}

JSON& JSON::SetString( const char* pzName, const String& cValue, int nIndex )
{
	m->Store( pzName, nIndex, cJSON_CreateString( cValue.c_str() ) );
	return *this;
}

JSON& JSON::SetInt8( const char* pzName, int8 nValue, int nIndex )
{
	return SetInt64( pzName, nValue, nIndex );
}

JSON& JSON::SetInt16( const char* pzName, int16 nValue, int nIndex )
{
	return SetInt64( pzName, nValue, nIndex );
}

JSON& JSON::SetInt32( const char* pzName, int32 nValue, int nIndex )
{
	return SetInt64( pzName, nValue, nIndex );
}

/** A number, or beyond 2^53 (where doubles lose digits) a string of digits */
JSON& JSON::SetInt64( const char* pzName, int64 nValue, int nIndex )
{
	m->Store( pzName, nIndex, Private::number_or_string( nValue ) );
	return *this;
}

JSON& JSON::SetBool( const char* pzName, bool bValue, int nIndex )
{
	m->Store( pzName, nIndex, cJSON_CreateBool( bValue ) );
	return *this;
}

/** Saved as the shortest number that reads back as the same float (0.1, not
 * 0.10000000149011612) */
JSON& JSON::SetFloat( const char* pzName, float vValue, int nIndex )
{
	m->Store( pzName, nIndex, cJSON_CreateNumber( Private::float_to_double( vValue ) ) );
	return *this;
}

JSON& JSON::SetDouble( const char* pzName, double vValue, int nIndex )
{
	m->Store( pzName, nIndex, cJSON_CreateNumber( vValue ) );
	return *this;
}

JSON& JSON::SetRect( const char* pzName, const Rect& cValue, int nIndex )
{
	double av[4] = { Private::float_to_double( cValue.left ), Private::float_to_double( cValue.top ),
	                 Private::float_to_double( cValue.right ), Private::float_to_double( cValue.bottom ) };
	m->Store( pzName, nIndex, Private::make_numbers( Private::g_apzRect, 4, av ) );
	return *this;
}

JSON& JSON::SetIRect( const char* pzName, const IRect& cValue, int nIndex )
{
	double av[4] = { (double)cValue.left, (double)cValue.top, (double)cValue.right, (double)cValue.bottom };
	m->Store( pzName, nIndex, Private::make_numbers( Private::g_apzRect, 4, av ) );
	return *this;
}

JSON& JSON::SetPoint( const char* pzName, const Point& cValue, int nIndex )
{
	double av[2] = { Private::float_to_double( cValue.x ), Private::float_to_double( cValue.y ) };
	m->Store( pzName, nIndex, Private::make_numbers( Private::g_apzPoint, 2, av ) );
	return *this;
}

JSON& JSON::SetIPoint( const char* pzName, const IPoint& cValue, int nIndex )
{
	double av[2] = { (double)cValue.x, (double)cValue.y };
	m->Store( pzName, nIndex, Private::make_numbers( Private::g_apzPoint, 2, av ) );
	return *this;
}

/** "#rrggbb", with the alpha ("#rrggbbaa") when it isn't 255 */
JSON& JSON::SetColor32( const char* pzName, const Color32_s& cValue, int nIndex )
{
	m->Store( pzName, nIndex, Private::color_string( cValue ) );
	return *this;
}

JSON& JSON::SetNull( const char* pzName, int nIndex )
{
	m->Store( pzName, nIndex, cJSON_CreateNull() );
	return *this;
}

JSON& JSON::AddString( const char* pzName, const String& cValue )
{
	m->Store( pzName, GetCount( pzName ), cJSON_CreateString( cValue.c_str() ), true );
	return *this;
}

JSON& JSON::AddInt32( const char* pzName, int32 nValue )
{
	m->Store( pzName, GetCount( pzName ), cJSON_CreateNumber( nValue ), true );
	return *this;
}

JSON& JSON::AddInt64( const char* pzName, int64 nValue )
{
	m->Store( pzName, GetCount( pzName ), Private::number_or_string( nValue ), true );
	return *this;
}

JSON& JSON::AddBool( const char* pzName, bool bValue )
{
	m->Store( pzName, GetCount( pzName ), cJSON_CreateBool( bValue ), true );
	return *this;
}

JSON& JSON::AddDouble( const char* pzName, double vValue )
{
	m->Store( pzName, GetCount( pzName ), cJSON_CreateNumber( vValue ), true );
	return *this;
}

bool JSON::HasError() const
{
	return m->m_pcDocument->m_bError;
}

String JSON::GetLastError() const
{
	return String( m->m_pcDocument->m_cError.c_str() );
}

void JSON::ClearError()
{
	m->m_pcDocument->m_bError = false;
	m->m_pcDocument->m_cError.clear();
}
