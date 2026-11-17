/*  os::JSON: the Get/Set methods, nesting, files, Flattenable and os::Message.
 */

#include "test_util.h"

#include <util/json.h>
#include <util/message.h>

using namespace os;

namespace
{

/* A document built by chaining; the handle returned is a nested object,
 * which keeps the document alive */
JSON Build()
{
	JSON cDoc;
	cDoc.SetString( "name", "Pyro" )
	    .SetObject( "window" )
	        .SetRect( "frame", Rect( 10, 20, 410.5f, 320 ) )
	        .SetObject( "view" ).SetString( "mode", "icons" ).SetInt32( "zoom", 2 ).End()
	        .SetBool( "maximized", false )
	    .End()
	    .AddObject( "recent" ).SetString( "path", "/a" ).SetInt32( "line", 1 ).End()
	    .AddObject( "recent" ).SetString( "path", "/b" ).End()
	    .AddString( "tags", "os" ).AddString( "tags", "desktop" )
	    .SetFloat( "zoom", 0.1f )
	    .SetColor32( "color", Color32_s( 255, 128, 0 ) )
	    .SetInt64( "big", 9007199254740993LL );
	return cDoc.GetObject( "window.view" );
}

} // namespace

TEST( JSONTest, ChainedSetsReadBack )
{
	JSON cView = Build();
	EXPECT_STREQ( "icons", cView.GetString( "mode" ).c_str() );
	JSON cDoc = cView.Top();
	EXPECT_FALSE( cDoc.HasError() );
	EXPECT_STREQ( "Pyro", cDoc.GetString( "name" ).c_str() );
	EXPECT_EQ( 410.5f, cDoc.GetRect( "window.frame", Rect( 0, 0, 0, 0 ) ).right );
	EXPECT_FALSE( cDoc.GetBool( "window.maximized", true ) );
	EXPECT_EQ( 2, cDoc.GetCount( "recent" ) );
	EXPECT_EQ( JSON::JSON_OBJECT, cDoc.GetType( "recent" ) );
	EXPECT_STREQ( "/b", cDoc.GetObject( "recent", 1 ).GetString( "path" ).c_str() );
	EXPECT_EQ( 2, cDoc.GetCount( "tags" ) );
	EXPECT_STREQ( "desktop", cDoc.GetString( "tags", "", 1 ).c_str() );
	EXPECT_EQ( 0.1f, cDoc.GetFloat( "zoom", 0 ) );
	EXPECT_EQ( 9007199254740993LL, cDoc.GetInt64( "big", 0 ) );
	EXPECT_EQ( 2, cView.End().GetObject( "view" ).GetInt32( "zoom", 0 ) );
	EXPECT_FALSE( cDoc.End().IsObject() );
}

TEST( JSONTest, FloatsAndColorsAreWrittenShort )
{
	JSON cDoc = Build().Top();
	String cText = cDoc.ToString( false );
	EXPECT_TRUE( Contains( cText, "\"zoom\":0.1" ) );
	EXPECT_TRUE( Contains( cText, "\"color\":\"#ff8000\"" ) );
	EXPECT_TRUE( Contains( cText, "\"big\":\"9007199254740993\"" ) );	// past 2^53: a string
}

TEST( JSONTest, OneAddedObjectIsStillAnArray )
{
	JSON cOne;
	cOne.AddObject( "list" ).SetInt32( "a", 1 );
	EXPECT_TRUE( Contains( cOne.ToString( false ), "\"list\":[{" ) );
}

TEST( JSONTest, ErrorsAreRecordedAndChainsGoOn )
{
	JSON cDoc = Build().Top();
	cDoc.SetInt32( "name.sub", 1 ).SetInt32( "after", 5 );
	EXPECT_TRUE( cDoc.HasError() );
	EXPECT_EQ( 5, cDoc.GetInt32( "after", 0 ) );
	cDoc.ClearError();
	EXPECT_FALSE( cDoc.HasError() );
	cDoc.End().SetInt32( "x", 1 );
	EXPECT_TRUE( cDoc.HasError() );
}

TEST( JSONTest, CloneIsIndependentCopiesShare )
{
	JSON cDoc = Build().Top();
	JSON cCopy = cDoc.Clone();
	cCopy.SetString( "name", "Other" );
	JSON cShare = cDoc;
	cShare.SetString( "shared", "yes" );
	EXPECT_STREQ( "Pyro", cDoc.GetString( "name" ).c_str() );
	EXPECT_STREQ( "yes", cDoc.GetString( "shared" ).c_str() );
	EXPECT_STREQ( "Other", cCopy.GetString( "name" ).c_str() );
}

class JSONFileTest : public FileTest {};

TEST_F( JSONFileTest, SaveMakesFoldersAndLoadReadsBack )
{
	JSON cDoc = Build().Top();
	std::string cPath = Path( "sub/dir/doc.json" );
	ASSERT_EQ( 0, cDoc.Save( cPath.c_str() ) );
	JSON cLoaded;
	ASSERT_EQ( 0, cLoaded.Load( cPath.c_str() ) );
	EXPECT_STREQ( cDoc.ToString().c_str(), cLoaded.ToString().c_str() );
}

TEST_F( JSONFileTest, BadTextLeavesTheObject )
{
	JSON cDoc;
	cDoc.SetString( "name", "Pyro" );
	EXPECT_LT( cDoc.FromString( "{ \"a\": [1, 2,, ] }" ), 0 );
	EXPECT_STREQ( "Pyro", cDoc.GetString( "name" ).c_str() );
	EXPECT_TRUE( Contains( cDoc.GetLastError(), "line 1" ) );
	EXPECT_LT( cDoc.Load( Path( "none.json" ).c_str() ), 0 );
	EXPECT_STREQ( "Pyro", cDoc.GetString( "name" ).c_str() );
}

TEST( JSONTest, FromStringIntoANestedObjectKeepsHandles )
{
	JSON cDoc = Build().Top();
	JSON cWin = cDoc.GetObject( "window" );
	ASSERT_EQ( 0, cWin.FromString( "{\"w\": 3}" ) );
	EXPECT_EQ( 3, cDoc.GetInt32( "window.w", 0 ) );
	EXPECT_EQ( 3, cWin.GetInt32( "w", 0 ) );
}

TEST( JSONTest, MessageToJSONToTextAndBack )
{
	Message cMsg, cChild;
	cMsg.AddString( "title", "Hello" );
	cMsg.AddInt32( "count", 3 );
	cMsg.AddInt32( "count", 4 );
	cMsg.AddInt64( "huge", 9007199254740993LL );
	cMsg.AddFloat( "scale", 1.25f );
	cMsg.AddBool( "on", true );
	cMsg.AddRect( "frame", Rect( 1, 2, 3, 4.5f ) );
	cMsg.AddIPoint( "pos", IPoint( 7, 8 ) );
	cMsg.AddColor32( "bg", Color32_s( 1, 2, 3, 4 ) );
	cMsg.AddString( "dotted.name", "kept" );
	cMsg.AddPointer( "ptr", NULL );
	cChild.AddString( "inner", "x" );
	cChild.AddInt8( "small", -5 );
	cMsg.AddMessage( "child", &cChild );
	cMsg.AddMessage( "child", &cChild );

	JSON cFromMsg = JSON::FromMessage( cMsg );
	EXPECT_FALSE( cFromMsg.HasValue( "ptr" ) );
	EXPECT_EQ( -5, cFromMsg.GetObject( "child", 1 ).GetInt8( "small", 0 ) );

	JSON cParsed;
	ASSERT_EQ( 0, cParsed.FromString( cFromMsg.ToString() ) );
	Message cBack;
	ASSERT_EQ( 0, cParsed.ToMessage( &cBack ) );
	const char* pz = NULL;
	int32 n = 0;
	double v = 0;
	Rect r;
	Point p;
	Color32_s c( 0, 0, 0 );
	Message cChildBack;
	EXPECT_EQ( 0, cBack.FindString( "title", &pz ) );
	EXPECT_STREQ( "Hello", pz );
	EXPECT_EQ( 0, cBack.FindInt32( "count", &n, 1 ) );
	EXPECT_EQ( 4, n );
	EXPECT_EQ( 0, cBack.FindString( "huge", &pz ) );
	EXPECT_STREQ( "9007199254740993", pz );
	EXPECT_EQ( 0, cBack.FindDouble( "scale", &v ) );
	EXPECT_EQ( 1.25, v );
	EXPECT_EQ( 0, cBack.FindRect( "frame", &r ) );
	EXPECT_EQ( 4.5f, r.bottom );
	EXPECT_EQ( 0, cBack.FindPoint( "pos", &p ) );
	EXPECT_EQ( 8, p.y );
	EXPECT_EQ( 0, cBack.FindString( "bg", &pz ) );
	EXPECT_STREQ( "#01020304", pz );
	EXPECT_TRUE( JSON::ParseColor( pz, &c ) );
	EXPECT_EQ( 4, c.alpha );
	EXPECT_EQ( 0, cBack.FindString( "dotted.name", &pz ) );
	EXPECT_STREQ( "kept", pz );
	ASSERT_EQ( 0, cBack.FindMessage( "child", &cChildBack, 1 ) );
	EXPECT_EQ( 0, cChildBack.FindInt32( "small", &n ) );
	EXPECT_EQ( -5, n );
}

TEST( JSONTest, FlattensIntoAMessageAsItIs )
{
	JSON cDoc;
	cDoc.SetString( "name", "Pyro" ).SetObject( "w" ).SetInt32( "x", 1 ).End().AddInt32( "list", 5 );
	EXPECT_EQ( (int)T_JSON, cDoc.GetType() );
	Message cMsg;
	ASSERT_EQ( 0, cMsg.AddObject( "config", cDoc ) );
	JSON cGot;
	ASSERT_EQ( 0, cMsg.FindObject( "config", cGot ) );
	EXPECT_STREQ( "Pyro", cGot.GetString( "name" ).c_str() );
	EXPECT_EQ( 1, cGot.GetInt32( "$.w.x", 0 ) );
	EXPECT_STREQ( cDoc.ToString().c_str(), cGot.ToString().c_str() );

	// a message holding a JSON converts to JSON with it nested
	cMsg.AddInt32( "n", 3 );
	JSON cAll = JSON::FromMessage( cMsg );
	EXPECT_STREQ( "Pyro", cAll.GetString( "config.name" ).c_str() );
	EXPECT_EQ( 5, cAll.GetInt32( "$.config.list[0]", 0 ) );

	uint8 aBuffer[4];
	EXPECT_LT( cDoc.Flatten( aBuffer, sizeof( aBuffer ) ), 0 );
	// a nested handle flattens just its object
	EXPECT_EQ( strlen( "{\"x\":1}" ) + 1, cDoc.GetObject( "w" ).GetFlattenedSize() );
	EXPECT_EQ( JSON::JSON_OBJECT, cDoc.GetValueType() );
}
