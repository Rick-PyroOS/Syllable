
#include <signal.h>


using namespace os;


int main( int argc, char **argv )
{
	dbprintf( "Screensaver alive at %d\n", get_thread_id( NULL ) );

	signal( SIGINT, SIG_IGN );
	signal( SIGQUIT, SIG_IGN );
	signal( SIGTERM, SIG_IGN );
	
	while(1 /*!InputServer::IsRestart() && !InputServer::IsQuit()*/) {
		Engine *pcEngine = new Engine();
		
		pcEngine->Start();
	}
	
	return ( 0 );
}

