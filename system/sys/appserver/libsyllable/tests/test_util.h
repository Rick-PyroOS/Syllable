/*  Helpers for libsyllable's unit tests (Google Test).
 */

#ifndef LIBSYLLABLE_TESTS_TEST_UTIL_H
#define LIBSYLLABLE_TESTS_TEST_UTIL_H

#include <gtest/gtest.h>
#include <util/string.h>

#include <string>
#include <vector>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

/* Whether a String has a text in it (for EXPECT_TRUE, which prints the
 * String on failure through the message) */
inline ::testing::AssertionResult Contains( const os::String& cText, const char* pzPart )
{
	if( strstr( cText.c_str(), pzPart ) != NULL )
		return ::testing::AssertionSuccess();
	return ::testing::AssertionFailure() << "\"" << cText.c_str() << "\" has no \"" << pzPart << "\"";
}

/* A test with a folder of its own for files, removed afterwards */
class FileTest : public ::testing::Test
{
protected:
	virtual void SetUp()
	{
		char zTemplate[] = "/tmp/libsyllable-test-XXXXXX";
		const char* pzDir = mkdtemp( zTemplate );
		ASSERT_TRUE( pzDir != NULL );
		m_cDir = pzDir;
	}
	virtual void TearDown()
	{
		if( !m_cDir.empty() && system( ( "rm -rf '" + m_cDir + "'" ).c_str() ) != 0 )
			fprintf( stderr, "can't remove %s\n", m_cDir.c_str() );
	}
	std::string Path( const char* pzName ) const { return m_cDir + "/" + pzName; }
	void Write( const char* pzName, const char* pzText ) const
	{
		FILE* hFile = fopen( Path( pzName ).c_str(), "w" );
		ASSERT_TRUE( hFile != NULL );
		fputs( pzText, hFile );
		fclose( hFile );
	}

	std::string m_cDir;
};

#endif
