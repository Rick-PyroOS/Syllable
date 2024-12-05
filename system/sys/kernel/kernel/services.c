
/*
 *  The Pyro kernel
 *	Servicemanager & Client handling
 *  Copyright (C) 2009 - 2010 Dee Sharpe <demetrioussharpe@netscape.net>
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of version 2 of the GNU Library
 *  General Public License as published by the Free Software
 *  Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#include <os/kernel.h>
#include <os/service.h>
#include <os/device.h>
#include <os/kdebug.h>
#include <os/image.h>
#include <os/fs_attribs.h>
#include <macros.h>


#include "inc/scheduler.h"
#include "inc/sysbase.h"
#include "inc/global.h"
#include "inc/areas.h"
#include "inc/image.h"
#include "inc/mman.h"
#include "inc/ksyms.h"
#include "inc/smp.h"


/* Function definitions */
typedef status_t service_init_t( void );
typedef void service_uninit_t( void );
typedef void *service_get_hooks_t( int nVersion );

/* One Servicemanager */

struct ServiceHandle_t;
struct ServiceHandle_t
{
	struct ServiceHandle_t *b_psNext;
	int b_nClientID;
	char b_zName[MAX_SERVICEMANAGER_NAME_LENGTH];
	service_get_hooks_t *b_pGetHooks;
};

typedef struct ServiceHandle_t ServiceHandle_s;


/* One client */

struct ClientHandle_t;
struct ClientHandle_t
{
	struct ClientHandle_t *d_psNext;
	int d_nHandle;
	int d_nClientID;
	char d_zOriginalName[MAX_CLIENT_NAME_LENGTH];
	char d_zName[MAX_CLIENT_NAME_LENGTH];
	char d_zService[MAX_SERVICEMANAGER_NAME_LENGTH];
	bool d_bClaimed;
	void* d_pData;
};

typedef struct ClientHandle_t ClientHandle_s;


/* Global stuff */

ServiceHandle_s *g_psFirstService;
ClientHandle_s *g_psFirstClient;
static uint32 g_nDriverHandle = 0;
sem_id g_hClientListLock;

//****************************************************************************/
/** Registers a client and returns a handle to it.
 * \ingroup ServicesAPI
 * \param pzName the name of the client; will be overwritten after calling
 *     claim_client().
 * \param pzService the name of the Service the client is attached to.
 * \return a handle to the client.
 * \sa claim_client(), unregister_client()
 * \author Dee Sharpe (demetrioussharpe@netscape.net)
 *****************************************************************************/
int register_client( const char *pzName, const char *pzService )
{
	/* Create new handle */
	ClientHandle_s *psClient = ( ClientHandle_s * ) kmalloc( sizeof( ClientHandle_s ), MEMF_KERNEL );

	memset( psClient, 0, sizeof( ClientHandle_s ) );

	psClient->d_nHandle = g_nDriverHandle++;
	if ( pzName )
		strcpy( psClient->d_zName, pzName );
	else
		strcpy( psClient->d_zName, "Unknown client" );
	strcpy( psClient->d_zOriginalName, psClient->d_zName );
	if ( pzService )
		strcpy( psClient->d_zService, pzService );
	else
		strcpy( psClient->d_zService, "Unknown Service" );
	psClient->d_nClientID = -1;
	psClient->d_bClaimed = false;

	LOCK( g_hClientListLock );

	psClient->d_psNext = g_psFirstClient;
	g_psFirstClient = psClient;

	//printk( "New client %s (Service: %s)\n", psClient->d_zName, psClient->d_zService );
	UNLOCK( g_hClientListLock );

	return ( psClient->d_nHandle );
}

//****************************************************************************/
/** Unregisters a previously registered client.
 * \ingroup ServicesAPI
 * \par Note:
 * An error message will be written to the kernel log if the client is still claimed.
 * \param nHandle the handle returned by the register_client() call.
 * \sa register_client(), release_client()
 * \author Dee Sharpe (demetrioussharpe@netscape.net)
 *****************************************************************************/
void unregister_client( int nHandle )
{
	ClientHandle_s *psPrev = NULL;
	ClientHandle_s *psClient = g_psFirstClient;
	
	LOCK( g_hClientListLock );

	while ( psClient != NULL )
	{
		if ( psClient->d_nHandle == nHandle )
		{
			if ( psClient->d_bClaimed )
				printk( "Warning: trying to unregister claimed client %s\n", psClient->d_zName );
			if ( psPrev )
				psPrev->d_psNext = psClient->d_psNext;
			else
				g_psFirstClient = psClient->d_psNext;

			//printk( "Client %s removed\n", psClient->d_zName );
			kfree( psClient );
			UNLOCK( g_hClientListLock );
			return;
		}
		psPrev = psClient;
		psClient = psClient->d_psNext;
	}
	UNLOCK( g_hClientListLock );
	printk( "Error: unregister_client() called with invalid client\n" );
}

//****************************************************************************/
/** Claims a registered client.
 * \ingroup ServicesAPI
 * \param nClientID the client id of the driver.
 * \param nHandle the handle returned by the register_client() call.
 * \param pzName the new name of the client.
 * \return <code>0</code> if successful or another value if the client is already
 *     claimed.
 * \sa register_client(), release_client()
 * \author Dee Sharpe (demetrioussharpe@netscape.net)
 *****************************************************************************/
status_t claim_client( int nClientID, int nHandle, const char *pzName )
{
	ClientHandle_s *psClient = g_psFirstClient;
	
	LOCK( g_hClientListLock );

	while ( psClient != NULL )
	{
		if ( psClient->d_nHandle == nHandle )
		{
			if ( psClient->d_bClaimed )
			{
				printk( "Error: client %s already claimed\n", psClient->d_zName );
				UNLOCK( g_hClientListLock );
				return ( -1 );
			}

			/* Overwrite name */
			//printk( "New Client %s (Service: %s)\n", pzName, psClient->d_zService );

			if ( pzName )
				strcpy( psClient->d_zName, pzName );
			psClient->d_nClientID = nClientID;
			psClient->d_bClaimed = true;
			UNLOCK( g_hClientListLock );
			return ( 0 );
		}
		psClient = psClient->d_psNext;
	}
	UNLOCK( g_hClientListLock );
	printk( "Error: claim_client() called with invalid client\n" );
	return ( -1 );
}

//****************************************************************************/
/** Releases a client.
 * \ingroup ServicesAPI
 * \param nHandle the handle returned by the register_client() call.
 * \sa claim_client(), unregister_client()
 * \author Dee Sharpe (demetrioussharpe@netscape.net)
 *****************************************************************************/
void release_client( int nHandle )
{
	ClientHandle_s *psClient = g_psFirstClient;
	
	LOCK( g_hClientListLock );

	while ( psClient != NULL )
	{
		if ( psClient->d_nHandle == nHandle )
		{
			if ( !psClient->d_bClaimed )
			{
				printk( "Error: client %s not claimed, but release_client() called\n", psClient->d_zName );
				UNLOCK( g_hClientListLock );
				return;
			}

			/* Overwrite name */
			//printk( "Client %s removed\n", psClient->d_zName );
			psClient->d_nClientID = -1;
			psClient->d_bClaimed = false;
			strcpy( psClient->d_zName, psClient->d_zOriginalName );
			UNLOCK( g_hClientListLock );
			return;
		}
		psClient = psClient->d_psNext;
	}
	UNLOCK( g_hClientListLock );
	printk( "Error: release_client() called with invalid client\n" );
	return;
}


//****************************************************************************/
/** Release all clients of one driver.
 * \param nClientID - The client id of the driver.
 * \author Dee Sharpe (demetrioussharpe@netscape.net)
 *****************************************************************************/
void release_clients( int nClientID )
{
	ClientHandle_s *psClient = g_psFirstClient;
	
	extern void do_release_client( int nClientID, int nClientHandle, void* pPrivateData );
	
	LOCK( g_hClientListLock );

	while ( psClient != NULL )
	{
		ClientHandle_s *psNext = psClient->d_psNext;
		if ( psClient->d_nClientID == nClientID )
		{
			if ( !psClient->d_bClaimed )
			{
				printk( "Error: client %s not claimed, but release_client() called\n", psClient->d_zName );
				UNLOCK( g_hClientListLock );
				continue;
			}

			/* Overwrite name */
			//printk( "Client %s removed\n", psClient->d_zName );
			psClient->d_nClientID = -1;
			psClient->d_bClaimed = false;
			strcpy( psClient->d_zName, psClient->d_zOriginalName );
			UNLOCK( g_hClientListLock );
			return;
		}
		psClient = psNext;
	}
	UNLOCK( g_hClientListLock );
	return;
}

//****************************************************************************/
/** Returns a <code>ClientInfo_s</code> structure for a registered client.
 * \ingroup ServicesAPI
 * \param psInfo a pointer to the <code>ClientInfo_s</code> structure to fill.
 * \param nIndex the index of the client.
 * \return <code>0</code> if successful.
 * \author Dee Sharpe (demetrioussharpe@netscape.net)
 *****************************************************************************/
status_t get_client_info( ClientInfo_s * psInfo, int nIndex )
{
	ClientHandle_s *psClient = g_psFirstClient;
	int nCount = 0;

	if ( nIndex < 0 )
		return ( -1 );
		
	LOCK( g_hClientListLock );

	while ( psClient != NULL )
	{
		if ( nCount == nIndex )
		{
			psInfo->di_nHandle = psClient->d_nHandle;
			strcpy( psInfo->di_zOriginalName, psClient->d_zOriginalName );
			strcpy( psInfo->di_zName, psClient->d_zName );
			strcpy( psInfo->di_zService, psClient->d_zService );
			UNLOCK( g_hClientListLock );
			return ( 0 );
		}
		nCount++;
		psClient = psClient->d_psNext;
	}
	UNLOCK( g_hClientListLock );
	return ( -1 );
}

//****************************************************************************/
/** Sets private client driver data of one client.
 * \ingroup ServicesAPI
 * \param nHandle the handle returned by the register_client() call.
 * \param pData the data.
 * \sa get_client_data()
 * \author Dee Sharpe (demetrioussharpe@netscape.net)
 *****************************************************************************/
void set_client_data( int nHandle, void* pData )
{
	ClientHandle_s *psClient = g_psFirstClient;
	
	LOCK( g_hClientListLock );

	while ( psClient != NULL )
	{
		if ( psClient->d_nHandle == nHandle )
		{
			if ( !psClient->d_bClaimed )
			{
				printk( "Error: client %s not claimed, but set_client_data() called\n", psClient->d_zName );
				UNLOCK( g_hClientListLock );
				return;
			}
			
			psClient->d_pData = pData;
			UNLOCK( g_hClientListLock );
			return;
		}
		psClient = psClient->d_psNext;
	}
	UNLOCK( g_hClientListLock );
	printk( "Error: set_client_data() called with invalid client\n" );
	return;
}

//****************************************************************************/
/** Returns the private client driver data of one client.
 * \ingroup ServicesAPI
 * \param nHandle the handle of the client.
 * \sa set_client_data()
 * \author Dee Sharpe (demetrioussharpe@netscape.net)
 *****************************************************************************/
void* get_client_data( int nHandle )
{
	ClientHandle_s *psClient = g_psFirstClient;
	
	LOCK( g_hClientListLock );

	while ( psClient != NULL )
	{
		if ( psClient->d_nHandle == nHandle )
		{
			void* pData = psClient->d_pData;
			UNLOCK( g_hClientListLock );
			return( pData );
		}
		psClient = psClient->d_psNext;
	}
	UNLOCK( g_hClientListLock );
	printk( "Error: set_client_data() called with invalid client\n" );
	return( NULL );
}


//****************************************************************************/
/** Registers a servicemanager.
 * \ingroup ServicesAPI
 * \param nClientID client id of the driver registering this Servicemanager
 * \param pzName the name of the Servicemanager.
 * \param pfHooks pointer to the function table of the Servicemanager
 * \sa get_Servicemanager()
 * \return 0 if successful.
 * \author Dee Sharpe (demetrioussharpe@netscape.net)
 *****************************************************************************/
 status_t register_servicemanager( int nClientID, const char* pzName, servicemanager_get_hooks* pfHooks )
 {
 	/* Check if this servicemanager is already registered */
 	ServiceHandle_s *psService = g_psFirstService;
 	while ( psService != NULL )
	{
		if ( !strcmp( psService->b_zName, pzName ) )
			return ( -EEXIST );
		psService = psService->b_psNext;
	}
 	
 	/* Create a new entry */
	psService = ( ServiceHandle_s * ) kmalloc( sizeof( ServiceHandle_s ), MEMF_KERNEL );
	memset( psService, 0, sizeof( ServiceHandle_s ) );

	strcpy( psService->b_zName, pzName );
	psService->b_pGetHooks = pfHooks;
	psService->b_psNext = g_psFirstService;
	psService->b_nClientID = nClientID;
	g_psFirstService = psService;
	
	/* Mark this driver as a busmanager, even though it's not really one (implementation in vfs/dev.c) */
	set_device_as_busmanager( nClientID );
	
	return( 0 );
}

//****************************************************************************/
/** Returns a pointer to a structure containing function pointers for the
 *  the specified servicemanager.
 * \ingroup Clients
 * \param pzName the name of the Service.
 * \param nVersion the version of the Servicemanager.
 * \return a pointer to the Servicemanager's function pointer structure.
 * \author Dee Sharpe (demetrioussharpe@netscape.net)
 *****************************************************************************/
void *get_servicemanager( const char *pzName, int nVersion )
{
	ServiceHandle_s *psService = g_psFirstService;

	while ( psService != NULL )
	{
		if ( !strcmp( psService->b_zName, pzName ) )
			return ( psService->b_pGetHooks( nVersion ) );
		psService = psService->b_psNext;
	}
	return ( NULL );
}
//****************************************************************************/
/** Set defaults.
 * \internal
 * \ingroup Clients
 * \author Dee Sharpe (demetrioussharpe@netscape.net)
 *****************************************************************************/
void init_services_boot( void )
{
	/* First set everything to the default state */
	g_psFirstService = NULL;
	g_psFirstClient = NULL;
	
	g_hClientListLock = create_semaphore( "client_list_lock", 1, 0 );
}


static int dummy ( const char *pzPath, struct stat *psStat, void *pArg )
{
	return ( 0 );
}

//****************************************************************************/
/** Initialize all clients after the root volume has been mounted.
 * \internal
 * \ingroup Clients
 * \author Dee Sharpe (demetrioussharpe@netscape.net)
 *****************************************************************************/
void init_services( void )
{
	/* Iterate over /dev/service to load the servicemanager drivers */
	iterate_directory( "/dev/service", false, dummy, NULL );
}
