/*  os::JSON's JSON Schema validation (2020-12 and draft-07 keywords).
 */

#include "test_util.h"

#include <util/json.h>

using namespace os;

namespace
{

/* Whether pzDoc matches pzSchema (as the value "v" of an object) */
::testing::AssertionResult Valid( const char* pzSchema, const char* pzDoc )
{
	JSON cDoc;
	cDoc.FromString( std::string( "{\"v\":" ).append( pzDoc ).append( "}" ).c_str() );
	JSON cWrap;
	cWrap.FromString( std::string( "{\"type\":\"object\",\"properties\":{\"v\":" ).append( pzSchema ).append( "}}" ).c_str() );
	cDoc.SetSchema( cWrap );
	if( cDoc.IsValid() )
		return ::testing::AssertionSuccess() << pzDoc << " matches " << pzSchema;
	return ::testing::AssertionFailure() << pzDoc << " doesn't match " << pzSchema;
}

} // namespace

#define V( s, d ) EXPECT_TRUE( Valid( s, d ) )
#define NV( s, d ) EXPECT_FALSE( Valid( s, d ) )

TEST( JSONSchemaTest, NoSchemaIsValid )
{
	JSON cNone;
	EXPECT_FALSE( cNone.HasSchema() );
	EXPECT_TRUE( cNone.IsValid() );
}

TEST( JSONSchemaTest, Types )
{
	V( "{\"type\":\"integer\"}", "3" );
	NV( "{\"type\":\"integer\"}", "3.5" );
	V( "{\"type\":\"number\"}", "3.5" );
	V( "{\"type\":[\"string\",\"null\"]}", "null" );
	NV( "{\"type\":[\"string\",\"null\"]}", "1" );
	V( "true", "1" );
	NV( "false", "1" );
	V( "{\"enum\":[1,\"a\",[1,2]]}", "[1,2]" );
	NV( "{\"enum\":[1,\"a\"]}", "2" );
	V( "{\"enum\":[1]}", "1.0" );
	V( "{\"const\":{\"a\":1}}", "{\"a\":1}" );
	NV( "{\"const\":{\"a\":1}}", "{\"a\":2}" );
}

TEST( JSONSchemaTest, Numbers )
{
	V( "{\"minimum\":1,\"maximum\":3}", "3" );
	NV( "{\"minimum\":1}", "0" );
	NV( "{\"exclusiveMaximum\":3}", "3" );
	NV( "{\"maximum\":3,\"exclusiveMaximum\":true}", "3" );
	V( "{\"multipleOf\":0.1}", "0.3" );
	NV( "{\"multipleOf\":2}", "3" );
}

TEST( JSONSchemaTest, Strings )
{
	V( "{\"minLength\":2,\"maxLength\":3}", "\"\xc3\xa9t\xc3\xa9\"" );
	NV( "{\"maxLength\":2}", "\"abc\"" );
	V( "{\"pattern\":\"^\\\\d{3}-\\\\d{4}$\"}", "\"555-1234\"" );
	NV( "{\"pattern\":\"^\\\\d+$\"}", "\"12a\"" );
}

TEST( JSONSchemaTest, Arrays )
{
	V( "{\"items\":{\"type\":\"integer\"},\"minItems\":2}", "[1,2]" );
	NV( "{\"items\":{\"type\":\"integer\"}}", "[1,\"x\"]" );
	V( "{\"prefixItems\":[{\"type\":\"string\"}],\"items\":{\"type\":\"integer\"}}", "[\"a\",1,2]" );
	NV( "{\"prefixItems\":[{\"type\":\"string\"}],\"items\":false}", "[\"a\",1]" );
	NV( "{\"items\":[{\"type\":\"string\"}],\"additionalItems\":false}", "[\"a\",1]" );
	NV( "{\"uniqueItems\":true}", "[1,{\"a\":1},{\"a\":1}]" );
	V( "{\"uniqueItems\":true}", "[1,2]" );
	V( "{\"contains\":{\"const\":5}}", "[1,5]" );
	NV( "{\"contains\":{\"const\":5}}", "[1,2]" );
	NV( "{\"contains\":{\"const\":5},\"maxContains\":1}", "[5,5]" );
}

TEST( JSONSchemaTest, Objects )
{
	V( "{\"required\":[\"a\"],\"properties\":{\"a\":{\"type\":\"string\"}}}", "{\"a\":\"x\"}" );
	NV( "{\"required\":[\"a\"]}", "{}" );
	NV( "{\"properties\":{\"a\":true},\"additionalProperties\":false}", "{\"a\":1,\"b\":2}" );
	V( "{\"patternProperties\":{\"^x_\":{\"type\":\"integer\"}},\"additionalProperties\":false}", "{\"x_1\":1}" );
	NV( "{\"patternProperties\":{\"^x_\":{\"type\":\"integer\"}}}", "{\"x_1\":\"s\"}" );
	NV( "{\"propertyNames\":{\"maxLength\":2}}", "{\"abc\":1}" );
	NV( "{\"dependentRequired\":{\"a\":[\"b\"]}}", "{\"a\":1}" );
	V( "{\"dependencies\":{\"a\":[\"b\"]}}", "{\"a\":1,\"b\":2}" );
	NV( "{\"maxProperties\":1}", "{\"a\":1,\"b\":2}" );
}

TEST( JSONSchemaTest, Combinators )
{
	V( "{\"anyOf\":[{\"type\":\"string\"},{\"type\":\"integer\"}]}", "1" );
	NV( "{\"anyOf\":[{\"type\":\"string\"}]}", "1" );
	NV( "{\"oneOf\":[{\"type\":\"integer\"},{\"minimum\":0}]}", "1" );
	V( "{\"oneOf\":[{\"type\":\"integer\"},{\"minimum\":0}]}", "1.5" );
	NV( "{\"not\":{\"type\":\"null\"}}", "null" );
	NV( "{\"allOf\":[{\"minimum\":0},{\"maximum\":1}]}", "2" );
	V( "{\"if\":{\"type\":\"string\"},\"then\":{\"minLength\":2},\"else\":{\"minimum\":5}}", "\"ab\"" );
	NV( "{\"if\":{\"type\":\"string\"},\"then\":{\"minLength\":2},\"else\":{\"minimum\":5}}", "3" );
}

class JSONSchemaFileTest : public FileTest {};

TEST_F( JSONSchemaFileTest, SchemaFileWithRefsAndErrors )
{
	Write( "settings.schema.json",
	       "{ \"$schema\": \"https://json-schema.org/draft/2020-12/schema\",\n"
	       "  \"type\": \"object\", \"required\": [ \"name\", \"window\" ],\n"
	       "  \"properties\": {\n"
	       "    \"name\": { \"type\": \"string\", \"minLength\": 1 },\n"
	       "    \"window\": { \"$ref\": \"#/$defs/window\" },\n"
	       "    \"recent\": { \"type\": \"array\", \"items\": { \"$ref\": \"#/$defs/file\" } } },\n"
	       "  \"additionalProperties\": false,\n"
	       "  \"$defs\": {\n"
	       "    \"window\": { \"type\": \"object\", \"properties\": { \"width\": { \"type\": \"integer\", \"minimum\": 0 } } },\n"
	       "    \"file\": { \"$anchor\": \"file\", \"type\": \"object\", \"required\": [ \"path\" ] } } }\n" );
	JSON cDoc( File( Path( "settings.schema.json" ).c_str() ) );
	ASSERT_TRUE( cDoc.HasSchema() );
	EXPECT_FALSE( cDoc.HasError() );
	cDoc.SetString( "name", "Pyro" ).SetInt32( "window.width", 640 ).AddObject( "recent" ).SetString( "path", "/a" );
	EXPECT_TRUE( cDoc.IsValid() );
	cDoc.SetInt32( "window.width", -1 ).AddObject( "recent" ).End().SetBool( "extra", true ).SetString( "name", "" );
	EXPECT_FALSE( cDoc.IsValid() );
	std::vector<String> cErrors = cDoc.GetValidationErrors();
	EXPECT_EQ( 4U, cErrors.size() );
	bool bWidth = false;
	for( size_t i = 0; i < cErrors.size(); i++ )
		if( Contains( cErrors[i], "$.window.width" ) )
			bWidth = true;
	EXPECT_TRUE( bWidth );
	// a nested handle validates the whole document
	EXPECT_FALSE( cDoc.GetObject( "window" ).IsValid() );
	cDoc.ClearSchema();
	EXPECT_TRUE( cDoc.IsValid() );
}

TEST_F( JSONSchemaFileTest, BadSchemaFileIsRecorded )
{
	Write( "bad.schema.json", "{ \"type\": " );
	JSON cBad( File( Path( "bad.schema.json" ).c_str() ) );
	EXPECT_FALSE( cBad.HasSchema() );
	EXPECT_TRUE( cBad.HasError() );
	// a file that isn't there: os::File throws
	EXPECT_ANY_THROW( File( Path( "none.json" ).c_str() ) );
}

TEST( JSONSchemaTest, RefLoopsEnd )
{
	JSON cLoop, cLoopSchema;
	cLoopSchema.FromString( "{\"$ref\":\"#\"}" );
	cLoop.SetSchema( cLoopSchema );
	EXPECT_FALSE( cLoop.IsValid() );
}
