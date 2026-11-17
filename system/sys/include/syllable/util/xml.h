/*  XML - XML documents with the Get/Set methods of os::Settings.
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

#ifndef	__F_UTIL_XML_H__
#define	__F_UTIL_XML_H__

#include <util/string.h>
#include <util/flattenable.h>
#include <storage/file.h>
#include <gui/rect.h>
#include <gui/point.h>
#include <gui/gfxtypes.h>
#include <vector>

namespace os
{

class Message;

/** An XML element, with the Get and Set methods of os::Settings.
 * \ingroup util
 * \par Description:
 *	An XML is one element of an XML document: the root element of a new
 *	document, or an element inside one (GetElement(), SetElement()).
 *	Copying an XML copies the reference: both refer to the same element,
 *	and the document stays alive as long as any XML refers into it.
 *	Clone() makes an independent copy.  It is os::JSON's interface for XML,
 *	built on libxml2.
 *
 *	Names are paths of element names: "window.frame" is the element
 *	<frame> in the element <window>.  A name can end in an attribute:
 *	"window.frame@left" is the attribute left of that element, and "@id"
 *	the attribute id of this one.  The Set methods create the elements of
 *	a path that don't exist yet.  The value of an element is its text, of
 *	an attribute its value; a Set of an element's text replaces what is
 *	in it.  Prefixed names ("svg:rect", "@xlink:href") match the
 *	element's or attribute's namespace prefix; a name without a prefix
 *	matches elements without one (in the default namespace, or none).
 *
 *	nIndex picks one of the elements of a name, as it picks one of the
 *	values of a name in os::Settings: "item", 2 is the third <item>.
 *	Setting an index past the last one adds empty elements up to it.  The
 *	Add methods add another element of the name, after the last one.
 *
 *	The Set and Add methods return the XML, so they chain.  SetElement()
 *	and AddElement() return the new element, and End() goes back up to the
 *	element it is in:
 * \code
 *	os::XML cDoc( "settings" );
 *	cDoc.SetString( "name", "Pyro" )
 *	    .SetElement( "window" )
 *	        .SetRect( "frame", os::Rect( 10, 20, 410, 320 ) )	// <frame left="10" top="20" right="410" bottom="320"/>
 *	        .SetElement( "view" ).SetString( "@mode", "icons" ).SetInt32( "zoom", 2 ).End()
 *	    .End()
 *	    .AddElement( "recent" ).SetString( "@path", "/home/root/a.txt" ).End()
 *	    .AddString( "tag", "os" ).AddString( "tag", "desktop" )
 *	    .SetBool( "enabled", true );
 *	if( cDoc.HasError() )
 *	    printf( "%s\n", cDoc.GetLastError().c_str() );
 *	cDoc.Save( "/home/root/pyro.xml" );
 *
 *	os::Rect cFrame = cDoc.GetRect( "window.frame", os::Rect( 0, 0, 400, 300 ) );
 *	for( int i = 0; i < cDoc.GetCount( "recent" ); i++ )
 *	    printf( "%s\n", cDoc.GetString( "recent@path", "", i ).c_str() );
 * \endcode
 *
 *	XPath 1.0 queries pick nodes out of a document:
 * \code
 *	std::vector<os::XML> cTitles = cDoc.Query( "//book[price < 10]/title" );
 *	for( size_t i = 0; i < cTitles.size(); i++ )
 *	    printf( "%s\n", cTitles[i].AsString().c_str() );
 *	double vMax = cDoc.GetDouble( "//book[last()]/price", 0.0 );	// the Get methods take queries too
 *	int nBooks = atoi( cDoc.Evaluate( "count(//book)" ).c_str() );
 * \endcode
 *	A name is a query when it starts with "/", "./" or "../"; "./" and
 *	"../" start from this element.  The Get methods, HasValue(),
 *	GetType() and GetElement() take the match nIndex picks, GetCount()
 *	counts the matches and RemoveValue() removes them all.  The Set
 *	methods take names, not queries.  Queries know the namespace prefixes
 *	declared where they start, and those given to RegisterNamespace() (the
 *	only way to reach a default namespace: XPath 1.0 has no default).
 *
 *	os::Message and os::Settings: an XML is an os::Flattenable (T_XML),
 *	flattened to its text, so it goes into a message or the settings as a
 *	value, as a string does:
 * \code
 *	cMsg.AddObject( "config", cDoc );			// send it
 *	os::XML cGot;  cMsg.FindObject( "config", cGot );	// receive it
 *	cSettings.SetXML( "layout", cDoc );			// keep it in the settings
 *	os::XML cLayout = cSettings.GetXML( "layout" );
 * \endcode
 *	FromMessage() and ToMessage() convert a whole message to and from XML
 *	(names become elements, nested messages elements with elements in
 *	them).
 *
 *	Validation: give a schema to the constructor, or to SetSchema(), and
 *	IsValid() checks the document against it.  The schema can be a W3C XML
 *	Schema (XSD), a Relax-NG schema (its XML syntax) or a DTD.  Without
 *	one, IsValid() checks the document against its own DTD (a <!DOCTYPE
 *	with declarations in it), and is true when it has none:
 * \code
 *	os::XML cDoc( os::File( "/system/resources/myapp/settings.xsd" ) );
 *	cDoc.Load( cPath );
 *	if( !cDoc.IsValid() )
 *	{
 *	    std::vector<os::String> cErrors = cDoc.GetValidationErrors();
 *	    for( size_t i = 0; i < cErrors.size(); i++ )
 *	        printf( "%s\n", cErrors[i].c_str() );	// "/settings/window/width: ... '-1' is less than ... '0'"
 *	}
 * \endcode
 *	A schema's references to other files (xs:include, xs:import, a Relax-NG
 *	include, a DTD's external parameter entities) are not followed, and
 *	external DTDs and entities in documents are not loaded: nothing reads
 *	the network or other files.
 *
 *	A Set that can't be done (a name that isn't an XML name, a prefix that
 *	isn't declared, a path through an attribute, or an XML that refers to
 *	nothing) does nothing and is recorded: HasError(), GetLastError().
 *
 *	Values are text: numbers are written as the shortest decimal that
 *	reads back the same, bools as "true" and "false" (read: also "1",
 *	"0", "yes", "no", "on", "off"), colors as "#rrggbb" ("#rrggbbaa" when
 *	not opaque).  Rects and points are attributes of their element
 *	(left, top, right, bottom; x, y), or four and two numbers in an
 *	attribute ("10 20 410 320").
 *
 *	Whitespace-only text between elements is not kept when a document is
 *	read (except under xml:space="preserve"), so ToString() and Save()
 *	indent the whole document anew.
 *
 *	An XML that refers to an element that is then removed or replaced
 *	(RemoveValue(), a Set of text on an element it is in, Load() or
 *	FromString() of an element it is in) must not be used any more; one
 *	that refers to the root element (the constructor's, Top()) stays good.
 *	The XML calls of all documents are serialized (libxml2 is built
 *	without threads), but a document used from more than one thread needs
 *	a lock around what is done with it.
 *
 * \sa os::JSON, os::Settings
 *****************************************************************************/

class XML : public Flattenable
{
public:
	enum Type { XML_NONE, XML_ELEMENT, XML_ATTRIBUTE, XML_TEXT, XML_CDATA, XML_COMMENT, XML_PI };

	XML();
	explicit XML( const char* pzRootName );
	XML( const XML& cOther );
	virtual ~XML();
	XML& operator=( const XML& cOther );

	explicit XML( const File& cSchema );

	XML			Clone() const;
	bool		IsElement() const;

	// Validation (XML Schema, Relax-NG or a DTD): IsValid() is true without
	// a schema (or the document's own DTD), and otherwise tells whether the
	// document matches it (GetValidationErrors() says where not)
	status_t	SetSchema( const File& cSchema );
	status_t	SetSchema( const XML& cSchema );
	status_t	SetSchemaString( const String& cSchema );
	void		ClearSchema();
	bool		HasSchema() const;
	bool		IsValid() const;
	std::vector<String>	GetValidationErrors() const;

	// Text and files: Load() and FromString() replace this element
	// (at the root: the whole document)
	status_t	FromString( const String& cText );
	String		ToString( bool bFormatted = true ) const;
	status_t	Load( const String& cPath );
	status_t	Save( const String& cPath ) const;

	// os::Message: names become elements (one per value), a message in a
	// message an element with elements in it
	static XML	FromMessage( const Message& cMessage, const char* pzRootName = "message" );
	status_t	ToMessage( Message* pcMessage ) const;

	// Flattenable (T_XML): an XML goes into an os::Message or os::Settings
	// as it is (Message::AddObject(), FindObject(), Settings::SetXML()),
	// flattened to its text
	virtual int			GetType() const;
	virtual size_t		GetFlattenedSize() const;
	virtual status_t	Flatten( uint8* pBuffer, size_t nSize ) const;
	virtual status_t	Unflatten( const uint8* pBuffer );

	// Nested elements
	XML			GetElement( const char* pzName, int nIndex = 0 ) const;
	XML			SetElement( const char* pzName, int nIndex = 0 );
	XML			AddElement( const char* pzName );
	XML			End() const;
	XML			Top() const;
	int			CountChildren() const;
	XML			GetChild( int nIndex ) const;
	String		GetChildName( int nIndex ) const;
	int			CountAttributes() const;
	String		GetAttributeName( int nIndex ) const;

	// This element itself
	XML&		SetName( const char* pzName );
	XML&		SetText( const String& cText );
	XML&		AddComment( const String& cText );
	XML&		DeclareNamespace( const char* pzPrefix, const char* pzURI );
	String		GetNamespaceURI() const;

	// XPath 1.0 queries: every node they select (elements, attributes,
	// text...), or the first.  Evaluate() gives the value of any expression
	// as text (count(), sum(), string()...).  An invalid query selects
	// nothing and is recorded (GetLastError()).
	std::vector<XML>	Query( const String& cQuery ) const;
	XML			QueryFirst( const String& cQuery ) const;
	String		Evaluate( const String& cExpression ) const;
	status_t	RegisterNamespace( const char* pzPrefix, const char* pzURI );

	// The node an XML refers to (one from Query() can be any node)
	bool		Exists() const;
	Type		GetValueType() const;
	String		GetName() const;
	String		AsString( const char* pzDefault = "" ) const;
	int64		AsInt64( int64 nDefault = 0 ) const;
	double		AsDouble( double vDefault = 0.0 ) const;
	bool		AsBool( bool bDefault = false ) const;

	// What is there
	bool		HasValue( const char* pzName, int nIndex = 0 ) const;
	Type		GetType( const char* pzName, int nIndex = 0 ) const;
	int			GetCount( const char* pzName ) const;
	XML&		RemoveValue( const char* pzName, int nIndex = -1 );
	XML&		MakeEmpty();

	String		GetString( const char* pzName, const char* pzDefault = "", int nIndex = 0 ) const;
	int8		GetInt8( const char* pzName, int8 nDefault, int nIndex = 0 ) const;
	int16		GetInt16( const char* pzName, int16 nDefault, int nIndex = 0 ) const;
	int32		GetInt32( const char* pzName, int32 nDefault, int nIndex = 0 ) const;
	int64		GetInt64( const char* pzName, int64 nDefault, int nIndex = 0 ) const;
	bool		GetBool( const char* pzName, bool bDefault, int nIndex = 0 ) const;
	float		GetFloat( const char* pzName, float vDefault, int nIndex = 0 ) const;
	double		GetDouble( const char* pzName, double vDefault, int nIndex = 0 ) const;
	Rect		GetRect( const char* pzName, const Rect& cDefault, int nIndex = 0 ) const;
	IRect		GetIRect( const char* pzName, const IRect& cDefault, int nIndex = 0 ) const;
	Point		GetPoint( const char* pzName, const Point& cDefault, int nIndex = 0 ) const;
	IPoint		GetIPoint( const char* pzName, const IPoint& cDefault, int nIndex = 0 ) const;
	Color32_s	GetColor32( const char* pzName, const Color32_s& cDefault, int nIndex = 0 ) const;

	XML&		SetString( const char* pzName, const String& cValue, int nIndex = 0 );
	XML&		SetInt8( const char* pzName, int8 nValue, int nIndex = 0 );
	XML&		SetInt16( const char* pzName, int16 nValue, int nIndex = 0 );
	XML&		SetInt32( const char* pzName, int32 nValue, int nIndex = 0 );
	XML&		SetInt64( const char* pzName, int64 nValue, int nIndex = 0 );
	XML&		SetBool( const char* pzName, bool bValue, int nIndex = 0 );
	XML&		SetFloat( const char* pzName, float vValue, int nIndex = 0 );
	XML&		SetDouble( const char* pzName, double vValue, int nIndex = 0 );
	XML&		SetRect( const char* pzName, const Rect& cValue, int nIndex = 0 );
	XML&		SetIRect( const char* pzName, const IRect& cValue, int nIndex = 0 );
	XML&		SetPoint( const char* pzName, const Point& cValue, int nIndex = 0 );
	XML&		SetIPoint( const char* pzName, const IPoint& cValue, int nIndex = 0 );
	XML&		SetColor32( const char* pzName, const Color32_s& cValue, int nIndex = 0 );

	XML&		AddString( const char* pzName, const String& cValue );
	XML&		AddInt32( const char* pzName, int32 nValue );
	XML&		AddInt64( const char* pzName, int64 nValue );
	XML&		AddBool( const char* pzName, bool bValue );
	XML&		AddDouble( const char* pzName, double vValue );

	/** Whether a Set, Add, Load, FromString or query of this document
	 * failed since the last ClearError() */
	bool		HasError() const;
	String		GetLastError() const;
	void		ClearError();

	/** "#rgb", "#rrggbb" or "#rrggbbaa" */
	static bool	ParseColor( const char* pzText, Color32_s* psColor );

private:
	class Private;
	friend class Private;
	__attribute__((visibility("hidden"))) XML( Private* pcPrivate );

	Private* m;
};

} // end of namespace

#endif // __F_UTIL_XML_H__
