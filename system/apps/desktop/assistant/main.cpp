#include "application.h"
#include <signal.h>
#include "common.h"
App *g_pcApp;

int main( int argc, char *argv[] )
{

	mkdir(
			GetSettingsPath().c_str(),
			700
	);


	g_pcApp = new App();
	g_pcApp->Run();
	return ( 0 );
}
