/*  os::XML: the Get/Set methods, paths and attributes, XPath, namespaces,
 *  files, Flattenable and os::Message.
 */

#include "test_util.h"

#include <util/xml.h>
#include <util/message.h>

using namespace os;

namespace
{

/* A document built by chaining; the handle returned is a nested element,
 * which keeps the document alive */
XML Build()
{
	XML cDoc( "settings" );
	cDoc.SetString( "name", "Pyro & <friends>" )
	    .SetElement( "window" )
	        .SetRect( "frame", Rect( 10, 20, 410.5f, 320 ) )
	        .SetElement( "view" ).SetString( "@mode", "icons" ).SetInt32( "zoom", 2 ).End()
	        .SetBool( "maximized", false )
	    .End()
	    .AddElement( "recent" ).SetString( "@path", "/a" ).SetInt32( "@line", 1 ).End()
	    .AddElement( "recent" ).SetString( "@path", "/b" ).End()
	    .AddString( "tag", "os" ).AddString( "tag", "desktop" )
	    .SetFloat( "zoom", 0.1f )
	    .SetColor32( "color", Color32_s( 255, 128, 0 ) )
	    .SetColor32( "shade", Color32_s( 1, 2, 3, 4 ) )
	    .SetInt64( "big", 9007199254740993LL )
	    .SetPoint( "window@origin", Point( 3, 4 ) );
	return cDoc.GetElement( "window.view" );
}

const char* const g_pzStore =
	"<store><book id=\"1\"><title>A</title><price>8.95</price></book>"
	"<book id=\"2\"><title>B</title><price>12.99</price></book>"
	"<book id=\"3\"><title>C</title><price>8.99</price><isbn>x</isbn></book></store>";

} // namespace

TEST( XMLTest, ChainedSetsReadBack )
{
	XML cView = Build();
	EXPECT_STREQ( "icons", cView.GetString( "@mode" ).c_str() );
	XML cDoc = cView.Top();
	EXPECT_FALSE( cDoc.HasError() ) << cDoc.GetLastError().c_str();
	EXPECT_STREQ( "settings", cDoc.GetName().c_str() );
	EXPECT_STREQ( "Pyro & <friends>", cDoc.GetString( "name" ).c_str() );
	EXPECT_EQ( 410.5f, cDoc.GetRect( "window.frame", Rect( 0, 0, 0, 0 ) ).right );
	EXPECT_EQ( 4, cDoc.GetPoint( "window@origin", Point() ).y );
	EXPECT_FALSE( cDoc.GetBool( "window.maximized", true ) );
	EXPECT_EQ( 0.1f, cDoc.GetFloat( "zoom", 0 ) );
	EXPECT_STREQ( "#ff8000", cDoc.GetString( "color" ).c_str() );
	EXPECT_EQ( 4, cDoc.GetColor32( "shade", Color32_s() ).alpha );
	EXPECT_EQ( 9007199254740993LL, cDoc.GetInt64( "big", 0 ) );
	EXPECT_EQ( 2, cView.End().GetElement( "view" ).GetInt32( "zoom", 0 ) );
	EXPECT_FALSE( cDoc.End().IsElement() );
}

TEST( XMLTest, ValuesAreWrittenAsText )
{
	String cText = Build().Top().ToString();
	EXPECT_TRUE( Contains( cText, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>" ) );
	EXPECT_TRUE( Contains( cText, "<name>Pyro &amp; &lt;friends&gt;</name>" ) );
	EXPECT_TRUE( Contains( cText, "<frame left=\"10\" top=\"20\" right=\"410.5\" bottom=\"320\"/>" ) );
	EXPECT_TRUE( Contains( cText, "<window origin=\"3 4\">" ) );
	EXPECT_TRUE( Contains( cText, "<zoom>0.1</zoom>" ) );
	EXPECT_TRUE( Contains( cText, "<maximized>false</maximized>" ) );
	EXPECT_TRUE( Contains( cText, "<shade>#01020304</shade>" ) );
}

TEST( XMLTest, RepeatedElementsAndAttributes )
{
	XML cDoc = Build().Top();
	EXPECT_EQ( 2, cDoc.GetCount( "recent" ) );
	EXPECT_EQ( XML::XML_ELEMENT, cDoc.GetType( "recent" ) );
	EXPECT_EQ( XML::XML_ATTRIBUTE, cDoc.GetType( "recent@path" ) );
	EXPECT_EQ( 1, cDoc.GetCount( "recent@line" ) );		// the elements that have it
	EXPECT_STREQ( "/b", cDoc.GetString( "recent@path", "", 1 ).c_str() );
	EXPECT_STREQ( "/b", cDoc.GetElement( "recent", 1 ).GetString( "@path" ).c_str() );
	EXPECT_EQ( 2, cDoc.GetCount( "tag" ) );
	EXPECT_STREQ( "desktop", cDoc.GetString( "tag", "", 1 ).c_str() );
	EXPECT_EQ( 10, cDoc.CountChildren() );
	EXPECT_STREQ( "name", cDoc.GetChildName( 0 ).c_str() );
	XML cWindow = cDoc.GetElement( "window" );
	EXPECT_EQ( 1, cWindow.CountAttributes() );
	EXPECT_STREQ( "origin", cWindow.GetAttributeName( 0 ).c_str() );
}

TEST( XMLTest, AddGoesAfterTheLastOfTheName )
{
	XML cDoc = Build().Top();
	cDoc.AddString( "recent", "x" );
	EXPECT_STREQ( "recent", cDoc.GetChildName( 4 ).c_str() );
	EXPECT_STREQ( "x", cDoc.GetString( "recent", "", 2 ).c_str() );
	EXPECT_STREQ( "tag", cDoc.GetChildName( 5 ).c_str() );
	cDoc.RemoveValue( "recent", 2 );
	EXPECT_EQ( 2, cDoc.GetCount( "recent" ) );
}

TEST( XMLTest, IndexPastTheEndMakesTheElementsBefore )
{
	XML cPad( "p" );
	cPad.SetInt32( "a", 5, 2 );
	EXPECT_EQ( 3, cPad.GetCount( "a" ) );
	EXPECT_STREQ( "", cPad.GetString( "a", "?", 0 ).c_str() );
	EXPECT_EQ( 5, cPad.GetInt32( "a", 0, 2 ) );
}

TEST( XMLTest, ErrorsAreRecordedAndChainsGoOn )
{
	XML cDoc = Build().Top();
	cDoc.SetInt32( "bad name", 1 ).SetInt32( "after", 5 );
	EXPECT_TRUE( cDoc.HasError() );
	EXPECT_EQ( 5, cDoc.GetInt32( "after", 0 ) );
	cDoc.ClearError();
	cDoc.SetString( "//x", "q" );			// a query
	EXPECT_TRUE( cDoc.HasError() );
	cDoc.ClearError();
	cDoc.AddString( "@a", "q" );			// attributes don't repeat
	EXPECT_TRUE( cDoc.HasError() );
	cDoc.ClearError();
	cDoc.SetString( "svg:rect", "q" );		// no such prefix
	EXPECT_TRUE( Contains( cDoc.GetLastError(), "svg:rect" ) );
	cDoc.ClearError();
	cDoc.End().SetInt32( "x", 1 );			// refers to nothing
	EXPECT_TRUE( cDoc.HasError() );
	XML cBadRoot( "1x" );
	EXPECT_TRUE( cBadRoot.HasError() );
	EXPECT_STREQ( "root", cBadRoot.GetName().c_str() );
}

TEST( XMLTest, CloneIsIndependentCopiesShare )
{
	XML cDoc = Build().Top();
	XML cCopy = cDoc.Clone();
	cCopy.SetString( "name", "Other" );
	XML cShare = cDoc;
	cShare.SetString( "shared", "yes" );
	EXPECT_STREQ( "Pyro & <friends>", cDoc.GetString( "name" ).c_str() );
	EXPECT_STREQ( "yes", cDoc.GetString( "shared" ).c_str() );
	XML cWindow = cDoc.GetElement( "window" ).Clone();
	EXPECT_STREQ( "window", cWindow.GetName().c_str() );
	EXPECT_EQ( 2, cWindow.GetInt32( "view.zoom", 0 ) );
}

class XMLFileTest : public FileTest {};

TEST_F( XMLFileTest, SaveMakesFoldersAndLoadReadsBack )
{
	XML cDoc = Build().Top();
	std::string cPath = Path( "sub/dir/doc.xml" );
	ASSERT_EQ( 0, cDoc.Save( cPath.c_str() ) );
	XML cLoaded;
	ASSERT_EQ( 0, cLoaded.Load( cPath.c_str() ) );
	EXPECT_STREQ( cDoc.ToString().c_str(), cLoaded.ToString().c_str() );
	EXPECT_STREQ( "settings", cLoaded.GetName().c_str() );
}

TEST_F( XMLFileTest, BadTextLeavesTheElement )
{
	XML cDoc( "doc" );
	cDoc.SetString( "name", "Pyro" );
	EXPECT_LT( cDoc.FromString( "<a><b></a>" ), 0 );
	EXPECT_TRUE( Contains( cDoc.GetLastError(), "line 1" ) );
	EXPECT_STREQ( "Pyro", cDoc.GetString( "name" ).c_str() );
	EXPECT_LT( cDoc.Load( Path( "none.xml" ).c_str() ), 0 );
	EXPECT_STREQ( "Pyro", cDoc.GetString( "name" ).c_str() );
}

TEST( XMLTest, FromStringOfAnElementReplacesItInPlace )
{
	XML cDoc = Build().Top();
	XML cWindow = cDoc.GetElement( "window" );
	ASSERT_EQ( 0, cWindow.FromString( "<win2 a=\"1\"><c>7</c></win2>" ) );
	EXPECT_STREQ( "win2", cWindow.GetName().c_str() );
	EXPECT_EQ( 7, cWindow.GetInt32( "c", 0 ) );
	EXPECT_EQ( 1, cWindow.GetInt32( "@a", 0 ) );
	EXPECT_EQ( 7, cDoc.GetInt32( "win2.c", 0 ) );
	EXPECT_STREQ( "<win2 a=\"1\"><c>7</c></win2>", cWindow.ToString( false ).c_str() );
}

TEST( XMLTest, RootHandlesStayGoodThroughFromString )
{
	XML cDoc = Build().Top();
	XML cTop = cDoc.Top();
	ASSERT_EQ( 0, cDoc.FromString( "<?xml version=\"1.0\"?><other><x>1</x></other>" ) );
	EXPECT_STREQ( "other", cTop.GetName().c_str() );
	EXPECT_EQ( 1, cTop.GetInt32( "x", 0 ) );
}

TEST( XMLTest, XPathQueries )
{
	XML cStore;
	ASSERT_EQ( 0, cStore.FromString( g_pzStore ) );
	std::vector<XML> cTitles = cStore.Query( "//book[price < 10]/title" );
	ASSERT_EQ( 2U, cTitles.size() );
	EXPECT_STREQ( "A", cTitles[0].AsString().c_str() );
	EXPECT_STREQ( "C", cTitles[1].AsString().c_str() );
	EXPECT_EQ( 8.99, cStore.GetDouble( "//book[last()]/price", 0 ) );
	EXPECT_EQ( 3, cStore.GetCount( "//book" ) );
	EXPECT_STREQ( "2", cStore.GetString( "//book/@id", "", 1 ).c_str() );
	EXPECT_STREQ( "3", cStore.Evaluate( "count(//book)" ).c_str() );
	EXPECT_STREQ( "30.93", cStore.Evaluate( "sum(//price)" ).c_str() );
	EXPECT_STREQ( "B", cStore.QueryFirst( "//book[@id='2']" ).GetString( "title" ).c_str() );
	EXPECT_EQ( XML::XML_ATTRIBUTE, cStore.QueryFirst( "//book/@id" ).GetValueType() );
	EXPECT_EQ( XML::XML_TEXT, cStore.QueryFirst( "//title/text()" ).GetValueType() );
	EXPECT_STREQ( "store", cStore.QueryFirst( "/" ).GetName().c_str() );
	EXPECT_FALSE( cStore.QueryFirst( "//nope" ).Exists() );
}

TEST( XMLTest, RelativeQueriesStartAtTheElement )
{
	XML cStore;
	ASSERT_EQ( 0, cStore.FromString( g_pzStore ) );
	XML cBook = cStore.GetElement( "book", 1 );
	EXPECT_STREQ( "B", cBook.GetString( "./title" ).c_str() );
	EXPECT_STREQ( "A", cBook.GetString( "../book[1]/title" ).c_str() );
}

TEST( XMLTest, InvalidQueriesAreRecorded )
{
	XML cStore;
	ASSERT_EQ( 0, cStore.FromString( g_pzStore ) );
	EXPECT_TRUE( cStore.Query( "//[" ).empty() );
	EXPECT_TRUE( Contains( cStore.GetLastError(), "XPath" ) );
}

TEST( XMLTest, RemoveValueWithQueries )
{
	XML cStore;
	ASSERT_EQ( 0, cStore.FromString( g_pzStore ) );
	cStore.RemoveValue( "//book[isbn]" );
	EXPECT_EQ( 2, cStore.GetCount( "book" ) );
	cStore.RemoveValue( "//@id" );
	EXPECT_EQ( 0, cStore.GetCount( "//@id" ) );
	cStore.RemoveValue( "/store" );		// the root stays
	EXPECT_EQ( 2, cStore.GetCount( "book" ) );
}

TEST( XMLTest, NamespacesInPathsAndQueries )
{
	XML cFeed;
	ASSERT_EQ( 0, cFeed.FromString( "<feed xmlns=\"http://www.w3.org/2005/Atom\" xmlns:x=\"urn:x\">"
	                                "<entry><title>T</title><x:tag>t</x:tag></entry></feed>" ) );
	EXPECT_STREQ( "T", cFeed.GetString( "entry.title" ).c_str() );
	EXPECT_STREQ( "t", cFeed.GetString( "entry.x:tag" ).c_str() );
	EXPECT_EQ( 1, cFeed.GetCount( "//x:tag" ) );
	EXPECT_EQ( 0, cFeed.GetCount( "//entry" ) );		// XPath 1.0 has no default namespace
	cFeed.RegisterNamespace( "a", "http://www.w3.org/2005/Atom" );
	EXPECT_STREQ( "T", cFeed.GetString( "//a:entry/a:title" ).c_str() );
	cFeed.SetString( "entry.x:more", "m" ).SetString( "entry.summary", "s" );
	EXPECT_FALSE( cFeed.HasError() );
	EXPECT_STREQ( "http://www.w3.org/2005/Atom", cFeed.GetElement( "entry.summary" ).GetNamespaceURI().c_str() );
	// an element on its own declares the namespaces it uses
	EXPECT_STREQ( "<entry xmlns=\"http://www.w3.org/2005/Atom\" xmlns:x=\"urn:x\"><title>T</title><x:tag>t</x:tag>"
	              "<x:more>m</x:more><summary>s</summary></entry>",
	              cFeed.GetElement( "entry" ).ToString( false ).c_str() );
}

TEST( XMLTest, DeclareNamespace )
{
	XML cSvg( "svg" );
	cSvg.DeclareNamespace( "", "http://www.w3.org/2000/svg" ).DeclareNamespace( "xlink", "http://www.w3.org/1999/xlink" );
	cSvg.SetRect( "rect", Rect( 0, 0, 5, 5 ) ).SetString( "use@xlink:href", "#r" ).SetString( "@xml:lang", "en" );
	EXPECT_FALSE( cSvg.HasError() ) << cSvg.GetLastError().c_str();
	String cText = cSvg.ToString();
	EXPECT_TRUE( Contains( cText, "<svg xmlns=\"http://www.w3.org/2000/svg\" xmlns:xlink=\"http://www.w3.org/1999/xlink\" xml:lang=\"en\">" ) );
	EXPECT_TRUE( Contains( cText, "<use xlink:href=\"#r\"/>" ) );
	EXPECT_STREQ( "http://www.w3.org/2000/svg", cSvg.GetElement( "rect" ).GetNamespaceURI().c_str() );
}

TEST( XMLTest, TextCommentsAndNames )
{
	XML cMisc( "m" );
	cMisc.AddComment( " gone " ).SetText( "hello" );
	EXPECT_STREQ( "hello", cMisc.AsString().c_str() );
	EXPECT_FALSE( Contains( cMisc.ToString(), "gone" ) );
	cMisc.AddComment( " a comment " ).SetName( "renamed" );
	EXPECT_TRUE( Contains( cMisc.ToString(), "<renamed>hello<!-- a comment --></renamed>" ) );
	cMisc.AddComment( "bad -- comment" );
	EXPECT_TRUE( cMisc.HasError() );
}

TEST( XMLTest, FlattensIntoAMessageAsItIs )
{
	XML cDoc = Build().Top();
	EXPECT_EQ( (int)T_XML, cDoc.GetType() );
	std::vector<uint8> cBuffer( cDoc.GetFlattenedSize() );
	ASSERT_EQ( 0, cDoc.Flatten( &cBuffer[0], cBuffer.size() ) );
	uint8 aSmall[3];
	EXPECT_LT( cDoc.Flatten( aSmall, sizeof( aSmall ) ), 0 );
	XML cUnflat;
	ASSERT_EQ( 0, cUnflat.Unflatten( &cBuffer[0] ) );
	EXPECT_STREQ( "desktop", cUnflat.GetString( "tag", "", 1 ).c_str() );

	Message cMsg;
	ASSERT_EQ( 0, cMsg.AddObject( "config", cDoc.GetElement( "window" ) ) );
	XML cGot;
	ASSERT_EQ( 0, cMsg.FindObject( "config", cGot ) );
	EXPECT_STREQ( "window", cGot.GetName().c_str() );
	EXPECT_EQ( 2, cGot.GetInt32( "view.zoom", 0 ) );
}

TEST( XMLTest, MessageToXMLAndBack )
{
	Message cMsg( 7 ), cChild;
	cMsg.AddString( "name", "Pyro" );
	cMsg.AddInt32( "count", 3 );
	cMsg.AddInt32( "count", 4 );
	cMsg.AddInt64( "big", 5000000000LL );
	cMsg.AddBool( "on", true );
	cMsg.AddDouble( "ratio", 0.5 );
	cMsg.AddFloat( "f", 0.1f );
	cMsg.AddRect( "frame", Rect( 1, 2, 3, 4 ) );
	cMsg.AddPoint( "pos", Point( 5, 6 ) );
	cMsg.AddColor32( "color", Color32_s( 1, 2, 3 ) );
	cMsg.AddString( "bad name", "x" );
	cChild.AddString( "path", "/c" );
	cChild.AddInt32( "line", 9 );
	cMsg.AddMessage( "child", &cChild );
	cMsg.AddObject( "xml", Build().Top().GetElement( "window" ) );

	XML cDoc = XML::FromMessage( cMsg, "msg" );
	EXPECT_TRUE( cDoc.HasError() );			// "bad name" is left out
	EXPECT_STREQ( "msg", cDoc.GetName().c_str() );
	EXPECT_EQ( 2, cDoc.GetCount( "count" ) );
	EXPECT_EQ( 4, cDoc.GetInt32( "count", 0, 1 ) );
	EXPECT_STREQ( "/c", cDoc.GetString( "child.path" ).c_str() );
	EXPECT_STREQ( "0.1", cDoc.GetString( "f" ).c_str() );
	EXPECT_EQ( 2, cDoc.GetInt32( "xml.view.zoom", 0 ) );
	EXPECT_STREQ( "icons", cDoc.GetString( "xml.view@mode" ).c_str() );

	Message cBack;
	ASSERT_EQ( 0, cDoc.ToMessage( &cBack ) );
	int32 n = 0;
	int64 n64 = 0;
	bool b = false;
	double v = 0;
	const char* pz = NULL;
	Rect r;
	Point p;
	Message cGot;
	EXPECT_EQ( 0, cBack.FindInt32( "count", &n, 1 ) );
	EXPECT_EQ( 4, n );
	EXPECT_EQ( 0, cBack.FindInt64( "big", &n64 ) );
	EXPECT_EQ( 5000000000LL, n64 );
	EXPECT_EQ( 0, cBack.FindBool( "on", &b ) );
	EXPECT_TRUE( b );
	EXPECT_EQ( 0, cBack.FindDouble( "ratio", &v ) );
	EXPECT_EQ( 0.5, v );
	EXPECT_EQ( 0, cBack.FindString( "name", &pz ) );
	EXPECT_STREQ( "Pyro", pz );
	EXPECT_EQ( 0, cBack.FindRect( "frame", &r ) );
	EXPECT_EQ( 4, r.bottom );
	EXPECT_EQ( 0, cBack.FindPoint( "pos", &p ) );
	EXPECT_EQ( 5, p.x );
	EXPECT_EQ( 0, cBack.FindString( "color", &pz ) );
	EXPECT_STREQ( "#010203", pz );
	ASSERT_EQ( 0, cBack.FindMessage( "child", &cGot ) );
	EXPECT_EQ( 0, cGot.FindInt32( "line", &n ) );
	EXPECT_EQ( 9, n );
	EXPECT_EQ( 0, cBack.FindMessage( "xml", &cGot ) );
}

TEST( XMLTest, TheDocumentsOwnDTD )
{
	XML cNote;
	ASSERT_EQ( 0, cNote.FromString( "<!DOCTYPE note [<!ELEMENT note (to,body)><!ELEMENT to (#PCDATA)><!ELEMENT body (#PCDATA)>]>"
	                                "<note><to>a</to><body>b</body></note>" ) );
	EXPECT_FALSE( cNote.HasSchema() );
	EXPECT_TRUE( cNote.IsValid() );
	cNote.RemoveValue( "to" );
	EXPECT_FALSE( cNote.IsValid() );
	ASSERT_FALSE( cNote.GetValidationErrors().empty() );
	EXPECT_TRUE( Contains( cNote.GetValidationErrors()[0], "/note" ) );
	EXPECT_TRUE( Contains( cNote.ToString(), "<!DOCTYPE note [" ) );
	XML cPlain;
	EXPECT_TRUE( cPlain.IsValid() );
}

TEST_F( XMLFileTest, ExternalEntitiesAreNotRead )
{
	Write( "secret.txt", "SECRET" );
	XML cDoc;
	cDoc.FromString( ( "<!DOCTYPE a [<!ENTITY e SYSTEM \"file://" + Path( "secret.txt" ) + "\">]><a>&e;</a>" ).c_str() );
	EXPECT_FALSE( Contains( cDoc.ToString(), "SECRET" ) );
	EXPECT_FALSE( Contains( cDoc.AsString(), "SECRET" ) );
}
