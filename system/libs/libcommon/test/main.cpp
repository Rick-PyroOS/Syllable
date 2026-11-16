#include <common/debug.h>

#include "application.h"


int main( int argc, char *argv[] )
{
	FN_START();
	
	App* pcApp = new App();
	pcApp->Run();
	
	FN_END();
	return( 0 );
}

