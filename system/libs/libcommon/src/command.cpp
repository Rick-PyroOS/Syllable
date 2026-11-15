#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "command.h"
#include "debug.h"


using namespace common;

Command::Command( callback pfnCB )
{
	FN_START();
	
	m_pfnCallback = pfnCB;
	
	FN_END();
}

Command::~Command()
{
	FN_START();
	FN_END();
}

/* Command execution
 *
 * Runs szCommand (a NULL-terminated argv; szCommand[0] is looked up in PATH)
 * and passes everything it writes to stdout to the callback, in chunks.  Each
 * chunk is NUL-terminated, so the callback can treat it as a C string.
 *
 * Returns the command's exit status (0-255) once it has finished, 127 if it
 * could not be run, 128 + the signal number if a signal killed it, or -1 with
 * errno set if the pipe, the fork or the wait failed.
 */
int Command::Execute( char** szCommand )
{
	FN_START();
	
	if( szCommand == NULL || szCommand[0] == NULL ) {
		errno = EINVAL;
		FN_END();
		return( -1 );
	}
	
	int hPipe[2];
	
	if( pipe( hPipe ) < 0 ) {
		FN_PRINTF( "Failed to create pipe: %s\n", strerror( errno ) );
		FN_END();
		return( -1 );
	}
	
	pid_t hPid = fork();
	
	if( hPid < 0 ) {
		int nError = errno;
		FN_PRINTF( "Failed to fork process: %s\n", strerror( nError ) );
		close( hPipe[0] );
		close( hPipe[1] );
		errno = nError;
		FN_END();
		return( -1 );
	}
	
	if( hPid == 0 ) { /* Child */
		close( hPipe[0] );
		if( hPipe[1] != STDOUT_FILENO ) {
			dup2( hPipe[1], STDOUT_FILENO );
			close( hPipe[1] );
		}
		execvp( szCommand[0], szCommand );
		/* Only reached if the command could not be run.  _exit() so the child
		   doesn't go on to run the caller's code or flush its stdio buffers. */
		_exit( 127 );
	}
	
	/* Parent: close our copy of the write end, or read() never sees EOF */
	close( hPipe[1] );
	
	char szBuf[2048];
	ssize_t nCount;
	
	for( ;; ) {
		nCount = read( hPipe[0], szBuf, sizeof( szBuf ) - 1 );
		if( nCount < 0 ) {
			if( errno == EINTR )
				continue;
			FN_PRINTF( "Failed to read from pipe: %s\n", strerror( errno ) );
			break;
		}
		if( nCount == 0 )
			break;
		
		szBuf[nCount] = '\0';
		if( m_pfnCallback ) {
			FN_PRINTF( "Sending data to callback function\n" );
			m_pfnCallback( szBuf );
		}
	}
	
	close( hPipe[0] );
	
	/* Reap the child so it doesn't stay around as a zombie */
	int nStatus;
	pid_t hRet;
	
	while( ( hRet = waitpid( hPid, &nStatus, 0 ) ) < 0 && errno == EINTR )
		;
	
	FN_END();
	
	if( hRet < 0 )
		return( -1 );
	if( WIFEXITED( nStatus ) )
		return( WEXITSTATUS( nStatus ) );
	if( WIFSIGNALED( nStatus ) )
		return( 128 + WTERMSIG( nStatus ) );
	return( -1 );
}

void Command::SetCallback( callback pfnCB )
{
	FN_START();
	
	m_pfnCallback = pfnCB;
	
	FN_END();
}

