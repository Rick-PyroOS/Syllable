/*
 * The Mesa demos open their data files relative to the working directory
 * ("isosurf.dat", "../images/reflect.rgb"), so each demo runs in its own
 * folder: its main() is compiled as mesademo_main() (CMakeLists.txt), and
 * this main() changes to the program's folder first.
 */
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int mesademo_main( int argc, char** argv );

int main( int argc, char** argv )
{
	if( argc > 0 && argv[0] != NULL && strchr( argv[0], '/' ) != NULL )
	{
		char* dir = strdup( argv[0] );
		char* slash = strrchr( dir, '/' );
		if( slash == dir )
			slash[1] = '\0';
		else
			*slash = '\0';
		if( chdir( dir ) != 0 )
			;	/* stay where we are */
		free( dir );
	}
	return mesademo_main( argc, argv );
}
