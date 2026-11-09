/*
 * sb-dock: Syllable-Build-System's ./sb compile --run for a Dock plugin.
 *
 *   sb-dock /boot/system/extensions/dock/Clock.so
 *
 * The Dock reads a plugin when it loads it, and only then: a new file copied
 * over one it has loaded changes nothing, and one it does not know of stays
 * unused.  So a plugin it knows of means a new Dock (nothing restarts it, so
 * this does, as root, as sb-agent runs it): that loads every plugin from its
 * file, in the same order, without counting on each one's unloading, which
 * not all of them do reliably.  One it does not know of is added through the
 * event the Dock preferences use, which also goes in its settings.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <signal.h>
#include <atheos/threads.h>
#include <string>
#include <vector>
#include <util/application.h>
#include <util/event.h>
#include <util/message.h>
#include <util/string.h>

extern "C" int kill_proc( proc_id hProcess, int nSigNum );	/* in libc, in no header */

typedef std::vector<os::String> PluginList;

static const char *leaf( const char *pzPath )
{
	const char *pzSlash = strrchr( pzPath, '/' );
	return ( pzSlash != NULL ) ? pzSlash + 1 : pzPath;
}

static void get_plugins( os::Event &cGet, PluginList &cList )
{
	os::Message cReply;
	os::String zPlugin;
	cList.clear();
	if( cGet.GetLastEventMessage( &cReply ) != 0 )
		return;
	for( int i = 0; cReply.FindString( "plugin", &zPlugin.str(), i ) == 0; i++ )
		cList.push_back( zPlugin );
}

static int find_plugin( const PluginList &cList, const char *pzName )
{
	for( uint i = 0; i < cList.size(); i++ )
		if( strcmp( leaf( cList[i].c_str() ), pzName ) == 0 )
			return i;
	return -1;
}

static void set_plugins( os::Event &cSet, const PluginList &cList )
{
	os::Message cMsg;
	for( uint i = 0; i < cList.size(); i++ )
		cMsg.AddString( "plugin", cList[i] );
	cSet.PostEvent( &cMsg, NULL, 0 );
}

/* Wait until the Dock has the plugin; about 15 seconds.  A new event each time:
   after a restart the Dock is a new process, which registers its own. */
static bool wait_for_plugin( const char *pzName )
{
	for( int i = 0; i < 150; i++ )
	{
		os::Event cGet;
		PluginList cList;
		if( cGet.SetToRemote( "os/Dock/GetPlugins", 0 ) == 0 )
		{
			get_plugins( cGet, cList );
			if( find_plugin( cList, pzName ) >= 0 )
				return true;
		}
		usleep( 100000 );
	}
	return false;
}

/* The Dock's process, or -1 (get_process_id() only looks in the caller's; a
   killed one stays a zombie, nothing waits for it) */
static proc_id find_dock()
{
	thread_info sInfo;
	for( int nError = get_thread_info( -1, &sInfo ); nError >= 0; nError = get_next_thread_info( &sInfo ) )
		if( strcmp( sInfo.ti_process_name, "Dock" ) == 0 && sInfo.ti_state != TS_ZOMBIE )
			return sInfo.ti_process_id;
	return -1;
}

/* A new Dock, detached from sb-agent's connection */
static bool restart_dock()
{
	proc_id hDock = find_dock();
	if( hDock < 0 || kill_proc( hDock, SIGKILL ) != 0 )
		return false;
	for( int i = 0; find_dock() >= 0; i++ )
	{
		if( i == 50 )
			return false;
		usleep( 100000 );
	}
	if( fork() == 0 )
	{
		int nNull = open( "/dev/null", O_RDWR );
		setsid();
		dup2( nNull, 0 );
		dup2( nNull, 1 );
		dup2( nNull, 2 );
		if( nNull > 2 )
			close( nNull );
		set_thread_priority( -1, 0 );
		execl( "/system/programs/Dock", "/system/programs/Dock", NULL );
		_exit( 127 );
	}
	return true;
}

int main( int argc, char **argv )
{
	if( argc != 2 )
	{
		fprintf( stderr, "usage: sb-dock /boot/system/extensions/dock/NAME.so\n" );
		return 2;
	}
	if( access( argv[1], R_OK ) != 0 )
	{
		fprintf( stderr, "sb-dock: no %s\n", argv[1] );
		return 1;
	}
	const char *pzName = leaf( argv[1] );
	new os::Application( "application/x-vnd.syllable-sb-dock" );
	os::Event cGet, cSet;
	if( cGet.SetToRemote( "os/Dock/GetPlugins", 0 ) != 0 || cSet.SetToRemote( "os/Dock/SetPlugins", 0 ) != 0 )
	{
		fprintf( stderr, "sb-dock: no Dock is running (not logged in yet?)\n" );
		_exit( 1 );
	}

	PluginList cList;
	get_plugins( cGet, cList );
	bool bKnown = find_plugin( cList, pzName ) >= 0;
	if( bKnown )
	{
		if( !restart_dock() )
		{
			fprintf( stderr, "sb-dock: could not restart the Dock\n" );
			_exit( 1 );
		}
	}
	else
	{
		cList.push_back( os::String( argv[1] ) );
		set_plugins( cSet, cList );
	}
	if( !wait_for_plugin( pzName ) )
	{
		fprintf( stderr, "sb-dock: the Dock did not load %s (see the serial log)\n", pzName );
		_exit( 1 );
	}
	printf( "%s %s\n", bKnown ? "restarted the Dock for" : "added", argv[1] );
	fflush( stdout );
	_exit( 0 );		/* no Run(): nothing of the application to shut down */
}
