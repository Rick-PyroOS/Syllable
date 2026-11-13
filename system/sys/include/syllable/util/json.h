/*  JSON - JSON documents with the Get/Set methods of os::Settings.
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

#ifndef	__F_UTIL_JSON_H__
#define	__F_UTIL_JSON_H__

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

/** A JSON object, with the Get and Set methods of os::Settings.
 * \ingroup util
 * \par Description:
 *	A JSON is one object in a JSON document: the top-level object of a new
 *	document, or an object inside one (GetObject(), SetObject()).  Copying
 *	a JSON copies the reference: both refer to the same object, and the
 *	document stays alive as long as any JSON refers into it.  Clone()
 *	makes an independent copy.
 *
 *	Names can be paths: "window.frame" is the value "frame" in the object
 *	"window".  The Set methods create the objects of a path that don't
 *	exist yet.  A name with a '.' in it is reached through GetObject() of
 *	its object, or FromMessage().
 *
 *	nIndex picks an element of a JSON array, as it picks one of the values
 *	of a name in os::Settings.  Index 0 of a value that isn't an array is
 *	the value itself, and setting a higher index makes the value an array
 *	(the old value becomes element 0, gaps are null).  The Add methods
 *	append to the array of a name (making it one).
 *
 *	The Set and Add methods return the JSON, so they chain.  SetObject()
 *	and AddObject() return the new object, and End() goes back up to the
 *	object it is in:
 * \code
 *	os::JSON cDoc;
 *	cDoc.SetString( "name", "Pyro" )
 *	    .SetObject( "window" )
 *	        .SetRect( "frame", os::Rect( 10, 20, 410, 320 ) )
 *	        .SetObject( "view" ).SetString( "mode", "icons" ).SetInt32( "zoom", 2 ).End()
 *	    .End()
 *	    .AddObject( "recent" ).SetString( "path", "/home/root/a.txt" ).End()
 *	    .AddString( "tags", "os" ).AddString( "tags", "desktop" )
 *	    .SetBool( "enabled", true );
 *	if( cDoc.HasError() )
 *	    printf( "%s\n", cDoc.GetLastError().c_str() );
 *	cDoc.Save( "/home/root/pyro.json" );
 *
 *	os::Rect cFrame = cDoc.GetRect( "window.frame", os::Rect( 0, 0, 400, 300 ) );
 *	for( int i = 0; i < cDoc.GetCount( "recent" ); i++ )
 *	    printf( "%s\n", cDoc.GetObject( "recent", i ).GetString( "path" ).c_str() );
 * \endcode
 *
 *	JSONPath (RFC 9535) queries pick values out of a document:
 * \code
 *	std::vector<os::JSON> cTitles = cDoc.Query( "$.store.book[?@.price < 10].title" );
 *	for( size_t i = 0; i < cTitles.size(); i++ )
 *	    printf( "%s\n", cTitles[i].AsString().c_str() );
 *	double vMax = cDoc.GetDouble( "$..book[-1].price", 0.0 );	// the Get methods take queries too
 * \endcode
 *	A name is a query when it is "$" or starts with "$." or "$[" ("$schema"
 *	is a name).  The Get methods, HasValue(), GetType() and GetObject() take
 *	the match nIndex picks, GetCount() counts the matches and RemoveValue()
 *	removes them all.  The Set methods take names, not queries.
 *	Supported: .name ['name'] .* [*] [3] [-1] [start:end:step] [a,b] ..name
 *	and filters ([?@.price < 10 && @.isbn], == != < <= > >=, && || !) with
 *	the RFC's length(), count(), value(), match() and search().
 *
 *	os::Message and os::Settings: a JSON is an os::Flattenable (T_JSON),
 *	flattened to its text, so it goes into a message or the settings as a
 *	value, as a string does:
 * \code
 *	cMsg.AddObject( "config", cDoc );			// send it
 *	os::JSON cGot;  cMsg.FindObject( "config", cGot );	// receive it
 *	cSettings.SetJSON( "layout", cDoc );			// keep it in the settings
 *	os::JSON cLayout = cSettings.GetJSON( "layout" );
 * \endcode
 *	FromMessage() and ToMessage() convert a whole message to and from JSON
 *	(names become members, nested messages objects).
 *
 *	JSON Schema: give the schema to the constructor, or to SetSchema(), and
 *	IsValid() checks the document against it (without a schema, every
 *	document is valid):
 * \code
 *	os::JSON cDoc( os::File( "/system/resources/myapp/settings.schema.json" ) );
 *	cDoc.Load( cPath );
 *	if( !cDoc.IsValid() )
 *	{
 *	    std::vector<os::String> cErrors = cDoc.GetValidationErrors();
 *	    for( size_t i = 0; i < cErrors.size(); i++ )
 *	        printf( "%s\n", cErrors[i].c_str() );	// "$.window.width: -1 is less than the minimum 0"
 *	}
 * \endcode
 *	The validation keywords of JSON Schema 2020-12 and draft-07 are
 *	supported, with $ref inside the schema ("#/$defs/...", "#anchor").
 *	Not supported: $ref to other files or URLs, $dynamicRef,
 *	unevaluatedProperties and unevaluatedItems; "format" is not checked.
 *
 *	A Set that can't be done (a path through a value that isn't an object,
 *	or a JSON that refers to nothing) does nothing and is recorded:
 *	HasError(), GetLastError().
 *
 *	Numbers are doubles, as in JavaScript: int64 values beyond 2^53 are
 *	stored as strings, and read back from either.  Rects and points are
 *	objects ({ "left", "top", "right", "bottom" }, { "x", "y" }), colors
 *	are strings ("#rrggbb", or "#rrggbbaa" when not opaque).
 *
 *	A JSON that refers to an object that is then removed or replaced
 *	(RemoveValue(), a Set of another value on its name, Load() or
 *	FromString() of the object it is in) must not be used any more.
 *	Not thread safe: lock around a document used from more than one thread.
 *
 * \sa os::Settings
 *****************************************************************************/

class JSON : public Flattenable
{
public:
	enum Type { JSON_NONE, JSON_NULL, JSON_BOOL, JSON_NUMBER, JSON_STRING, JSON_ARRAY, JSON_OBJECT };

	JSON();
	JSON( const JSON& cOther );
	virtual ~JSON();
	JSON& operator=( const JSON& cOther );

	explicit JSON( const File& cSchema );

	JSON		Clone() const;
	bool		IsObject() const;

	// JSON Schema: IsValid() is true without a schema, and otherwise tells
	// whether the document matches it (GetValidationErrors() says where not)
	status_t	SetSchema( const File& cSchema );
	status_t	SetSchema( const JSON& cSchema );
	void		ClearSchema();
	bool		HasSchema() const;
	bool		IsValid() const;
	std::vector<String>	GetValidationErrors() const;

	// Text and files: Load() and FromString() replace this object's contents
	status_t	FromString( const String& cText );
	String		ToString( bool bFormatted = true ) const;
	status_t	Load( const String& cPath );
	status_t	Save( const String& cPath ) const;

	// os::Message: names become members, several values of a name an array,
	// a message in a message an object
	static JSON	FromMessage( const Message& cMessage );
	status_t	ToMessage( Message* pcMessage ) const;

	// Flattenable (T_JSON): a JSON goes into an os::Message or os::Settings
	// as it is (Message::AddObject(), FindObject(), Settings::SetJSON()),
	// flattened to its text
	virtual int			GetType() const;
	virtual size_t		GetFlattenedSize() const;
	virtual status_t	Flatten( uint8* pBuffer, size_t nSize ) const;
	virtual status_t	Unflatten( const uint8* pBuffer );

	// Nested objects
	JSON		GetObject( const char* pzName, int nIndex = 0 ) const;
	JSON		SetObject( const char* pzName, int nIndex = 0 );
	JSON		AddObject( const char* pzName );
	JSON		End() const;
	JSON		Top() const;

	// JSONPath queries: every value they select (of any type, not only
	// objects), or the first.  An invalid query selects nothing and is
	// recorded (GetLastError()).
	std::vector<JSON>	Query( const String& cQuery ) const;
	JSON		QueryFirst( const String& cQuery ) const;

	// The value a JSON refers to (one from Query() can be any value)
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
	int			CountMembers() const;
	String		GetMemberName( int nIndex ) const;
	JSON&		RemoveValue( const char* pzName, int nIndex = -1 );
	JSON&		MakeEmpty();

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

	JSON&		SetString( const char* pzName, const String& cValue, int nIndex = 0 );
	JSON&		SetInt8( const char* pzName, int8 nValue, int nIndex = 0 );
	JSON&		SetInt16( const char* pzName, int16 nValue, int nIndex = 0 );
	JSON&		SetInt32( const char* pzName, int32 nValue, int nIndex = 0 );
	JSON&		SetInt64( const char* pzName, int64 nValue, int nIndex = 0 );
	JSON&		SetBool( const char* pzName, bool bValue, int nIndex = 0 );
	JSON&		SetFloat( const char* pzName, float vValue, int nIndex = 0 );
	JSON&		SetDouble( const char* pzName, double vValue, int nIndex = 0 );
	JSON&		SetRect( const char* pzName, const Rect& cValue, int nIndex = 0 );
	JSON&		SetIRect( const char* pzName, const IRect& cValue, int nIndex = 0 );
	JSON&		SetPoint( const char* pzName, const Point& cValue, int nIndex = 0 );
	JSON&		SetIPoint( const char* pzName, const IPoint& cValue, int nIndex = 0 );
	JSON&		SetColor32( const char* pzName, const Color32_s& cValue, int nIndex = 0 );
	JSON&		SetNull( const char* pzName, int nIndex = 0 );

	JSON&		AddString( const char* pzName, const String& cValue );
	JSON&		AddInt32( const char* pzName, int32 nValue );
	JSON&		AddInt64( const char* pzName, int64 nValue );
	JSON&		AddBool( const char* pzName, bool bValue );
	JSON&		AddDouble( const char* pzName, double vValue );

	/** Whether a Set, Add, Load or FromString of this document failed since
	 * the last ClearError() */
	bool		HasError() const;
	String		GetLastError() const;
	void		ClearError();

	/** "#rgb", "#rrggbb" or "#rrggbbaa" */
	static bool	ParseColor( const char* pzText, Color32_s* psColor );

private:
	class Private;
	friend class Private;
	__attribute__((visibility("hidden"))) JSON( Private* pcPrivate );

	Private* m;
};

} // end of namespace

#endif // __F_UTIL_JSON_H__
