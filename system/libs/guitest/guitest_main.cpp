/*  libguitest's main(): Google Test's flags, then the tests in an
 *  os::Application (guitest::RunAllTests()).  In an object file of its own,
 *  so a program with a main() of its own gets that one instead. */

#include "guitest.h"

int main( int argc, char** argv )
{
	::testing::InitGoogleTest( &argc, argv );
	return guitest::RunAllTests();
}
