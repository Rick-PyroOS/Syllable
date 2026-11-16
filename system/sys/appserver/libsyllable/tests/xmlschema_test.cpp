/*  os::XML's validation: W3C XML Schema, Relax-NG and DTDs.
 */

#include "test_util.h"

#include <util/xml.h>

using namespace os;

namespace
{

const char* const g_pzXSD =
	"<xs:schema xmlns:xs=\"http://www.w3.org/2001/XMLSchema\">"
	" <xs:element name=\"settings\"><xs:complexType><xs:sequence>"
	"  <xs:element name=\"name\" type=\"xs:string\"/>"
	"  <xs:element name=\"window\"><xs:complexType><xs:sequence>"
	"   <xs:element name=\"width\"><xs:simpleType><xs:restriction base=\"xs:int\"><xs:minInclusive value=\"0\"/></xs:restriction></xs:simpleType></xs:element>"
	"  </xs:sequence><xs:attribute name=\"id\" type=\"xs:ID\"/></xs:complexType></xs:element>"
	" </xs:sequence></xs:complexType></xs:element>"
	"</xs:schema>";

const char* const g_pzRelaxNG =
	"<element name=\"note\" xmlns=\"http://relaxng.org/ns/structure/1.0\">"
	" <element name=\"to\"><text/></element>"
	" <optional><attribute name=\"lang\"><choice><value>en</value><value>de</value></choice></attribute></optional>"
	"</element>";

const char* const g_pzDTD =
	"<!ELEMENT list (item*)>\n"
	"<!ELEMENT item (#PCDATA)>\n"
	"<!ATTLIST item n CDATA #REQUIRED>";

} // namespace

class XMLSchemaTest : public FileTest {};

TEST_F( XMLSchemaTest, NoSchemaIsValid )
{
	XML cNone;
	EXPECT_FALSE( cNone.HasSchema() );
	EXPECT_TRUE( cNone.IsValid() );
}

TEST_F( XMLSchemaTest, XMLSchemaFromAFile )
{
	Write( "settings.xsd", g_pzXSD );
	XML cDoc( File( Path( "settings.xsd" ).c_str() ) );
	ASSERT_TRUE( cDoc.HasSchema() );
	EXPECT_FALSE( cDoc.HasError() ) << cDoc.GetLastError().c_str();
	ASSERT_EQ( 0, cDoc.FromString( "<settings><name>Pyro</name><window id=\"w\"><width>3</width></window></settings>" ) );
	EXPECT_TRUE( cDoc.IsValid() );
	EXPECT_TRUE( cDoc.GetValidationErrors().empty() );

	cDoc.SetInt32( "window.width", -1 );
	EXPECT_FALSE( cDoc.IsValid() );
	ASSERT_FALSE( cDoc.GetValidationErrors().empty() );
	EXPECT_TRUE( Contains( cDoc.GetValidationErrors()[0], "/settings/window/width" ) );

	cDoc.SetInt32( "window.width", 5 ).SetString( "extra", "x" );
	EXPECT_FALSE( cDoc.IsValid() );
	cDoc.RemoveValue( "extra" );
	EXPECT_TRUE( cDoc.IsValid() );
}

TEST_F( XMLSchemaTest, CloneKeepsTheSchemaAndTheWholeDocumentIsChecked )
{
	XML cDoc;
	ASSERT_EQ( 0, cDoc.SetSchemaString( g_pzXSD ) );
	ASSERT_EQ( 0, cDoc.FromString( "<settings><name>Pyro</name><window><width>3</width></window></settings>" ) );
	XML cCopy = cDoc.Clone();
	EXPECT_TRUE( cCopy.HasSchema() );
	EXPECT_TRUE( cCopy.IsValid() );
	cCopy.SetString( "window.width", "wide" );
	EXPECT_FALSE( cCopy.IsValid() );
	EXPECT_FALSE( cCopy.GetElement( "window" ).IsValid() );
	EXPECT_TRUE( cDoc.IsValid() );
	cDoc.ClearSchema();
	EXPECT_FALSE( cDoc.HasSchema() );
	EXPECT_TRUE( cDoc.IsValid() );
}

TEST_F( XMLSchemaTest, RelaxNGFromAnXML )
{
	XML cSchema;
	ASSERT_EQ( 0, cSchema.FromString( g_pzRelaxNG ) );
	XML cNote( "note" );
	ASSERT_EQ( 0, cNote.SetSchema( cSchema ) );
	EXPECT_FALSE( cNote.IsValid() );
	cNote.SetString( "to", "you" );
	EXPECT_TRUE( cNote.IsValid() );
	cNote.SetString( "@lang", "fr" );
	EXPECT_FALSE( cNote.IsValid() );
	ASSERT_FALSE( cNote.GetValidationErrors().empty() );
	EXPECT_TRUE( Contains( cNote.GetValidationErrors()[0], "lang" ) );
}

TEST_F( XMLSchemaTest, ADTDAsTheSchema )
{
	XML cList( "list" );
	ASSERT_EQ( 0, cList.SetSchemaString( g_pzDTD ) );
	EXPECT_TRUE( cList.IsValid() );
	cList.AddString( "item", "a" );
	EXPECT_FALSE( cList.IsValid() );
	ASSERT_FALSE( cList.GetValidationErrors().empty() );
	EXPECT_TRUE( Contains( cList.GetValidationErrors()[0], "/list/item" ) );
	cList.SetInt32( "item@n", 1 );
	EXPECT_TRUE( cList.IsValid() );
}

TEST_F( XMLSchemaTest, BadSchemasLeaveTheOneThereWas )
{
	XML cList( "list" );
	ASSERT_EQ( 0, cList.SetSchemaString( g_pzDTD ) );
	EXPECT_LT( cList.SetSchemaString( "<a><b></a>" ), 0 );
	EXPECT_TRUE( Contains( cList.GetLastError(), "not well-formed" ) );
	EXPECT_LT( cList.SetSchemaString( "<notaschema/>" ), 0 );
	EXPECT_LT( cList.SetSchemaString( "<xs:schema xmlns:xs=\"http://www.w3.org/2001/XMLSchema\">"
	                                  "<xs:element name=\"a\" type=\"nope\"/></xs:schema>" ), 0 );
	EXPECT_TRUE( Contains( cList.GetLastError(), "XML Schema" ) );
	EXPECT_TRUE( cList.HasSchema() );
	Write( "bad.xsd", "<xs:schema" );
	EXPECT_LT( cList.SetSchema( File( Path( "bad.xsd" ).c_str() ) ), 0 );
	EXPECT_TRUE( cList.HasSchema() );
	// a file that isn't there: os::File throws
	EXPECT_ANY_THROW( File( Path( "none.xsd" ).c_str() ) );
}

TEST_F( XMLSchemaTest, IncludesAreNotFollowed )
{
	Write( "inc.xsd", "<xs:schema xmlns:xs=\"http://www.w3.org/2001/XMLSchema\"><xs:element name=\"a\"/></xs:schema>" );
	std::string cMain = "<xs:schema xmlns:xs=\"http://www.w3.org/2001/XMLSchema\"><xs:include schemaLocation=\"" +
	                    Path( "inc.xsd" ) + "\"/></xs:schema>";
	XML cDoc;
	int nResult = cDoc.SetSchemaString( cMain.c_str() );
	cDoc.FromString( "<a/>" );
	EXPECT_TRUE( nResult < 0 || !cDoc.IsValid() );
}
