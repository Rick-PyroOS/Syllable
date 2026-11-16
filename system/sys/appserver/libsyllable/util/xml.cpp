/*  XML - XML documents with the Get/Set methods of os::Settings, on libxml2.
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

#include <util/xml.h>
#include <util/message.h>
#include <util/locker.h>

#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/xmlsave.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>
#include <libxml/valid.h>
#include <libxml/xmlschemas.h>
#include <libxml/relaxng.h>

#include <string>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>

using namespace os;

// ---- libxml2 set-up, serialization and errors ---------------------------------
//
// libsyllable's libxml2 is its own (static, its symbols hidden), built without
// threads: its globals (the error handlers, the entity loader) are set once
// here, and every XML call holds g_cXMLLock while it uses libxml2.

namespace
{

Locker g_cXMLLock( "os::XML", true );

// Where libxml2's errors go while a call collects them (Capture), or nowhere
std::vector<std::string>* g_pcErrors = NULL;

void structured_error( void*, const xmlError* psError )
{
	if( g_pcErrors == NULL || psError == NULL || psError->level == XML_ERR_WARNING )
		return;
	std::string cMessage = psError->message ? psError->message : "error";
	while( !cMessage.empty() && ( cMessage[cMessage.size() - 1] == '\n' || cMessage[cMessage.size() - 1] == ' ' ) )
		cMessage.resize( cMessage.size() - 1 );
	std::string cWhere;
	xmlNodePtr psNode = (xmlNodePtr)psError->node;
	if( psNode != NULL && psNode->doc != NULL &&
	    ( psNode->type == XML_ELEMENT_NODE || psNode->type == XML_ATTRIBUTE_NODE ) )
	{
		xmlChar* pzPath = xmlGetNodePath( psNode );
		if( pzPath != NULL )
		{
			cWhere = (const char*)pzPath;
			xmlFree( pzPath );
		}
	}
	if( cWhere.empty() && psError->line > 0 )
	{
		char zBuffer[64];
		if( psError->int2 > 0 )
			snprintf( zBuffer, sizeof( zBuffer ), "line %d, column %d", psError->line, psError->int2 );
		else
			snprintf( zBuffer, sizeof( zBuffer ), "line %d", psError->line );
		cWhere = zBuffer;
	}
	g_pcErrors->push_back( cWhere.empty() ? cMessage : cWhere + ": " + cMessage );
}

void generic_error( void*, const char*, ... )
{
}

// Nothing outside the text given is read: no external DTDs or entities, no
// xs:include, xs:import or Relax-NG includes, no network
xmlParserInputPtr no_entity_loader( const char* pzURL, const char*, xmlParserCtxtPtr )
{
	if( g_pcErrors != NULL )
		g_pcErrors->push_back( std::string( "not loaded (os::XML reads no other files): " ) + ( pzURL ? pzURL : "" ) );
	return NULL;
}

/* Holds the lock for one XML call (calls in calls take it again) */
class Guard
{
public:
	Guard()
	{
		g_cXMLLock.Lock();
		static bool bInitialized = false;
		if( !bInitialized )
		{
			bInitialized = true;
			xmlInitParser();
			xmlSetGenericErrorFunc( NULL, generic_error );
			xmlSetStructuredErrorFunc( NULL, structured_error );
			xmlSetExternalEntityLoader( no_entity_loader );
		}
	}
	~Guard() { g_cXMLLock.Unlock(); }
};

/* Collects the errors libxml2 reports while it lives */
class Capture
{
public:
	Capture() : m_pcOuter( g_pcErrors ) { g_pcErrors = &m_cErrors; }
	~Capture() { g_pcErrors = m_pcOuter; }
	std::string First( const char* pzDefault ) const { return m_cErrors.empty() ? std::string( pzDefault ) : m_cErrors[0]; }
	std::vector<std::string> m_cErrors;
private:
	std::vector<std::string>* m_pcOuter;
};

const int PARSE_OPTIONS = XML_PARSE_NONET | XML_PARSE_NOBLANKS | XML_PARSE_NO_XXE;

const xmlChar* X( const char* pz ) { return (const xmlChar*)pz; }
const char* C( const xmlChar* pz ) { return (const char*)pz; }

/* A text as a document, or NULL (*pcError says why) */
xmlDocPtr parse_document( const char* pzText, size_t nLength, std::string* pcError )
{
	Capture cErrors;
	xmlDocPtr psDoc = xmlReadMemory( pzText, (int)nLength, NULL, NULL, PARSE_OPTIONS );
	if( psDoc != NULL && xmlDocGetRootElement( psDoc ) == NULL )
	{
		xmlFreeDoc( psDoc );
		psDoc = NULL;
	}
	if( psDoc == NULL )
		*pcError = "not well-formed XML: " + cErrors.First( "no element" );
	return psDoc;
}

/* A node and what is in it as text (UTF-8); a whole document with its XML
 * declaration */
std::string serialize( xmlDocPtr psDoc, xmlNodePtr psNode, bool bFormatted )
{
	xmlBufferPtr psBuffer = xmlBufferCreate();
	if( psBuffer == NULL )
		return std::string();
	int nOptions = bFormatted ? XML_SAVE_FORMAT : 0;
	if( psNode != NULL )
		nOptions |= XML_SAVE_NO_DECL;
	xmlSaveCtxtPtr psSave = xmlSaveToBuffer( psBuffer, "UTF-8", nOptions );
	std::string cText;
	if( psSave != NULL )
	{
		if( psNode != NULL )
			xmlSaveTree( psSave, psNode );
		else
			xmlSaveDoc( psSave, psDoc );
		xmlSaveClose( psSave );
		cText.assign( C( xmlBufferContent( psBuffer ) ), xmlBufferLength( psBuffer ) );
	}
	xmlBufferFree( psBuffer );
	if( psNode != NULL && bFormatted )
		cText += '\n';
	return cText;
}

/* A new document whose root element is a copy of psElement (with the
 * namespaces it uses declared on it) */
xmlDocPtr copy_as_document( xmlNodePtr psElement )
{
	xmlDocPtr psDoc = xmlNewDoc( X( "1.0" ) );
	if( psDoc == NULL )
		return NULL;
	xmlNodePtr psCopy = xmlDocCopyNode( psElement, psDoc, 1 );
	if( psCopy == NULL )
	{
		xmlFreeDoc( psDoc );
		return NULL;
	}
	xmlDocSetRootElement( psDoc, psCopy );
	return psDoc;
}

} // anonymous namespace

// XML::Private stays out of libsyllable's ABI (a nested class has the
// visibility of the class it is in unless it says otherwise)

/** \internal The implementation of XML: the document a handle refers into,
 * the node it refers to, and the helpers */
class __attribute__((visibility("hidden"))) XML::Private
{
public:
	enum SchemaKind { SCHEMA_NONE, SCHEMA_XSD, SCHEMA_RELAXNG, SCHEMA_DTD };

	/* The libxml2 document and its schema, shared by the handles into it and
	 * freed with the last of them */
	class __attribute__((visibility("hidden"))) Document
	{
	public:
		Document( xmlDocPtr psDoc ) : m_psDoc( psDoc ), m_nSchema( SCHEMA_NONE ), m_psSchemaDoc( NULL ),
			m_psXSD( NULL ), m_psRelaxNG( NULL ), m_psDTD( NULL ), m_nRefs( 0 ), m_bError( false ) {}
		~Document() { FreeSchema(); xmlFreeDoc( m_psDoc ); }

		void FreeSchema()
		{
			if( m_psXSD != NULL )		xmlSchemaFree( m_psXSD );
			if( m_psRelaxNG != NULL )	xmlRelaxNGFree( m_psRelaxNG );
			if( m_psDTD != NULL )		xmlFreeDtd( m_psDTD );
			if( m_psSchemaDoc != NULL )	xmlFreeDoc( m_psSchemaDoc );
			m_psXSD = NULL;
			m_psRelaxNG = NULL;
			m_psDTD = NULL;
			m_psSchemaDoc = NULL;
			m_nSchema = SCHEMA_NONE;
			m_cSchemaText.clear();
			m_cValidationErrors.clear();
		}

		xmlDocPtr		m_psDoc;
		SchemaKind		m_nSchema;
		std::string		m_cSchemaText;		// for Clone()
		xmlDocPtr		m_psSchemaDoc;		// the XSD's or Relax-NG schema's document
		xmlSchemaPtr	m_psXSD;
		xmlRelaxNGPtr	m_psRelaxNG;
		xmlDtdPtr		m_psDTD;
		std::vector<std::string> m_cValidationErrors;
		std::vector<std::pair<std::string, std::string> > m_cNamespaces;	// RegisterNamespace()
		int m_nRefs;
		bool m_bError;
		std::string m_cError;
	};

	/* A new document, with an empty root element */
	Private( const char* pzRootName ) : m_pcDocument( NULL ), m_psNode( NULL ), m_bRoot( true )
	{
		bool bValid = pzRootName != NULL && xmlValidateNCName( X( pzRootName ), 0 ) == 0;
		xmlDocPtr psDoc = xmlNewDoc( X( "1.0" ) );
		xmlDocSetRootElement( psDoc, xmlNewDocNode( psDoc, NULL, X( bValid ? pzRootName : "root" ), NULL ) );
		Refer( new Document( psDoc ), NULL, true );
		if( !bValid )
			Fail( "not an XML name (the root element is <root>):", pzRootName ? pzRootName : "" );
	}
	Private( Document* pcDocument, xmlNodePtr psNode, bool bRoot ) : m_pcDocument( NULL ), m_psNode( NULL ), m_bRoot( false )
	{
		Refer( pcDocument, psNode, bRoot );
	}
	~Private() { Release(); }

	/* Refers to psNode in pcDocument (in place of what it referred to).  A
	 * handle to the root element finds it anew each time (bRoot), so it stays
	 * good when Load() or FromString() replaces the document. */
	void Refer( Document* pcDocument, xmlNodePtr psNode, bool bRoot )
	{
		pcDocument->m_nRefs++;
		Release();
		m_pcDocument = pcDocument;
		m_psNode = bRoot ? NULL : psNode;
		m_bRoot = bRoot;
	}

	void Release()
	{
		if( m_pcDocument != NULL && --m_pcDocument->m_nRefs == 0 )
			delete m_pcDocument;
		m_pcDocument = NULL;
	}

	xmlDocPtr Doc() const { return m_pcDocument->m_psDoc; }
	xmlNodePtr Node() const { return m_bRoot ? xmlDocGetRootElement( Doc() ) : m_psNode; }
	xmlNodePtr Element() const { xmlNodePtr p = Node(); return is_element( p ) ? p : NULL; }
	bool IsRoot() const { return m_bRoot; }

	/* A handle to a node of this document (the root element as such) */
	XML Make( xmlNodePtr psNode ) const
	{
		if( psNode != NULL && ( psNode == xmlDocGetRootElement( Doc() ) || psNode->type == XML_DOCUMENT_NODE ) )
			return XML( new Private( m_pcDocument, NULL, true ) );
		return XML( new Private( m_pcDocument, psNode, false ) );
	}

	/* Records an error (GetLastError()) */
	void Fail( const char* pzWhat, const char* pzName = NULL )
	{
		m_pcDocument->m_bError = true;
		m_pcDocument->m_cError = std::string( pzWhat ) + ( pzName ? std::string( " \"" ) + pzName + "\"" : std::string() );
	}
	void Fail( const std::string& cWhat ) { Fail( cWhat.c_str() ); }

	Document*	m_pcDocument;
	xmlNodePtr	m_psNode;		// NULL: refers to nothing (unless m_bRoot)
	bool		m_bRoot;

	// -- Values --

	/* What a Set stores: text, and for rects and points numbers that are
	 * attributes of an element (and the text, for an attribute) */
	struct Value
	{
		Value( const std::string& cText ) : m_cText( cText ), m_apzNames( NULL ), m_nCount( 0 ) {}
		Value( const char* const* apzNames, int nCount, const double* av ) : m_apzNames( apzNames ), m_nCount( nCount )
		{
			for( int i = 0; i < nCount; i++ )
			{
				m_av[i] = av[i];
				m_cText += ( i ? " " : "" ) + number_text( av[i] );
			}
		}
		std::string m_cText;
		const char* const* m_apzNames;	// NULL: text
		int m_nCount;
		double m_av[4];
	};

	static const char* const g_apzRect[];
	static const char* const g_apzPoint[];
	static const char* const g_apzColor[];

	static bool is_element( const xmlNode* p ) { return p != NULL && p->type == XML_ELEMENT_NODE; }

	/* The shortest of 15 or 17 digits that reads back the same (8.95, not
	 * 8.9499999999999993); integers without a point */
	static std::string number_text( double v )
	{
		char zBuffer[40];
		if( v != v )
			return "NaN";
		if( v > 1.7976931348623157e308 )
			return "INF";
		if( v < -1.7976931348623157e308 )
			return "-INF";
		if( v < 9007199254740992.0 && v > -9007199254740992.0 && v == (double)(int64)v )
			snprintf( zBuffer, sizeof( zBuffer ), "%lld", (long long)v );
		else
		{
			snprintf( zBuffer, sizeof( zBuffer ), "%.15g", v );
			if( strtod( zBuffer, NULL ) != v )
				snprintf( zBuffer, sizeof( zBuffer ), "%.17g", v );
		}
		return zBuffer;
	}

	/* A float as the shortest decimal that reads back as the same float, so
	 * 0.1f is saved as 0.1 rather than 0.100000001 */
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

	static std::string int_text( int64 n )
	{
		char zBuffer[32];
		snprintf( zBuffer, sizeof( zBuffer ), "%lld", (long long)n );
		return zBuffer;
	}

	static std::string color_text( const Color32_s& sColor )
	{
		char zBuffer[16];
		if( sColor.alpha == 255 )
			snprintf( zBuffer, sizeof( zBuffer ), "#%02x%02x%02x", sColor.red, sColor.green, sColor.blue );
		else
			snprintf( zBuffer, sizeof( zBuffer ), "#%02x%02x%02x%02x", sColor.red, sColor.green, sColor.blue, sColor.alpha );
		return zBuffer;
	}

	static std::string trim( const std::string& c )
	{
		static const char* const pzSpace = " \t\r\n";
		std::string::size_type nStart = c.find_first_not_of( pzSpace );
		if( nStart == std::string::npos )
			return std::string();
		return c.substr( nStart, c.find_last_not_of( pzSpace ) - nStart + 1 );
	}

	/* The whole text is a number */
	static bool to_double( const std::string& cText, double* pv )
	{
		std::string c = trim( cText );
		if( c.empty() )
			return false;
		char* pzEnd;
		double v = strtod( c.c_str(), &pzEnd );
		if( *pzEnd != '\0' )
			return false;
		*pv = v;
		return true;
	}

	static bool to_bool( const std::string& cText, bool* pb )
	{
		std::string c = trim( cText );
		for( size_t i = 0; i < c.size(); i++ )
			c[i] = tolower( c[i] );
		if( c == "true" || c == "yes" || c == "on" || c == "1" )
			*pb = true;
		else if( c == "false" || c == "no" || c == "off" || c == "0" )
			*pb = false;
		else
		{
			double v;
			if( !to_double( c, &v ) || v != v )
				return false;
			*pb = v != 0.0;
		}
		return true;
	}

	/* An integer: digits, a number (its whole part), or a bool (1 or 0) */
	static bool to_int64( const std::string& cText, int64* pn )
	{
		std::string c = trim( cText );
		if( c.empty() )
			return false;
		char* pzEnd;
		errno = 0;
		long long n = strtoll( c.c_str(), &pzEnd, 10 );
		if( *pzEnd == '\0' && errno == 0 )
		{
			*pn = n;
			return true;
		}
		double v;
		if( to_double( c, &v ) )
		{
			if( v >= 9223372036854775807.0 || v < -9223372036854775807.0 || v != v )
				return false;
			*pn = (int64)v;
			return true;
		}
		bool b;
		if( to_bool( c, &b ) )
		{
			*pn = b ? 1 : 0;
			return true;
		}
		return false;
	}

	/* nCount numbers in a text, apart by spaces or commas */
	static bool parse_numbers( const std::string& cText, int nCount, double* av )
	{
		const char* p = cText.c_str();
		for( int i = 0; i < nCount; i++ )
		{
			while( *p == ' ' || *p == '\t' || *p == '\n' || *p == '\r' || ( i > 0 && *p == ',' ) )
				p++;
			char* pzEnd;
			av[i] = strtod( p, &pzEnd );
			if( pzEnd == p )
				return false;
			p = pzEnd;
		}
		while( *p == ' ' || *p == '\t' || *p == '\n' || *p == '\r' )
			p++;
		return *p == '\0';
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

	// -- Nodes --

	/* The text of a node: an element's (all the text in it), an
	 * attribute's value, a text's or comment's content */
	static bool node_text( const xmlNode* psNode, std::string* pcText )
	{
		if( psNode == NULL || psNode->type == XML_NAMESPACE_DECL )
			return false;
		xmlChar* pz = xmlNodeGetContent( psNode );
		pcText->assign( pz ? C( pz ) : "" );
		xmlFree( pz );
		return true;
	}

	/* "prefix:local" or "local" */
	static std::string qualified_name( const xmlNode* psNode )
	{
		if( psNode == NULL || psNode->name == NULL )
			return std::string();
		if( ( psNode->type == XML_ELEMENT_NODE || psNode->type == XML_ATTRIBUTE_NODE ) &&
		    psNode->ns != NULL && psNode->ns->prefix != NULL )
			return std::string( C( psNode->ns->prefix ) ) + ":" + C( psNode->name );
		return C( psNode->name );
	}

	/* An element or attribute name (prefixed: the prefix matches, else it has
	 * none).  Attributes and elements share the layout up to ns. */
	static bool matches( const xmlNode* psNode, const std::string& cPart )
	{
		std::string::size_type nColon = cPart.find( ':' );
		if( nColon == std::string::npos )
			return xmlStrEqual( psNode->name, X( cPart.c_str() ) ) && ( psNode->ns == NULL || psNode->ns->prefix == NULL );
		return psNode->ns != NULL && psNode->ns->prefix != NULL &&
		       xmlStrEqual( psNode->name, X( cPart.c_str() + nColon + 1 ) ) &&
		       cPart.compare( 0, nColon, C( psNode->ns->prefix ) ) == 0;
	}

	/* "local" or "prefix:local", each an XML name without ':' */
	static bool valid_name( const std::string& cName )
	{
		std::string::size_type nColon = cName.find( ':' );
		if( nColon == std::string::npos )
			return xmlValidateNCName( X( cName.c_str() ), 0 ) == 0;
		return xmlValidateNCName( X( cName.substr( 0, nColon ).c_str() ), 0 ) == 0 &&
		       xmlValidateNCName( X( cName.c_str() + nColon + 1 ), 0 ) == 0;
	}

	static xmlNodePtr find_child( xmlNodePtr psParent, const std::string& cPart, int nIndex )
	{
		if( psParent == NULL || nIndex < 0 )
			return NULL;
		for( xmlNodePtr p = psParent->children; p != NULL; p = p->next )
			if( p->type == XML_ELEMENT_NODE && matches( p, cPart ) && nIndex-- == 0 )
				return p;
		return NULL;
	}

	static int count_children( xmlNodePtr psParent, const std::string& cPart )
	{
		int nCount = 0;
		for( xmlNodePtr p = psParent ? psParent->children : NULL; p != NULL; p = p->next )
			if( p->type == XML_ELEMENT_NODE && matches( p, cPart ) )
				nCount++;
		return nCount;
	}

	static xmlNodePtr find_attribute( xmlNodePtr psElement, const std::string& cName )
	{
		if( !is_element( psElement ) )
			return NULL;
		for( xmlAttrPtr p = psElement->properties; p != NULL; p = p->next )
			if( matches( (xmlNodePtr)p, cName ) )
				return (xmlNodePtr)p;
		return NULL;
	}

	/* The namespace of a prefixed name at psElement, or (no prefix) the
	 * default namespace there (bDefault, for elements) */
	static bool find_ns( xmlDocPtr psDoc, xmlNodePtr psElement, const std::string& cName, bool bDefault, xmlNsPtr* ppsNs, std::string* pcLocal )
	{
		std::string::size_type nColon = cName.find( ':' );
		if( nColon == std::string::npos )
		{
			*ppsNs = bDefault ? xmlSearchNs( psDoc, psElement, NULL ) : NULL;
			*pcLocal = cName;
			return true;
		}
		*ppsNs = xmlSearchNs( psDoc, psElement, X( cName.substr( 0, nColon ).c_str() ) );
		*pcLocal = cName.substr( nColon + 1 );
		return *ppsNs != NULL;
	}

	/* A new element after the last one of the same name in psParent (or at its
	 * end), or NULL when the prefix isn't declared */
	static xmlNodePtr new_child( xmlDocPtr psDoc, xmlNodePtr psParent, const std::string& cPart )
	{
		xmlNsPtr psNs;
		std::string cLocal;
		if( !find_ns( psDoc, psParent, cPart, true, &psNs, &cLocal ) )
			return NULL;
		xmlNodePtr psNew = xmlNewDocNode( psDoc, psNs, X( cLocal.c_str() ), NULL );
		if( psNew == NULL )
			return NULL;
		xmlNodePtr psLast = NULL;
		for( xmlNodePtr p = psParent->children; p != NULL; p = p->next )
			if( p->type == XML_ELEMENT_NODE && matches( p, cPart ) )
				psLast = p;
		if( psLast != NULL )
			xmlAddNextSibling( psLast, psNew );
		else
			xmlAddChild( psParent, psNew );
		return psNew;
	}

	static void free_children( xmlNodePtr psElement )
	{
		xmlNodePtr p = psElement->children;
		while( p != NULL )
		{
			xmlNodePtr psNext = p->next;
			xmlUnlinkNode( p );
			xmlFreeNode( p );
			p = psNext;
		}
	}

	static bool set_attribute( xmlDocPtr psDoc, xmlNodePtr psElement, const std::string& cName, const std::string& cValue )
	{
		xmlNsPtr psNs;
		std::string cLocal;
		if( !find_ns( psDoc, psElement, cName, false, &psNs, &cLocal ) )
			return false;
		return xmlSetNsProp( psElement, psNs, X( cLocal.c_str() ), X( cValue.c_str() ) ) != NULL;
	}

	/* Replaces what is in an element with a value */
	static void set_content( xmlDocPtr psDoc, xmlNodePtr psElement, const Value& cValue )
	{
		free_children( psElement );
		if( cValue.m_apzNames != NULL )
		{
			for( int i = 0; i < cValue.m_nCount; i++ )
				set_attribute( psDoc, psElement, cValue.m_apzNames[i], number_text( cValue.m_av[i] ) );
		}
		else if( !cValue.m_cText.empty() )
			xmlAddChild( psElement, xmlNewDocTextLen( psDoc, X( cValue.m_cText.c_str() ), cValue.m_cText.size() ) );
	}

	/* Removes a node; the root element stays */
	static void remove_node( xmlDocPtr psDoc, xmlNodePtr psNode )
	{
		if( psNode == NULL || psNode == xmlDocGetRootElement( psDoc ) )
			return;
		if( psNode->type == XML_ATTRIBUTE_NODE )
			xmlRemoveProp( (xmlAttrPtr)psNode );
		else if( psNode->type != XML_DOCUMENT_NODE && psNode->type != XML_NAMESPACE_DECL &&
		         psNode->type != XML_DTD_NODE )
		{
			xmlUnlinkNode( psNode );
			xmlFreeNode( psNode );
		}
	}

	// -- Names and paths --

	/* "a.b.c@attr": the element names and the attribute ("" none) */
	struct Path
	{
		std::vector<std::string> m_cElements;
		std::string m_cAttribute;
		bool m_bAttribute;
	};

	static bool parse_path( const char* pzName, Path* pcPath )
	{
		if( pzName == NULL )
			return false;
		std::string cName( pzName );
		std::string::size_type nAt = cName.find( '@' );
		pcPath->m_bAttribute = nAt != std::string::npos;
		std::string cElements = cName.substr( 0, nAt );
		if( pcPath->m_bAttribute )
		{
			pcPath->m_cAttribute = cName.substr( nAt + 1 );
			if( pcPath->m_cAttribute.empty() || pcPath->m_cAttribute.find_first_of( "@." ) != std::string::npos )
				return false;
		}
		if( cElements.empty() )
			return true;
		std::string::size_type nStart = 0;
		for( ;; )
		{
			std::string::size_type nDot = cElements.find( '.', nStart );
			std::string cPart = cElements.substr( nStart, nDot == std::string::npos ? std::string::npos : nDot - nStart );
			if( cPart.empty() )
				return false;
			pcPath->m_cElements.push_back( cPart );
			if( nDot == std::string::npos )
				break;
			nStart = nDot + 1;
		}
		return true;
	}

	static bool is_query( const char* pz )
	{
		return pz != NULL && ( pz[0] == '/' || ( pz[0] == '.' && ( pz[1] == '/' || ( pz[1] == '.' && pz[2] == '/' ) ) ) );
	}

	/* Runs an XPath expression from psContext: the nodes it selects (in
	 * document order; namespace nodes left out), or (pcValue) its value as
	 * text */
	bool run_query( xmlNodePtr psContext, const char* pzQuery, std::vector<xmlNodePtr>* pcNodes, std::string* pcValue, std::string* pcError ) const
	{
		Capture cErrors;
		xmlXPathContextPtr psXPath = xmlXPathNewContext( Doc() );
		if( psXPath == NULL )
		{
			*pcError = "out of memory";
			return false;
		}
		xmlXPathSetContextNode( psContext ? psContext : (xmlNodePtr)Doc(), psXPath );
		if( is_element( psContext ) )
		{
			xmlNsPtr* apsNs = xmlGetNsList( Doc(), psContext );
			for( int i = 0; apsNs != NULL && apsNs[i] != NULL; i++ )
				if( apsNs[i]->prefix != NULL )
					xmlXPathRegisterNs( psXPath, apsNs[i]->prefix, apsNs[i]->href );
			xmlFree( apsNs );
		}
		for( size_t i = 0; i < m_pcDocument->m_cNamespaces.size(); i++ )
			xmlXPathRegisterNs( psXPath, X( m_pcDocument->m_cNamespaces[i].first.c_str() ), X( m_pcDocument->m_cNamespaces[i].second.c_str() ) );
		xmlXPathObjectPtr psResult = xmlXPathEval( X( pzQuery ), psXPath );
		xmlXPathFreeContext( psXPath );
		if( psResult == NULL )
		{
			*pcError = "XPath: " + cErrors.First( "invalid expression" );
			return false;
		}
		if( pcValue != NULL )
		{
			xmlChar* pz = xmlXPathCastToString( psResult );
			pcValue->assign( pz ? C( pz ) : "" );
			xmlFree( pz );
		}
		if( pcNodes != NULL && psResult->type == XPATH_NODESET && psResult->nodesetval != NULL )
		{
			for( int i = 0; i < psResult->nodesetval->nodeNr; i++ )
			{
				xmlNodePtr p = psResult->nodesetval->nodeTab[i];
				if( p != NULL && p->type != XML_NAMESPACE_DECL )
					pcNodes->push_back( p );
			}
		}
		xmlXPathFreeObject( psResult );
		return true;
	}

	/* The node nIndex of a name: element nIndex of the path, its attribute,
	 * or match nIndex of a query */
	xmlNodePtr lookup( const char* pzName, int nIndex ) const
	{
		xmlNodePtr psNode = Node();
		if( psNode == NULL || nIndex < 0 )
			return NULL;
		if( is_query( pzName ) )
		{
			std::vector<xmlNodePtr> cMatches;
			std::string cError;
			if( !run_query( psNode, pzName, &cMatches, NULL, &cError ) || nIndex >= (int)cMatches.size() )
				return NULL;
			return cMatches[nIndex];
		}
		Path cPath;
		if( !is_element( psNode ) || !parse_path( pzName, &cPath ) )
			return NULL;
		if( cPath.m_cElements.empty() )
		{
			if( nIndex != 0 )
				return NULL;
			return cPath.m_bAttribute ? find_attribute( psNode, cPath.m_cAttribute ) : psNode;
		}
		for( size_t i = 0; psNode != NULL && i + 1 < cPath.m_cElements.size(); i++ )
			psNode = find_child( psNode, cPath.m_cElements[i], 0 );
		psNode = find_child( psNode, cPath.m_cElements.back(), nIndex );
		return cPath.m_bAttribute ? find_attribute( psNode, cPath.m_cAttribute ) : psNode;
	}

	/* The element a Set of a name goes to, made when it isn't there (and the
	 * ones before it on the path, and up to nIndex), or a new one (bAdd);
	 * *pcAttribute is the attribute, if the name has one.  NULL: recorded. */
	xmlNodePtr target( const char* pzName, int nIndex, bool bAdd, std::string* pcAttribute )
	{
		xmlNodePtr psNode = Element();
		Path cPath;
		if( psNode == NULL )
		{
			Fail( "set on an XML that refers to nothing:", pzName );
			return NULL;
		}
		if( is_query( pzName ) )
		{
			Fail( "an XPath query can't be set (use a name or a dotted path):", pzName );
			return NULL;
		}
		if( !parse_path( pzName, &cPath ) )
		{
			Fail( "not a name or path (\"a.b@c\"):", pzName );
			return NULL;
		}
		for( size_t i = 0; i < cPath.m_cElements.size(); i++ )
		{
			if( !valid_name( cPath.m_cElements[i] ) )
			{
				Fail( "not an XML name:", cPath.m_cElements[i].c_str() );
				return NULL;
			}
		}
		if( cPath.m_bAttribute && !valid_name( cPath.m_cAttribute ) )
		{
			Fail( "not an XML name:", cPath.m_cAttribute.c_str() );
			return NULL;
		}
		if( nIndex < 0 || ( bAdd && cPath.m_bAttribute ) || ( cPath.m_cElements.empty() && ( bAdd || nIndex != 0 ) ) )
		{
			Fail( cPath.m_bAttribute ? "an element has one attribute of a name:" : "no such index:", pzName );
			return NULL;
		}
		*pcAttribute = cPath.m_cAttribute;
		for( size_t i = 0; i < cPath.m_cElements.size(); i++ )
		{
			const std::string& cPart = cPath.m_cElements[i];
			bool bLast = i + 1 == cPath.m_cElements.size();
			int nWant = bLast ? nIndex : 0;
			xmlNodePtr psChild = bLast && bAdd ? NULL : find_child( psNode, cPart, nWant );
			if( psChild == NULL )
			{
				int nCount = bAdd && bLast ? nWant : count_children( psNode, cPart );
				for( ; nCount <= nWant; nCount++ )
				{
					psChild = new_child( Doc(), psNode, cPart );
					if( psChild == NULL )
					{
						Fail( "no namespace is declared for the prefix of", cPart.c_str() );
						return NULL;
					}
				}
			}
			psNode = psChild;
		}
		return psNode;
	}

	/* Stores a value at a name, or records why it can't */
	void Store( const char* pzName, int nIndex, const Value& cValue, bool bAdd = false )
	{
		std::string cAttribute;
		xmlNodePtr psElement = target( pzName, nIndex, bAdd, &cAttribute );
		if( psElement == NULL )
			return;
		if( cAttribute.empty() )
			set_content( Doc(), psElement, cValue );
		else if( !set_attribute( Doc(), psElement, cAttribute, cValue.m_cText ) )
			Fail( "no namespace is declared for the prefix of", cAttribute.c_str() );
	}

	/* Numbers from an element's attributes (or elements) of the names, or
	 * from a text ("10 20 410 320") */
	static bool get_numbers( const xmlNode* psNode, const char* const* apzNames, int nCount, double* av )
	{
		if( psNode == NULL )
			return false;
		std::string cText;
		if( is_element( psNode ) )
		{
			bool bAttributes = true, bElements = true;
			for( int i = 0; i < nCount && ( bAttributes || bElements ); i++ )
			{
				xmlNodePtr psAttr = find_attribute( (xmlNodePtr)psNode, apzNames[i] );
				if( !( psAttr && node_text( psAttr, &cText ) && to_double( cText, &av[i] ) ) )
					bAttributes = false;
			}
			if( bAttributes )
				return true;
			for( int i = 0; i < nCount && bElements; i++ )
			{
				xmlNodePtr psChild = find_child( (xmlNodePtr)psNode, apzNames[i], 0 );
				if( !( psChild && node_text( psChild, &cText ) && to_double( cText, &av[i] ) ) )
					bElements = false;
			}
			if( bElements )
				return true;
		}
		return node_text( psNode, &cText ) && parse_numbers( cText, nCount, av );
	}

	static XML::Type type_of( const xmlNode* psNode )
	{
		if( psNode == NULL )
			return XML::XML_NONE;
		switch( psNode->type )
		{
			case XML_ELEMENT_NODE:		return XML::XML_ELEMENT;
			case XML_ATTRIBUTE_NODE:	return XML::XML_ATTRIBUTE;
			case XML_TEXT_NODE:			return XML::XML_TEXT;
			case XML_CDATA_SECTION_NODE:	return XML::XML_CDATA;
			case XML_COMMENT_NODE:		return XML::XML_COMMENT;
			case XML_PI_NODE:			return XML::XML_PI;
			default:					return XML::XML_NONE;
		}
	}

	// -- Replacing and validating --

	/* The parsed document's root element in place of psElement (which stays
	 * the same node, so handles to it stay good) */
	void replace_element( xmlNodePtr psElement, xmlDocPtr psParsed )
	{
		xmlNodePtr psSource = xmlDocGetRootElement( psParsed );
		free_children( psElement );
		while( psElement->properties != NULL )
			xmlRemoveProp( psElement->properties );
		for( xmlNsPtr psNs = psSource->nsDef; psNs != NULL; psNs = psNs->next )
		{
			xmlNsPtr psHere = xmlSearchNs( Doc(), psElement, psNs->prefix );
			if( psHere == NULL || !xmlStrEqual( psHere->href, psNs->href ) )
				xmlNewNs( psElement, psNs->href, psNs->prefix );
		}
		xmlNodeSetName( psElement, psSource->name );
		xmlSetNs( psElement, psSource->ns ? xmlSearchNsByHref( Doc(), psElement, psSource->ns->href ) : NULL );
		for( xmlAttrPtr psAttr = psSource->properties; psAttr != NULL; psAttr = psAttr->next )
		{
			xmlChar* pzValue = xmlNodeListGetString( psParsed, psAttr->children, 1 );
			xmlNsPtr psNs = psAttr->ns ? xmlSearchNsByHref( Doc(), psElement, psAttr->ns->href ) : NULL;
			xmlSetNsProp( psElement, psNs, psAttr->name, pzValue ? pzValue : X( "" ) );
			xmlFree( pzValue );
		}
		while( psSource->children != NULL )
		{
			xmlNodePtr psChild = psSource->children;
			xmlUnlinkNode( psChild );
			if( xmlDOMWrapAdoptNode( NULL, psParsed, psChild, Doc(), psElement, 0 ) != 0 )
			{
				xmlFreeNode( psChild );
				continue;
			}
			xmlAddChild( psElement, psChild );
		}
	}

	/* Replaces this element (the document, at the root) with a text */
	status_t from_text( const char* pzText, size_t nLength )
	{
		xmlNodePtr psElement = Element();
		if( psElement == NULL )
		{
			Fail( "FromString on an XML that refers to nothing" );
			errno = EINVAL;
			return -1;
		}
		std::string cError;
		xmlDocPtr psParsed = parse_document( pzText, nLength, &cError );
		if( psParsed == NULL )
		{
			Fail( cError );
			errno = EINVAL;
			return -1;
		}
		if( psElement == xmlDocGetRootElement( Doc() ) )
		{
			xmlFreeDoc( m_pcDocument->m_psDoc );
			m_pcDocument->m_psDoc = psParsed;
			m_bRoot = true;
			m_psNode = NULL;
			return 0;
		}
		replace_element( psElement, psParsed );
		xmlFreeDoc( psParsed );
		return 0;
	}

	/* A schema (XSD, Relax-NG or DTD) from its text, in place of the one
	 * there was */
	status_t load_schema( const std::string& cText )
	{
		Capture cErrors;
		std::string cError;
		xmlDocPtr psSchemaDoc = xmlReadMemory( cText.c_str(), (int)cText.size(), NULL, NULL, PARSE_OPTIONS );
		xmlNodePtr psRoot = psSchemaDoc ? xmlDocGetRootElement( psSchemaDoc ) : NULL;
		xmlSchemaPtr psXSD = NULL;
		xmlRelaxNGPtr psRelaxNG = NULL;
		xmlDtdPtr psDTD = NULL;
		if( psRoot != NULL )
		{
			const char* pzNs = psRoot->ns ? C( psRoot->ns->href ) : "";
			cErrors.m_cErrors.clear();
			if( strcmp( pzNs, "http://www.w3.org/2001/XMLSchema" ) == 0 && xmlStrEqual( psRoot->name, X( "schema" ) ) )
			{
				xmlSchemaParserCtxtPtr psParser = xmlSchemaNewDocParserCtxt( psSchemaDoc );
				if( psParser != NULL )
				{
					xmlSchemaSetParserStructuredErrors( psParser, structured_error, NULL );
					psXSD = xmlSchemaParse( psParser );
					xmlSchemaFreeParserCtxt( psParser );
				}
				if( psXSD == NULL )
					cError = "the XML Schema: " + cErrors.First( "not valid" );
			}
			else if( strcmp( pzNs, "http://relaxng.org/ns/structure/1.0" ) == 0 )
			{
				xmlRelaxNGParserCtxtPtr psParser = xmlRelaxNGNewDocParserCtxt( psSchemaDoc );
				if( psParser != NULL )
				{
					xmlRelaxNGSetParserStructuredErrors( psParser, structured_error, NULL );
					psRelaxNG = xmlRelaxNGParse( psParser );
					xmlRelaxNGFreeParserCtxt( psParser );
				}
				if( psRelaxNG == NULL )
					cError = "the Relax-NG schema: " + cErrors.First( "not valid" );
			}
			else
				cError = "the schema is XML, but not an XML Schema (xs:schema) or a Relax-NG schema";
		}
		else
		{
			// Not XML: a DTD?
			std::string cXMLError = cErrors.First( "" );
			cErrors.m_cErrors.clear();
			xmlParserInputBufferPtr psInput = xmlParserInputBufferCreateMem( cText.c_str(), (int)cText.size(), XML_CHAR_ENCODING_NONE );
			if( psInput != NULL )
				psDTD = xmlIOParseDTD( NULL, psInput, XML_CHAR_ENCODING_NONE );	// (takes psInput)
			if( psDTD == NULL || !cErrors.m_cErrors.empty() )
			{
				if( psDTD != NULL )
					xmlFreeDtd( psDTD );
				psDTD = NULL;
				bool bDTD = cText.find( "<!ELEMENT" ) != std::string::npos || cText.find( "<!ATTLIST" ) != std::string::npos ||
				            cText.find( "<!ENTITY" ) != std::string::npos;
				if( bDTD && !cErrors.m_cErrors.empty() )
					cError = "the DTD: " + cErrors.m_cErrors[0];
				else
					cError = "the schema is not well-formed XML (or a DTD): " + ( cXMLError.empty() ? std::string( "empty" ) : cXMLError );
			}
		}
		if( !cError.empty() )
		{
			if( psSchemaDoc != NULL )
				xmlFreeDoc( psSchemaDoc );
			Fail( cError );
			errno = EINVAL;
			return -1;
		}
		Document* d = m_pcDocument;
		d->FreeSchema();
		d->m_cSchemaText = cText;
		d->m_psXSD = psXSD;
		d->m_psRelaxNG = psRelaxNG;
		d->m_psDTD = psDTD;
		if( psDTD != NULL )
		{
			d->m_nSchema = SCHEMA_DTD;
			if( psSchemaDoc != NULL )
				xmlFreeDoc( psSchemaDoc );
		}
		else
		{
			d->m_nSchema = psXSD ? SCHEMA_XSD : SCHEMA_RELAXNG;
			d->m_psSchemaDoc = psSchemaDoc;		// the schema may point into it
		}
		return 0;
	}

	bool validate() const
	{
		Document* d = m_pcDocument;
		d->m_cValidationErrors.clear();
		if( d->m_nSchema == SCHEMA_NONE && d->m_psDoc->intSubset == NULL )
			return true;
		Capture cErrors;
		bool bValid = false;
		switch( d->m_nSchema )
		{
			case SCHEMA_XSD:
			{
				xmlSchemaValidCtxtPtr psValid = xmlSchemaNewValidCtxt( d->m_psXSD );
				if( psValid != NULL )
				{
					xmlSchemaSetValidStructuredErrors( psValid, structured_error, NULL );
					bValid = xmlSchemaValidateDoc( psValid, d->m_psDoc ) == 0;
					xmlSchemaFreeValidCtxt( psValid );
				}
				break;
			}
			case SCHEMA_RELAXNG:
			{
				xmlRelaxNGValidCtxtPtr psValid = xmlRelaxNGNewValidCtxt( d->m_psRelaxNG );
				if( psValid != NULL )
				{
					xmlRelaxNGSetValidStructuredErrors( psValid, structured_error, NULL );
					bValid = xmlRelaxNGValidateDoc( psValid, d->m_psDoc ) == 0;
					xmlRelaxNGFreeValidCtxt( psValid );
				}
				break;
			}
			default:	// a DTD: the schema's, or the document's own
			{
				xmlValidCtxtPtr psValid = xmlNewValidCtxt();
				if( psValid != NULL )
				{
					psValid->error = generic_error;		// the errors come through structured_error
					psValid->warning = generic_error;
					bValid = ( d->m_psDTD ? xmlValidateDtd( psValid, d->m_psDoc, d->m_psDTD ) : xmlValidateDocument( psValid, d->m_psDoc ) ) == 1;
					xmlFreeValidCtxt( psValid );
				}
				break;
			}
		}
		d->m_cValidationErrors = cErrors.m_cErrors;
		if( !bValid && d->m_cValidationErrors.empty() )
			d->m_cValidationErrors.push_back( "the document doesn't match the schema" );
		return bValid;
	}

	// -- Message conversion --

	/* Value nIndex of a message's name into a new element; false for a type
	 * XML has no form for (pointers, raw data, variants...) */
	static bool message_value( xmlDocPtr psDoc, xmlNodePtr psElement, const Message& cMessage, const char* pzName, int nType, int nIndex )
	{
		Value cValue( "" );
		switch( nType )
		{
			case T_STRING:
			{
				const char* pz = NULL;
				if( cMessage.FindString( pzName, &pz, nIndex ) != 0 || pz == NULL )
					return false;
				cValue = Value( pz );
				break;
			}
			case T_INT8:	{ int8 n;  if( cMessage.FindInt8( pzName, &n, nIndex ) != 0 ) return false;  cValue = Value( int_text( n ) ); break; }
			case T_INT16:	{ int16 n; if( cMessage.FindInt16( pzName, &n, nIndex ) != 0 ) return false; cValue = Value( int_text( n ) ); break; }
			case T_INT32:	{ int32 n; if( cMessage.FindInt32( pzName, &n, nIndex ) != 0 ) return false; cValue = Value( int_text( n ) ); break; }
			case T_INT64:	{ int64 n; if( cMessage.FindInt64( pzName, &n, nIndex ) != 0 ) return false; cValue = Value( int_text( n ) ); break; }
			case T_BOOL:	{ bool b;  if( cMessage.FindBool( pzName, &b, nIndex ) != 0 ) return false;   cValue = Value( b ? "true" : "false" ); break; }
			case T_FLOAT:	{ float v; if( cMessage.FindFloat( pzName, &v, nIndex ) != 0 ) return false;  cValue = Value( number_text( float_to_double( v ) ) ); break; }
			case T_DOUBLE:	{ double v; if( cMessage.FindDouble( pzName, &v, nIndex ) != 0 ) return false; cValue = Value( number_text( v ) ); break; }
			case T_RECT:
			{
				Rect r( 0, 0, 0, 0 );
				if( cMessage.FindRect( pzName, &r, nIndex ) != 0 )
					return false;
				double av[4] = { float_to_double( r.left ), float_to_double( r.top ), float_to_double( r.right ), float_to_double( r.bottom ) };
				cValue = Value( g_apzRect, 4, av );
				break;
			}
			case T_IRECT:
			{
				IRect r( 0, 0, 0, 0 );
				if( cMessage.FindIRect( pzName, &r, nIndex ) != 0 )
					return false;
				double av[4] = { (double)r.left, (double)r.top, (double)r.right, (double)r.bottom };
				cValue = Value( g_apzRect, 4, av );
				break;
			}
			case T_POINT:
			{
				Point p;
				if( cMessage.FindPoint( pzName, &p, nIndex ) != 0 )
					return false;
				double av[2] = { float_to_double( p.x ), float_to_double( p.y ) };
				cValue = Value( g_apzPoint, 2, av );
				break;
			}
			case T_IPOINT:
			{
				IPoint p;
				if( cMessage.FindIPoint( pzName, &p, nIndex ) != 0 )
					return false;
				double av[2] = { (double)p.x, (double)p.y };
				cValue = Value( g_apzPoint, 2, av );
				break;
			}
			case T_COLOR32:
			{
				Color32_s s( 0, 0, 0 );
				if( cMessage.FindColor32( pzName, &s, nIndex ) != 0 )
					return false;
				cValue = Value( color_text( s ) );
				break;
			}
			case T_MESSAGE:
			{
				Message cChild;
				if( cMessage.FindMessage( pzName, &cChild, nIndex ) != 0 )
					return false;
				message_to_xml( psDoc, psElement, cChild, NULL );
				return true;
			}
			case T_JSON:	// its text
			{
				const void* p = NULL;
				size_t nSize = 0;
				if( cMessage.FindData( pzName, T_JSON, &p, &nSize, nIndex ) != 0 || p == NULL || nSize == 0 )
					return false;
				cValue = Value( std::string( (const char*)p, strnlen( (const char*)p, nSize ) ) );
				break;
			}
			case T_XML:		// what is in its element, in this one
			{
				XML cXML;
				if( cMessage.FindObject( pzName, cXML, nIndex ) != 0 || cXML.m->Element() == NULL )
					return false;
				xmlNodePtr psSource = cXML.m->Element();
				for( xmlAttrPtr psAttr = psSource->properties; psAttr != NULL; psAttr = psAttr->next )
				{
					xmlChar* pzValue = xmlNodeListGetString( psSource->doc, psAttr->children, 1 );
					xmlSetProp( psElement, psAttr->name, pzValue ? pzValue : X( "" ) );
					xmlFree( pzValue );
				}
				for( xmlNodePtr psChild = psSource->children; psChild != NULL; psChild = psChild->next )
				{
					xmlNodePtr psCopy = xmlDocCopyNode( psChild, psDoc, 1 );
					if( psCopy != NULL )
						xmlAddChild( psElement, psCopy );
				}
				return true;
			}
			default:
				return false;
		}
		set_content( psDoc, psElement, cValue );
		return true;
	}

	/* A message's values as elements in psElement; names that aren't XML
	 * names go into *pcSkipped */
	static void message_to_xml( xmlDocPtr psDoc, xmlNodePtr psElement, const Message& cMessage, std::string* pcSkipped )
	{
		int nNames = cMessage.CountNames();
		for( int i = 0; i < nNames; i++ )
		{
			String cName = cMessage.GetName( i );
			int nType = 0, nCount = 0;
			if( cMessage.GetNameInfo( cName.c_str(), &nType, &nCount ) != 0 || nCount < 1 )
				continue;
			if( xmlValidateNCName( X( cName.c_str() ), 0 ) != 0 )
			{
				if( pcSkipped != NULL && pcSkipped->empty() )
					*pcSkipped = cName.c_str();
				continue;
			}
			for( int j = 0; j < nCount; j++ )
			{
				xmlNodePtr psChild = new_child( psDoc, psElement, cName.c_str() );
				if( psChild != NULL && !message_value( psDoc, psChild, cMessage, cName.c_str(), nType, j ) )
				{
					xmlUnlinkNode( psChild );
					xmlFreeNode( psChild );
				}
			}
		}
	}

	/* A text as the type it looks like: integers int32 (int64 when larger),
	 * other numbers doubles, true and false bools, the rest strings */
	static void add_text( Message* pcMessage, const char* pzName, const std::string& cText )
	{
		std::string c = trim( cText );
		char* pzEnd;
		if( !c.empty() && c.find_first_not_of( "+-0123456789" ) == std::string::npos )
		{
			errno = 0;
			long long n = strtoll( c.c_str(), &pzEnd, 10 );
			if( *pzEnd == '\0' && errno == 0 )
			{
				if( n >= -2147483647LL - 1 && n <= 2147483647LL )
					pcMessage->AddInt32( pzName, (int32)n );
				else
					pcMessage->AddInt64( pzName, n );
				return;
			}
		}
		double v;
		if( !c.empty() && c.find_first_of( "0123456789" ) != std::string::npos && to_double( c, &v ) )
			pcMessage->AddDouble( pzName, v );
		else if( c == "true" || c == "false" )
			pcMessage->AddBool( pzName, c == "true" );
		else
			pcMessage->AddString( pzName, cText.c_str() );
	}

	static bool has_text( const xmlNode* psElement )
	{
		std::string cText;
		return node_text( psElement, &cText ) && !trim( cText ).empty();
	}

	static int count_attributes( const xmlNode* psElement )
	{
		int n = 0;
		for( xmlAttrPtr p = psElement->properties; p != NULL; p = p->next )
			n++;
		return n;
	}

	static bool has_element_children( const xmlNode* psElement )
	{
		for( xmlNodePtr p = psElement->children; p != NULL; p = p->next )
			if( p->type == XML_ELEMENT_NODE )
				return true;
		return false;
	}

	/* An element's attributes and elements as a message's values */
	static void xml_to_message( const xmlNode* psElement, Message* pcMessage )
	{
		std::string cText;
		for( xmlAttrPtr psAttr = psElement->properties; psAttr != NULL; psAttr = psAttr->next )
			if( node_text( (xmlNodePtr)psAttr, &cText ) )
				add_text( pcMessage, qualified_name( (xmlNodePtr)psAttr ).c_str(), cText );
		for( xmlNodePtr psChild = psElement->children; psChild != NULL; psChild = psChild->next )
		{
			if( psChild->type != XML_ELEMENT_NODE )
				continue;
			std::string cName = qualified_name( psChild );
			double av[4];
			int nAttributes = count_attributes( psChild );
			if( has_element_children( psChild ) || ( nAttributes > 0 && !has_text( psChild ) &&
			    !( nAttributes == 4 && get_numbers( psChild, g_apzRect, 4, av ) ) &&
			    !( nAttributes == 2 && get_numbers( psChild, g_apzPoint, 2, av ) ) ) )
			{
				Message cChild;
				xml_to_message( psChild, &cChild );
				pcMessage->AddMessage( cName.c_str(), &cChild );
			}
			else if( nAttributes == 4 && get_numbers( psChild, g_apzRect, 4, av ) )
				pcMessage->AddRect( cName.c_str(), Rect( av[0], av[1], av[2], av[3] ) );
			else if( nAttributes == 2 && get_numbers( psChild, g_apzPoint, 2, av ) )
				pcMessage->AddPoint( cName.c_str(), Point( av[0], av[1] ) );
			else if( node_text( psChild, &cText ) )
				add_text( pcMessage, cName.c_str(), cText );
		}
	}

	// -- Files --

	static status_t read_file( const char* pzPath, std::string* pcText )
	{
		FILE* hFile = fopen( pzPath, "rb" );
		if( hFile == NULL )
			return -1;
		char zBuffer[4096];
		size_t nRead;
		while( ( nRead = fread( zBuffer, 1, sizeof( zBuffer ), hFile ) ) > 0 )
			pcText->append( zBuffer, nRead );
		bool bError = ferror( hFile );
		fclose( hFile );
		if( bError )
		{
			errno = EIO;
			return -1;
		}
		return 0;
	}

	/* This element as a whole document: the document at the root, else a
	 * copy of the element */
	std::string document_text( bool bFormatted ) const
	{
		xmlNodePtr psElement = Element();
		if( psElement == NULL )
			return std::string();
		if( psElement == xmlDocGetRootElement( Doc() ) )
			return serialize( Doc(), NULL, bFormatted );
		xmlDocPtr psCopy = copy_as_document( psElement );
		if( psCopy == NULL )
			return std::string();
		std::string cText = serialize( psCopy, NULL, bFormatted );
		xmlFreeDoc( psCopy );
		return cText;
	}

	/* This node as text: the whole document at the root, else the element
	 * (with the namespaces it uses), or another node's value */
	std::string node_string( bool bFormatted ) const
	{
		xmlNodePtr psNode = Node();
		if( psNode == NULL )
			return std::string();
		if( !is_element( psNode ) )
		{
			std::string cText;
			node_text( psNode, &cText );
			return cText;
		}
		if( psNode == xmlDocGetRootElement( Doc() ) )
			return serialize( Doc(), NULL, bFormatted );
		xmlDocPtr psCopy = copy_as_document( psNode );
		if( psCopy == NULL )
			return std::string();
		std::string cText = serialize( psCopy, xmlDocGetRootElement( psCopy ), bFormatted );
		xmlFreeDoc( psCopy );
		return cText;
	}
};

const char* const XML::Private::g_apzRect[] = { "left", "top", "right", "bottom" };
const char* const XML::Private::g_apzPoint[] = { "x", "y" };
const char* const XML::Private::g_apzColor[] = { "red", "green", "blue", "alpha" };

// ---- XML ----------------------------------------------------------------------

/** A new document, with an empty root element <root> */
XML::XML()
{
	Guard cGuard;
	m = new Private( "root" );
}

/** A new document, with an empty root element of the name ("settings":
 * <settings/>) */
XML::XML( const char* pzRootName )
{
	Guard cGuard;
	m = new Private( pzRootName );
}

XML::XML( Private* pcPrivate ) : m( pcPrivate )
{
}

/** The same element (not a copy of it: see Clone()) */
XML::XML( const XML& cOther ) : Flattenable()
{
	Guard cGuard;
	m = new Private( cOther.m->m_pcDocument, cOther.m->m_psNode, cOther.m->m_bRoot );
}

XML::~XML()
{
	Guard cGuard;
	delete m;
}

XML& XML::operator=( const XML& cOther )
{
	Guard cGuard;
	m->Refer( cOther.m->m_pcDocument, cOther.m->m_psNode, cOther.m->m_bRoot );
	return *this;
}

/** A new document, with an empty root element <root>, checked against a
 * schema file by IsValid() (see SetSchema()) */
XML::XML( const File& cSchema )
{
	Guard cGuard;
	m = new Private( "root" );
	SetSchema( cSchema );
}

/** The schema IsValid() checks the document against, from a file: a W3C XML
 * Schema (xs:schema), a Relax-NG schema (XML syntax) or a DTD.  A file that
 * can't be read or isn't a schema leaves the schema as it was, and is
 * recorded (GetLastError()). */
status_t XML::SetSchema( const File& cSchema )
{
	Guard cGuard;
	File cFile( cSchema );
	off_t nSize = cFile.GetSize();
	if( nSize < 0 || nSize > 0x800000 )
	{
		m->Fail( "the schema file can't be read (or is larger than 8 MB)" );
		return -1;
	}
	std::string cText( (size_t)nSize, '\0' );
	if( nSize > 0 && cFile.ReadPos( 0, &cText[0], nSize ) != nSize )
	{
		m->Fail( "the schema file can't be read" );
		return -1;
	}
	return m->load_schema( cText );
}

/** The schema IsValid() checks the document against (a copy of cSchema's
 * document: an XML Schema or a Relax-NG schema) */
status_t XML::SetSchema( const XML& cSchema )
{
	Guard cGuard;
	if( cSchema.m->Element() == NULL )
	{
		m->Fail( "the schema is an XML that refers to nothing" );
		return -1;
	}
	return m->load_schema( cSchema.m->document_text( false ) );
}

/** The schema IsValid() checks the document against, from its text */
status_t XML::SetSchemaString( const String& cSchema )
{
	Guard cGuard;
	return m->load_schema( std::string( cSchema.c_str(), cSchema.size() ) );
}

void XML::ClearSchema()
{
	Guard cGuard;
	m->m_pcDocument->FreeSchema();
}

bool XML::HasSchema() const
{
	Guard cGuard;
	return m->m_pcDocument->m_nSchema != Private::SCHEMA_NONE;
}

/** Whether the document matches its schema, or without one, its own DTD
 * (true when it has neither).  The whole document is checked, whichever of
 * its elements this XML refers to. */
bool XML::IsValid() const
{
	Guard cGuard;
	return m->validate();
}

/** Why the last IsValid() was false, one line per problem, with where:
 * "/settings/window/width: Element 'width': ..." */
std::vector<String> XML::GetValidationErrors() const
{
	Guard cGuard;
	std::vector<String> cErrors;
	for( size_t i = 0; i < m->m_pcDocument->m_cValidationErrors.size(); i++ )
		cErrors.push_back( String( m->m_pcDocument->m_cValidationErrors[i].c_str() ) );
	return cErrors;
}

/** A copy of this element, as a new document (with the schema and the
 * namespaces of RegisterNamespace()) */
XML XML::Clone() const
{
	Guard cGuard;
	XML cCopy;
	xmlNodePtr psElement = m->Element();
	xmlDocPtr psDoc = NULL;
	if( psElement != NULL )
		psDoc = psElement == xmlDocGetRootElement( m->Doc() ) ? xmlCopyDoc( m->Doc(), 1 ) : copy_as_document( psElement );
	if( psDoc != NULL )
	{
		xmlFreeDoc( cCopy.m->m_pcDocument->m_psDoc );
		cCopy.m->m_pcDocument->m_psDoc = psDoc;
	}
	if( m->m_pcDocument->m_nSchema != Private::SCHEMA_NONE )
		cCopy.m->load_schema( m->m_pcDocument->m_cSchemaText );
	cCopy.m->m_pcDocument->m_cNamespaces = m->m_pcDocument->m_cNamespaces;
	return cCopy;
}

/** false for an XML that refers to nothing (GetElement() of a name that
 * isn't there, End() of the root element) or to another node than an
 * element (from Query()) */
bool XML::IsElement() const
{
	Guard cGuard;
	return m->Element() != NULL;
}

/** Replaces this element with an XML text (an element and what is in it).
 * At the root, the text replaces the whole document (its DOCTYPE and all).
 * When it isn't well-formed (GetLastError()), the element stays as it was. */
status_t XML::FromString( const String& cText )
{
	Guard cGuard;
	return m->from_text( cText.c_str(), cText.size() );
}

/** This element as XML text, indented or on one line.  At the root, the
 * whole document, with its XML declaration ("<?xml version="1.0"
 * encoding="UTF-8"?>"); a node from Query() that isn't an element gives its
 * value. */
String XML::ToString( bool bFormatted ) const
{
	Guard cGuard;
	return String( m->node_string( bFormatted ) );
}

/** Replaces this element (at the root: the document) with an XML file's.
 * When it isn't there or isn't well-formed (GetLastError()), it stays as it
 * was. */
status_t XML::Load( const String& cPath )
{
	Guard cGuard;
	std::string cText;
	if( Private::read_file( cPath.c_str(), &cText ) < 0 )
	{
		int nError = errno;
		m->Fail( std::string( cPath.c_str() ) + ": " + strerror( nError ) );
		errno = nError;
		return -1;
	}
	if( m->from_text( cText.data(), cText.size() ) < 0 )
	{
		m->m_pcDocument->m_cError = std::string( cPath.c_str() ) + ": " + m->m_pcDocument->m_cError;
		return -1;
	}
	return 0;
}

/** Writes this element to a file as a document (making its folders),
 * through a temporary file renamed over it: a crash leaves the old file or
 * the new one */
status_t XML::Save( const String& cPath ) const
{
	Guard cGuard;
	if( m->Element() == NULL )
	{
		errno = EINVAL;
		return -1;
	}
	std::string cText = m->document_text( true );
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
		return -1;
	bool bOk = fwrite( cText.data(), 1, cText.size(), hFile ) == cText.size() &&
	           fflush( hFile ) == 0 && fsync( fileno( hFile ) ) == 0;
	int nError = errno;
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

/** A message as a new document: each value of a name becomes an element of
 * the name in the root element; a message in it an element with elements in
 * it, rects and points attributes, a JSON its text, an XML what is in it.
 * Pointers, raw data and variants are left out, and so are names that
 * aren't XML names (recorded: GetLastError()) and message codes. */
XML XML::FromMessage( const Message& cMessage, const char* pzRootName )
{
	Guard cGuard;
	XML cDoc( pzRootName );
	std::string cSkipped;
	Private::message_to_xml( cDoc.m->Doc(), cDoc.m->Element(), cMessage, &cSkipped );
	if( !cSkipped.empty() )
		cDoc.m->Fail( "a message name that isn't an XML name is left out:", cSkipped.c_str() );
	return cDoc;
}

/** Adds this element's attributes and elements to a message (see
 * FromMessage()): elements with elements in them (or only attributes)
 * become messages, rect- and point-shaped ones Rect and Point, the others
 * (and attributes) their text as the type it looks like: integers int32
 * (int64 when larger), other numbers doubles, "true" and "false" bools, the
 * rest strings.  Several elements of a name are several values of it. */
status_t XML::ToMessage( Message* pcMessage ) const
{
	Guard cGuard;
	xmlNodePtr psElement = m->Element();
	if( pcMessage == NULL || psElement == NULL )
	{
		errno = EINVAL;
		return -1;
	}
	Private::xml_to_message( psElement, pcMessage );
	return 0;
}

/** os::Flattenable: T_XML */
int XML::GetType() const
{
	return T_XML;
}

/** The size of this element as XML text on one line, with its NUL */
size_t XML::GetFlattenedSize() const
{
	Guard cGuard;
	std::string cText = m->document_text( false );
	return cText.empty() ? sizeof( "<root/>" ) : cText.size() + 1;
}

/** This element as XML text (a document) on one line, NUL-terminated */
status_t XML::Flatten( uint8* pBuffer, size_t nSize ) const
{
	Guard cGuard;
	std::string cText = m->document_text( false );
	if( cText.empty() )
		cText = "<root/>";
	if( cText.size() + 1 > nSize )
	{
		errno = ENOBUFS;
		return -1;
	}
	memcpy( pBuffer, cText.c_str(), cText.size() + 1 );
	return 0;
}

/** Replaces this element with flattened XML (as FromString()) */
status_t XML::Unflatten( const uint8* pBuffer )
{
	Guard cGuard;
	if( pBuffer == NULL )
	{
		errno = EINVAL;
		return -1;
	}
	return m->from_text( (const char*)pBuffer, strlen( (const char*)pBuffer ) );
}

/** The element nIndex of a name, or an XML that refers to nothing
 * (IsElement() false) when it isn't there or isn't an element */
XML XML::GetElement( const char* pzName, int nIndex ) const
{
	Guard cGuard;
	xmlNodePtr psNode = m->lookup( pzName, nIndex );
	return m->Make( Private::is_element( psNode ) ? psNode : NULL );
}

/** The element nIndex of a name, made (empty) when it isn't there, with the
 * ones before it */
XML XML::SetElement( const char* pzName, int nIndex )
{
	Guard cGuard;
	xmlNodePtr psNode = m->lookup( pzName, nIndex );
	if( Private::is_element( psNode ) && !Private::is_query( pzName ) )
		return m->Make( psNode );
	std::string cAttribute;
	psNode = m->target( pzName, nIndex, false, &cAttribute );
	if( psNode != NULL && !cAttribute.empty() )
	{
		m->Fail( "an attribute is not an element:", pzName );
		psNode = NULL;
	}
	return m->Make( psNode );
}

/** A new, empty element of a name, after the last one of the name */
XML XML::AddElement( const char* pzName )
{
	Guard cGuard;
	std::string cAttribute;
	xmlNodePtr psNode = m->target( pzName, 0, true, &cAttribute );
	return m->Make( psNode );
}

/** The element this one is in (for chains: SetElement( "a" ).Set...().End()),
 * or an XML that refers to nothing at the root */
XML XML::End() const
{
	Guard cGuard;
	xmlNodePtr psNode = m->Node();
	if( psNode == NULL || psNode == xmlDocGetRootElement( m->Doc() ) )
		return m->Make( NULL );
	xmlNodePtr psParent = psNode->parent;
	return m->Make( Private::is_element( psParent ) ? psParent : NULL );
}

/** The document's root element */
XML XML::Top() const
{
	Guard cGuard;
	return XML( new Private( m->m_pcDocument, NULL, true ) );
}

/** The number of elements in this one */
int XML::CountChildren() const
{
	Guard cGuard;
	int nCount = 0;
	xmlNodePtr psElement = m->Element();
	for( xmlNodePtr p = psElement ? psElement->children : NULL; p != NULL; p = p->next )
		if( p->type == XML_ELEMENT_NODE )
			nCount++;
	return nCount;
}

/** Element nIndex in this one, of any name, in the document's order */
XML XML::GetChild( int nIndex ) const
{
	Guard cGuard;
	xmlNodePtr psElement = m->Element();
	for( xmlNodePtr p = psElement && nIndex >= 0 ? psElement->children : NULL; p != NULL; p = p->next )
		if( p->type == XML_ELEMENT_NODE && nIndex-- == 0 )
			return m->Make( p );
	return m->Make( NULL );
}

/** The name of element nIndex in this one ("prefix:name" when it has a
 * prefix) */
String XML::GetChildName( int nIndex ) const
{
	Guard cGuard;
	xmlNodePtr psElement = m->Element();
	for( xmlNodePtr p = psElement && nIndex >= 0 ? psElement->children : NULL; p != NULL; p = p->next )
		if( p->type == XML_ELEMENT_NODE && nIndex-- == 0 )
			return String( Private::qualified_name( p ) );
	return String();
}

int XML::CountAttributes() const
{
	Guard cGuard;
	xmlNodePtr psElement = m->Element();
	return psElement ? Private::count_attributes( psElement ) : 0;
}

/** The name of attribute nIndex of this element (for GetString( "@" + name )) */
String XML::GetAttributeName( int nIndex ) const
{
	Guard cGuard;
	xmlNodePtr psElement = m->Element();
	for( xmlAttrPtr p = psElement && nIndex >= 0 ? psElement->properties : NULL; p != NULL; p = p->next )
		if( nIndex-- == 0 )
			return String( Private::qualified_name( (xmlNodePtr)p ) );
	return String();
}

/** Renames this element ("prefix:name": a prefix declared here or above) */
XML& XML::SetName( const char* pzName )
{
	Guard cGuard;
	xmlNodePtr psElement = m->Element();
	xmlNsPtr psNs = NULL;
	std::string cLocal;
	if( psElement == NULL )
		m->Fail( "SetName on an XML that refers to nothing:", pzName );
	else if( pzName == NULL || !Private::valid_name( pzName ) )
		m->Fail( "not an XML name:", pzName ? pzName : "" );
	else if( !Private::find_ns( m->Doc(), psElement, pzName, true, &psNs, &cLocal ) )
		m->Fail( "no namespace is declared for the prefix of", pzName );
	else
	{
		xmlNodeSetName( psElement, X( cLocal.c_str() ) );
		xmlSetNs( psElement, psNs );
	}
	return *this;
}

/** Replaces what is in this element with a text (or sets this attribute's
 * value) */
XML& XML::SetText( const String& cText )
{
	Guard cGuard;
	xmlNodePtr psNode = m->Node();
	std::string c( cText.c_str(), cText.size() );
	if( Private::is_element( psNode ) )
		Private::set_content( m->Doc(), psNode, Private::Value( c ) );
	else if( psNode != NULL && psNode->type == XML_ATTRIBUTE_NODE && Private::is_element( psNode->parent ) )
		xmlSetNsProp( psNode->parent, psNode->ns, psNode->name, X( c.c_str() ) );
	else
		m->Fail( "SetText on an XML that refers to no element or attribute" );
	return *this;
}

/** Adds a comment (<!-- text -->) at the end of this element */
XML& XML::AddComment( const String& cText )
{
	Guard cGuard;
	xmlNodePtr psElement = m->Element();
	std::string c( cText.c_str() );
	if( psElement == NULL )
		m->Fail( "AddComment on an XML that refers to nothing" );
	else if( c.find( "--" ) != std::string::npos || ( !c.empty() && c[c.size() - 1] == '-' ) )
		m->Fail( "a comment can't have \"--\" in it, or end in '-'" );
	else
		xmlAddChild( psElement, xmlNewDocComment( m->Doc(), X( c.c_str() ) ) );
	return *this;
}

/** Declares a namespace on this element (xmlns:prefix="uri"), so names with
 * the prefix can be set in it.  No prefix (NULL or ""): the default
 * namespace (xmlns="uri"), which this element is then in (if it has no
 * prefix), and the elements made in it. */
XML& XML::DeclareNamespace( const char* pzPrefix, const char* pzURI )
{
	Guard cGuard;
	xmlNodePtr psElement = m->Element();
	bool bDefault = pzPrefix == NULL || pzPrefix[0] == '\0';
	if( psElement == NULL )
		m->Fail( "DeclareNamespace on an XML that refers to nothing" );
	else if( pzURI == NULL || ( !bDefault && ( pzURI[0] == '\0' || xmlValidateNCName( X( pzPrefix ), 0 ) != 0 ) ) )
		m->Fail( "not a namespace prefix and URI:", pzPrefix ? pzPrefix : "" );
	else
	{
		const xmlChar* pzP = bDefault ? NULL : X( pzPrefix );
		xmlNsPtr psNs = NULL;
		for( xmlNsPtr p = psElement->nsDef; p != NULL; p = p->next )
			if( xmlStrEqual( p->prefix, pzP ) )
				psNs = p;
		if( psNs != NULL )
		{
			if( !xmlStrEqual( psNs->href, X( pzURI ) ) )
				m->Fail( "the prefix is declared here with another URI:", pzPrefix ? pzPrefix : "" );
		}
		else
			psNs = xmlNewNs( psElement, X( pzURI ), pzP );
		if( psNs != NULL && bDefault && ( psElement->ns == NULL || psElement->ns->prefix == NULL ) )
			xmlSetNs( psElement, psNs );
	}
	return *this;
}

/** The namespace URI of this element or attribute ("" in none) */
String XML::GetNamespaceURI() const
{
	Guard cGuard;
	xmlNodePtr psNode = m->Node();
	if( psNode != NULL && ( psNode->type == XML_ELEMENT_NODE || psNode->type == XML_ATTRIBUTE_NODE ) &&
	    psNode->ns != NULL && psNode->ns->href != NULL )
		return String( C( psNode->ns->href ) );
	return String();
}

/** Every node an XPath query selects, from this element (for "./" and
 * "../"), in document order */
std::vector<XML> XML::Query( const String& cQuery ) const
{
	Guard cGuard;
	std::vector<XML> cResult;
	std::vector<xmlNodePtr> cMatches;
	std::string cError;
	if( !m->run_query( m->Node(), cQuery.c_str(), &cMatches, NULL, &cError ) )
	{
		m->Fail( cError + " in \"" + cQuery.c_str() + "\"" );
		return cResult;
	}
	for( size_t i = 0; i < cMatches.size(); i++ )
		cResult.push_back( m->Make( cMatches[i] ) );
	return cResult;
}

/** The first node an XPath query selects, or an XML that refers to nothing
 * (Exists() false) */
XML XML::QueryFirst( const String& cQuery ) const
{
	Guard cGuard;
	std::vector<XML> cResult = Query( cQuery );
	return cResult.empty() ? m->Make( NULL ) : cResult[0];
}

/** The value of an XPath expression as text: "3" for "count(//book)", the
 * first node's text for a path */
String XML::Evaluate( const String& cExpression ) const
{
	Guard cGuard;
	std::string cValue, cError;
	if( !m->run_query( m->Node(), cExpression.c_str(), NULL, &cValue, &cError ) )
	{
		m->Fail( cError + " in \"" + cExpression.c_str() + "\"" );
		return String();
	}
	return String( cValue );
}

/** A namespace prefix for the queries of this document ("b", for
 * "//b:book" in a document whose default namespace is uri) */
status_t XML::RegisterNamespace( const char* pzPrefix, const char* pzURI )
{
	Guard cGuard;
	if( pzPrefix == NULL || pzURI == NULL || xmlValidateNCName( X( pzPrefix ), 0 ) != 0 )
	{
		m->Fail( "not a namespace prefix and URI:", pzPrefix ? pzPrefix : "" );
		errno = EINVAL;
		return -1;
	}
	std::vector<std::pair<std::string, std::string> >& cNamespaces = m->m_pcDocument->m_cNamespaces;
	for( size_t i = 0; i < cNamespaces.size(); i++ )
	{
		if( cNamespaces[i].first == pzPrefix )
		{
			cNamespaces[i].second = pzURI;
			return 0;
		}
	}
	cNamespaces.push_back( std::make_pair( std::string( pzPrefix ), std::string( pzURI ) ) );
	return 0;
}

/** Whether this XML refers to a node (of any type) */
bool XML::Exists() const
{
	Guard cGuard;
	return m->Node() != NULL;
}

XML::Type XML::GetValueType() const
{
	Guard cGuard;
	return Private::type_of( m->Node() );
}

/** The name of this element or attribute ("prefix:name" with a prefix), or
 * a processing instruction's target */
String XML::GetName() const
{
	Guard cGuard;
	xmlNodePtr psNode = m->Node();
	if( psNode == NULL || ( psNode->type != XML_ELEMENT_NODE && psNode->type != XML_ATTRIBUTE_NODE && psNode->type != XML_PI_NODE ) )
		return String();
	return String( Private::qualified_name( psNode ) );
}

/** The text of this element (all the text in it), attribute or other node */
String XML::AsString( const char* pzDefault ) const
{
	Guard cGuard;
	std::string cText;
	if( !Private::node_text( m->Node(), &cText ) )
		return String( pzDefault ? pzDefault : "" );
	return String( cText );
}

/** This node's text as an integer (or a number's whole part, or a bool) */
int64 XML::AsInt64( int64 nDefault ) const
{
	Guard cGuard;
	std::string cText;
	int64 n;
	return Private::node_text( m->Node(), &cText ) && Private::to_int64( cText, &n ) ? n : nDefault;
}

double XML::AsDouble( double vDefault ) const
{
	Guard cGuard;
	std::string cText;
	double v;
	return Private::node_text( m->Node(), &cText ) && Private::to_double( cText, &v ) ? v : vDefault;
}

/** true, false, yes, no, on, off, or a number (not 0 is true) */
bool XML::AsBool( bool bDefault ) const
{
	Guard cGuard;
	std::string cText;
	bool b;
	return Private::node_text( m->Node(), &cText ) && Private::to_bool( cText, &b ) ? b : bDefault;
}

bool XML::HasValue( const char* pzName, int nIndex ) const
{
	Guard cGuard;
	return m->lookup( pzName, nIndex ) != NULL;
}

XML::Type XML::GetType( const char* pzName, int nIndex ) const
{
	Guard cGuard;
	return Private::type_of( m->lookup( pzName, nIndex ) );
}

/** How many values a name has: the number of elements of the name (in the
 * element of the path before it), of those the number that have the
 * attribute ("item@id"), or the number of a query's matches */
int XML::GetCount( const char* pzName ) const
{
	Guard cGuard;
	xmlNodePtr psNode = m->Node();
	if( Private::is_query( pzName ) )
	{
		std::vector<xmlNodePtr> cMatches;
		std::string cError;
		m->run_query( psNode, pzName, &cMatches, NULL, &cError );
		return cMatches.size();
	}
	Private::Path cPath;
	if( !Private::is_element( psNode ) || !Private::parse_path( pzName, &cPath ) )
		return 0;
	if( cPath.m_cElements.empty() )
		return !cPath.m_bAttribute || Private::find_attribute( psNode, cPath.m_cAttribute ) ? 1 : 0;
	for( size_t i = 0; psNode != NULL && i + 1 < cPath.m_cElements.size(); i++ )
		psNode = Private::find_child( psNode, cPath.m_cElements[i], 0 );
	if( psNode == NULL )
		return 0;
	if( !cPath.m_bAttribute )
		return Private::count_children( psNode, cPath.m_cElements.back() );
	int nCount = 0;
	for( xmlNodePtr p = psNode->children; p != NULL; p = p->next )
		if( p->type == XML_ELEMENT_NODE && Private::matches( p, cPath.m_cElements.back() ) &&
		    Private::find_attribute( p, cPath.m_cAttribute ) )
			nCount++;
	return nCount;
}

/** Removes every element of a name (nIndex -1), or one of them; an
 * attribute (of each element of the path, or of element nIndex); or every
 * match of a query (or match nIndex).  Removing what isn't there is not an
 * error; the root element stays. */
XML& XML::RemoveValue( const char* pzName, int nIndex )
{
	Guard cGuard;
	xmlNodePtr psNode = m->Node();
	if( Private::is_query( pzName ) )
	{
		std::vector<xmlNodePtr> cMatches;
		std::string cError;
		if( psNode == NULL || !m->run_query( psNode, pzName, &cMatches, NULL, &cError ) )
		{
			m->Fail( ( cError + " in" ).c_str(), pzName );
			return *this;
		}
		if( nIndex >= 0 )
		{
			if( nIndex >= (int)cMatches.size() )
				return *this;
			cMatches = std::vector<xmlNodePtr>( 1, cMatches[nIndex] );
		}
		// Later matches first: descendants (and attributes) before the
		// elements they are in
		for( size_t i = cMatches.size(); i-- > 0; )
			Private::remove_node( m->Doc(), cMatches[i] );
		return *this;
	}
	Private::Path cPath;
	if( !Private::is_element( psNode ) || !Private::parse_path( pzName, &cPath ) )
		return *this;
	if( cPath.m_cElements.empty() )
	{
		if( cPath.m_bAttribute )
			Private::remove_node( m->Doc(), Private::find_attribute( psNode, cPath.m_cAttribute ) );
		return *this;
	}
	for( size_t i = 0; psNode != NULL && i + 1 < cPath.m_cElements.size(); i++ )
		psNode = Private::find_child( psNode, cPath.m_cElements[i], 0 );
	if( psNode == NULL )
		return *this;
	std::vector<xmlNodePtr> cElements;
	int nAt = 0;
	for( xmlNodePtr p = psNode->children; p != NULL; p = p->next )
		if( p->type == XML_ELEMENT_NODE && Private::matches( p, cPath.m_cElements.back() ) && ( nIndex < 0 || nAt++ == nIndex ) )
			cElements.push_back( p );
	for( size_t i = 0; i < cElements.size(); i++ )
		Private::remove_node( m->Doc(), cPath.m_bAttribute ? Private::find_attribute( cElements[i], cPath.m_cAttribute ) : cElements[i] );
	return *this;
}

/** Removes what is in this element: its elements, text and attributes (its
 * namespace declarations stay) */
XML& XML::MakeEmpty()
{
	Guard cGuard;
	xmlNodePtr psElement = m->Element();
	if( psElement != NULL )
	{
		Private::free_children( psElement );
		while( psElement->properties != NULL )
			xmlRemoveProp( psElement->properties );
	}
	return *this;
}

/** The text of an element (all the text in it) or attribute */
String XML::GetString( const char* pzName, const char* pzDefault, int nIndex ) const
{
	Guard cGuard;
	std::string cText;
	if( !Private::node_text( m->lookup( pzName, nIndex ), &cText ) )
		return String( pzDefault ? pzDefault : "" );
	return String( cText );
}

int8 XML::GetInt8( const char* pzName, int8 nDefault, int nIndex ) const
{
	return (int8)GetInt64( pzName, nDefault, nIndex );
}

int16 XML::GetInt16( const char* pzName, int16 nDefault, int nIndex ) const
{
	return (int16)GetInt64( pzName, nDefault, nIndex );
}

int32 XML::GetInt32( const char* pzName, int32 nDefault, int nIndex ) const
{
	return (int32)GetInt64( pzName, nDefault, nIndex );
}

/** An integer (or a number's whole part, or a bool: 1 or 0) */
int64 XML::GetInt64( const char* pzName, int64 nDefault, int nIndex ) const
{
	Guard cGuard;
	std::string cText;
	int64 n;
	return Private::node_text( m->lookup( pzName, nIndex ), &cText ) && Private::to_int64( cText, &n ) ? n : nDefault;
}

/** true, false, yes, no, on, off, or a number (not 0 is true) */
bool XML::GetBool( const char* pzName, bool bDefault, int nIndex ) const
{
	Guard cGuard;
	std::string cText;
	bool b;
	return Private::node_text( m->lookup( pzName, nIndex ), &cText ) && Private::to_bool( cText, &b ) ? b : bDefault;
}

float XML::GetFloat( const char* pzName, float vDefault, int nIndex ) const
{
	return (float)GetDouble( pzName, vDefault, nIndex );
}

double XML::GetDouble( const char* pzName, double vDefault, int nIndex ) const
{
	Guard cGuard;
	std::string cText;
	double v;
	return Private::node_text( m->lookup( pzName, nIndex ), &cText ) && Private::to_double( cText, &v ) ? v : vDefault;
}

/** The attributes left, top, right and bottom (or elements of those names),
 * or four numbers ("10 20 410 320") */
Rect XML::GetRect( const char* pzName, const Rect& cDefault, int nIndex ) const
{
	Guard cGuard;
	double av[4];
	if( !Private::get_numbers( m->lookup( pzName, nIndex ), Private::g_apzRect, 4, av ) )
		return cDefault;
	return Rect( av[0], av[1], av[2], av[3] );
}

IRect XML::GetIRect( const char* pzName, const IRect& cDefault, int nIndex ) const
{
	Guard cGuard;
	double av[4];
	if( !Private::get_numbers( m->lookup( pzName, nIndex ), Private::g_apzRect, 4, av ) )
		return cDefault;
	return IRect( (int)av[0], (int)av[1], (int)av[2], (int)av[3] );
}

/** The attributes x and y (or elements of those names), or two numbers */
Point XML::GetPoint( const char* pzName, const Point& cDefault, int nIndex ) const
{
	Guard cGuard;
	double av[2];
	if( !Private::get_numbers( m->lookup( pzName, nIndex ), Private::g_apzPoint, 2, av ) )
		return cDefault;
	return Point( av[0], av[1] );
}

IPoint XML::GetIPoint( const char* pzName, const IPoint& cDefault, int nIndex ) const
{
	Guard cGuard;
	double av[2];
	if( !Private::get_numbers( m->lookup( pzName, nIndex ), Private::g_apzPoint, 2, av ) )
		return cDefault;
	return IPoint( (int)av[0], (int)av[1] );
}

/** "#rrggbb", "#rrggbbaa" or "#rgb", or the attributes red, green, blue
 * (and alpha) */
Color32_s XML::GetColor32( const char* pzName, const Color32_s& cDefault, int nIndex ) const
{
	Guard cGuard;
	xmlNodePtr psNode = m->lookup( pzName, nIndex );
	std::string cText;
	Color32_s sColor;
	if( Private::node_text( psNode, &cText ) && Private::parse_color_hex( Private::trim( cText ).c_str(), &sColor ) )
		return sColor;
	double av[4];
	if( Private::is_element( psNode ) && Private::get_numbers( psNode, Private::g_apzColor, 3, av ) )
	{
		xmlNodePtr psAlpha = Private::find_attribute( psNode, "alpha" );
		if( !( psAlpha && Private::node_text( psAlpha, &cText ) && Private::to_double( cText, &av[3] ) ) )
			av[3] = 255;
		return Color32_s( (int)av[0], (int)av[1], (int)av[2], (int)av[3] );
	}
	return cDefault;
}

bool XML::ParseColor( const char* pzText, Color32_s* psColor )
{
	return pzText != NULL && psColor != NULL && Private::parse_color_hex( pzText, psColor );
}

XML& XML::SetString( const char* pzName, const String& cValue, int nIndex )
{
	Guard cGuard;
	m->Store( pzName, nIndex, Private::Value( std::string( cValue.c_str(), cValue.size() ) ) );
	return *this;
}

XML& XML::SetInt8( const char* pzName, int8 nValue, int nIndex )
{
	return SetInt64( pzName, nValue, nIndex );
}

XML& XML::SetInt16( const char* pzName, int16 nValue, int nIndex )
{
	return SetInt64( pzName, nValue, nIndex );
}

XML& XML::SetInt32( const char* pzName, int32 nValue, int nIndex )
{
	return SetInt64( pzName, nValue, nIndex );
}

XML& XML::SetInt64( const char* pzName, int64 nValue, int nIndex )
{
	Guard cGuard;
	m->Store( pzName, nIndex, Private::Value( Private::int_text( nValue ) ) );
	return *this;
}

/** "true" or "false" */
XML& XML::SetBool( const char* pzName, bool bValue, int nIndex )
{
	Guard cGuard;
	m->Store( pzName, nIndex, Private::Value( bValue ? "true" : "false" ) );
	return *this;
}

/** Saved as the shortest number that reads back as the same float (0.1, not
 * 0.100000001) */
XML& XML::SetFloat( const char* pzName, float vValue, int nIndex )
{
	Guard cGuard;
	m->Store( pzName, nIndex, Private::Value( Private::number_text( Private::float_to_double( vValue ) ) ) );
	return *this;
}

XML& XML::SetDouble( const char* pzName, double vValue, int nIndex )
{
	Guard cGuard;
	m->Store( pzName, nIndex, Private::Value( Private::number_text( vValue ) ) );
	return *this;
}

/** <frame left="10" top="20" right="410" bottom="320"/> (an attribute:
 * "10 20 410 320") */
XML& XML::SetRect( const char* pzName, const Rect& cValue, int nIndex )
{
	Guard cGuard;
	double av[4] = { Private::float_to_double( cValue.left ), Private::float_to_double( cValue.top ),
	                 Private::float_to_double( cValue.right ), Private::float_to_double( cValue.bottom ) };
	m->Store( pzName, nIndex, Private::Value( Private::g_apzRect, 4, av ) );
	return *this;
}

XML& XML::SetIRect( const char* pzName, const IRect& cValue, int nIndex )
{
	Guard cGuard;
	double av[4] = { (double)cValue.left, (double)cValue.top, (double)cValue.right, (double)cValue.bottom };
	m->Store( pzName, nIndex, Private::Value( Private::g_apzRect, 4, av ) );
	return *this;
}

/** <pos x="10" y="20"/> (an attribute: "10 20") */
XML& XML::SetPoint( const char* pzName, const Point& cValue, int nIndex )
{
	Guard cGuard;
	double av[2] = { Private::float_to_double( cValue.x ), Private::float_to_double( cValue.y ) };
	m->Store( pzName, nIndex, Private::Value( Private::g_apzPoint, 2, av ) );
	return *this;
}

XML& XML::SetIPoint( const char* pzName, const IPoint& cValue, int nIndex )
{
	Guard cGuard;
	double av[2] = { (double)cValue.x, (double)cValue.y };
	m->Store( pzName, nIndex, Private::Value( Private::g_apzPoint, 2, av ) );
	return *this;
}

/** "#rrggbb", with the alpha ("#rrggbbaa") when it isn't 255 */
XML& XML::SetColor32( const char* pzName, const Color32_s& cValue, int nIndex )
{
	Guard cGuard;
	m->Store( pzName, nIndex, Private::Value( Private::color_text( cValue ) ) );
	return *this;
}

XML& XML::AddString( const char* pzName, const String& cValue )
{
	Guard cGuard;
	m->Store( pzName, 0, Private::Value( std::string( cValue.c_str(), cValue.size() ) ), true );
	return *this;
}

XML& XML::AddInt32( const char* pzName, int32 nValue )
{
	return AddInt64( pzName, nValue );
}

XML& XML::AddInt64( const char* pzName, int64 nValue )
{
	Guard cGuard;
	m->Store( pzName, 0, Private::Value( Private::int_text( nValue ) ), true );
	return *this;
}

XML& XML::AddBool( const char* pzName, bool bValue )
{
	Guard cGuard;
	m->Store( pzName, 0, Private::Value( bValue ? "true" : "false" ), true );
	return *this;
}

XML& XML::AddDouble( const char* pzName, double vValue )
{
	Guard cGuard;
	m->Store( pzName, 0, Private::Value( Private::number_text( vValue ) ), true );
	return *this;
}

bool XML::HasError() const
{
	Guard cGuard;
	return m->m_pcDocument->m_bError;
}

String XML::GetLastError() const
{
	Guard cGuard;
	return String( m->m_pcDocument->m_cError.c_str() );
}

void XML::ClearError()
{
	Guard cGuard;
	m->m_pcDocument->m_bError = false;
	m->m_pcDocument->m_cError.clear();
}
