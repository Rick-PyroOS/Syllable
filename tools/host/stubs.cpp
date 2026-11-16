/*
 * storage/fsnode.cpp and storage/file.cpp have constructors taking an
 * os::FileReference, which live in filereference.cpp and drag in Directory,
 * Window, Messenger and the rest of the GUI.  The host tools never build a
 * FileReference, so these are never reached.
 */

#include <stdio.h>
#include <stdlib.h>
#include <storage/filereference.h>

using namespace os;

static void not_on_host( const char* pzWhat )
{
	fprintf( stderr, "%s is not available in the Syllable host tools\n", pzWhat );
	abort();
}

String FileReference::GetName() const
{
	not_on_host( "os::FileReference" );
	return( String() );
}

const Directory& FileReference::GetDirectory() const
{
	not_on_host( "os::FileReference" );
	return( *(Directory*)NULL );
}

bool FileReference::IsValid() const
{
	not_on_host( "os::FileReference" );
	return( false );
}
