#ifndef ssize_t
typedef int ssize_t;
#define ssize_t ssize_t
#endif


#include <posix/unistd.h>
#include <string.h>

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

/* Command execution */
void Command::Execute( char** szCommand )
{
	FN_START();
	
	int hPipe[2];
	
	pipe(hPipe);
	
	pid_t pid = fork();
	
	switch(pid) {
		case -1: {
			FN_PRINTF("Failed to fork process\n");
		} break;
		case 0: {
			dup2(hPipe[1], STDOUT_FILENO);
			close(hPipe[0]);
			close(hPipe[1]);
			execvp(szCommand[0], szCommand);
		} break;
		default: { /* Parent */
			char szBuf[2048];
			uint32 nBufSize = sizeof(szBuf);
			int32 nCount = 0;
			while((nCount = read(hPipe[0], szBuf, nBufSize)) != 0) {
				if(m_pfnCallback) {
					FN_PRINTF("Sending data to callback function\n");
					m_pfnCallback(szBuf);
				} else {
					FN_PRINTF("No callback function does not exist\n");
				}
			}
			close(hPipe[0]);
			close(hPipe[1]);
		}
	}
	
	FN_END();
}

void Command::SetCallback( callback pfnCB )
{
	FN_START();
	
	m_pfnCallback = pfnCB;
	
	FN_END();
}

