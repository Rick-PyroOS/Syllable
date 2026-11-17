/*  os::JSON's JSONPath (RFC 9535) queries.
 */

#include "test_util.h"

#include <util/json.h>

using namespace os;

namespace
{

/* The store document of RFC 9535, and a few more values */
class JSONPathTest : public ::testing::Test
{
protected:
	virtual void SetUp()
	{
		ASSERT_EQ( 0, d.FromString(
	"{ \"store\": {"
	"  \"book\": ["
	"    { \"category\": \"reference\", \"author\": \"Nigel Rees\", \"title\": \"Sayings of the Century\", \"price\": 8.95 },"
	"    { \"category\": \"fiction\", \"author\": \"Evelyn Waugh\", \"title\": \"Sword of Honour\", \"price\": 12.99 },"
	"    { \"category\": \"fiction\", \"author\": \"Herman Melville\", \"title\": \"Moby Dick\", \"isbn\": \"0-553-21311-3\", \"price\": 8.99 },"
	"    { \"category\": \"fiction\", \"author\": \"J. R. R. Tolkien\", \"title\": \"The Lord of the Rings\", \"isbn\": \"0-395-19395-8\", \"price\": 22.99 }"
	"  ],"
	"  \"bicycle\": { \"color\": \"red\", \"price\": 399 }"
	"}, \"a\": [3, 5, 1, 2, 4, 6], \"o\": {\"j\": 1, \"k\": 2}, \"n\": null, \"$schema\": \"s\", \"we ird\": 7 }" ) );
	}

	/* The matches as compact text, comma-separated: 'strings', {objects}
	 * by their title or color, [] for arrays */
	std::string Q( const char* pzQuery )
	{
		std::vector<JSON> c = d.Query( pzQuery );
		std::string s;
		for( size_t i = 0; i < c.size(); i++ )
		{
			if( i ) s += ",";
			JSON::Type t = c[i].GetValueType();
			if( t == JSON::JSON_STRING ) s += std::string( "'" ) + c[i].AsString().c_str() + "'";
			else if( t == JSON::JSON_OBJECT ) s += "{" + std::string( c[i].GetString( "title", c[i].GetString( "color", "?" ).c_str() ).c_str() ) + "}";
			else if( t == JSON::JSON_ARRAY ) s += "[]";
			else if( t == JSON::JSON_NULL ) s += "null";
			else s += c[i].AsString().c_str();
		}
		return s;
	}

	JSON d;
};

} // namespace

#define Q( path, expect ) EXPECT_EQ( std::string( expect ), Q( path ) ) << "query: " << path

TEST_F( JSONPathTest, Rfc9535Examples )
{
	Q( "$.store.book[*].author", "'Nigel Rees','Evelyn Waugh','Herman Melville','J. R. R. Tolkien'" );
	Q( "$..author", "'Nigel Rees','Evelyn Waugh','Herman Melville','J. R. R. Tolkien'" );
	Q( "$.store.*", "[],{red}" );
	Q( "$.store..price", "8.95,12.99,8.99,22.99,399" );
	Q( "$..book[2]", "{Moby Dick}" );
	Q( "$..book[2].author", "'Herman Melville'" );
	Q( "$..book[2].publisher", "" );
	Q( "$..book[-1]", "{The Lord of the Rings}" );
	Q( "$..book[0,1]", "{Sayings of the Century},{Sword of Honour}" );
	Q( "$..book[:2]", "{Sayings of the Century},{Sword of Honour}" );
	Q( "$..book[?@.isbn]", "{Moby Dick},{The Lord of the Rings}" );
	Q( "$..book[?@.price<10]", "{Sayings of the Century},{Moby Dick}" );
	Q( "$..book[?(@.price < 10)].title", "'Sayings of the Century','Moby Dick'" );
}

TEST_F( JSONPathTest, Slices )
{
	Q( "$.a[1:3]", "5,1" );
	Q( "$.a[5:1:-2]", "6,2" );
	Q( "$.a[::-1]", "6,4,2,1,5,3" );
	Q( "$.a[-2:]", "4,6" );
	Q( "$.a[0:6:0]", "" );
}

TEST_F( JSONPathTest, BracketsQuotesUnions )
{
	Q( "$['store']['bicycle']['color']", "'red'" );
	Q( "$[\"we ird\"]", "7" );
	Q( "$['$schema']", "'s'" );
	Q( "$.o[*]", "1,2" );
	Q( "$.o['k','j']", "2,1" );
	Q( "$.n", "null" );
}

TEST_F( JSONPathTest, Filters )
{
	Q( "$.store.book[?@.category == 'fiction' && @.price > 10].title", "'Sword of Honour','The Lord of the Rings'" );
	Q( "$.store.book[?!@.isbn].title", "'Sayings of the Century','Sword of Honour'" );
	Q( "$.store.book[?@.price == 8.95 || @.author == \"J. R. R. Tolkien\"].price", "8.95,22.99" );
	Q( "$.store.book[?@.price < $.store.bicycle.price && @.price >= 12.99].price", "12.99,22.99" );
	Q( "$.a[?@ > 3]", "5,4,6" );
	Q( "$.store.book[?length(@.author) == 10].author", "'Nigel Rees'" );
	Q( "$.store[?count(@.*) == 2].color", "'red'" );
	Q( "$.store[?count(@.*) == 2]", "{red}" );
	Q( "$.store.book[?match(@.author, 'H.*')].author", "'Herman Melville'" );
	Q( "$.store.book[?search(@.title, 'of')].title", "'Sayings of the Century','Sword of Honour','The Lord of the Rings'" );
	Q( "$.store.book[?search(@.title, '^S')].title", "'Sayings of the Century','Sword of Honour'" );
	Q( "$.store.book[?value(@.price) > 20].title", "'The Lord of the Rings'" );
	Q( "$.store.book[?@.missing == @.missing2].title", "'Sayings of the Century','Sword of Honour','Moby Dick','The Lord of the Rings'" );
	Q( "$.store.book[?@.price != 8.95].price", "12.99,8.99,22.99" );
}

TEST_F( JSONPathTest, InvalidQueriesSelectNothingAndAreRecorded )
{
	EXPECT_TRUE( d.Query( "$.store.book[" ).empty() );
	EXPECT_TRUE( d.HasError() );
	d.ClearError();
	EXPECT_TRUE( d.Query( "store" ).empty() );
	EXPECT_TRUE( d.HasError() );
	d.ClearError();
	EXPECT_TRUE( d.Query( "$[?@.a ==]" ).empty() );
	EXPECT_TRUE( d.HasError() );
}

TEST_F( JSONPathTest, GetMethodsTakeQueries )
{
	EXPECT_EQ( 22.99, d.GetDouble( "$..book[-1].price", 0 ) );
	EXPECT_STREQ( "Herman Melville", d.GetString( "$..author", "", 2 ).c_str() );
	EXPECT_EQ( 5, d.GetCount( "$..price" ) );
	EXPECT_STREQ( "red", d.GetObject( "$.store.bicycle" ).GetString( "color" ).c_str() );
	EXPECT_TRUE( d.HasValue( "$..isbn" ) );
	EXPECT_FALSE( d.HasValue( "$..nope" ) );
	EXPECT_STREQ( "s", d.GetString( "$schema" ).c_str() );	// a name, not a query
	EXPECT_STREQ( "j", d.QueryFirst( "$.o.*" ).GetName().c_str() );
	EXPECT_FALSE( d.QueryFirst( "$.zzz" ).Exists() );
}

TEST_F( JSONPathTest, SetRefusesQueries )
{
	d.SetString( "$.store.x", "no" );
	EXPECT_TRUE( d.HasError() );
	EXPECT_FALSE( d.HasValue( "store.x" ) );
}

TEST_F( JSONPathTest, QueryFromANestedObject )
{
	JSON s = d.GetObject( "store" );
	EXPECT_EQ( 1U, s.Query( "$.bicycle.color" ).size() );
	EXPECT_TRUE( s.Query( "$.store" ).empty() );
}

TEST_F( JSONPathTest, RemoveValueRemovesEveryMatch )
{
	d.RemoveValue( "$..isbn" );
	EXPECT_EQ( 0, d.GetCount( "$..isbn" ) );
	EXPECT_EQ( 4, d.GetCount( "$.store.book[*]" ) );
	EXPECT_EQ( 1, d.GetCount( "$.store.book" ) );
	d.RemoveValue( "$..book[?@.price > 10]" );
	Q( "$..book[*].title", "'Sayings of the Century','Moby Dick'" );
	d.RemoveValue( "$..*" );	// nested matches too
	EXPECT_EQ( 0, d.CountMembers() );
}
