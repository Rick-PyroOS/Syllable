/*
 * /dev/random: the generator in rng.c, waiting (or failing with EAGAIN when
 * opened O_NONBLOCK) until it has been seeded from 256 credited bits of
 * entropy.  Once seeded it never blocks again.  Writes are mixed into the
 * pool, never credited.
 *
 * This replaces the old /dev/random (the kernel's Mersenne Twister seeded
 * from the boot time, which was predictable).  /dev/urandom is the urandom
 * driver (../urandom/urandom.c), with its own generator.
 *
 * SPDX-License-Identifier: MIT
 */

#include <kernel/types.h>
#include <kernel/device.h>
#include <posix/errno.h>
#include <posix/fcntl.h>

#include "rng.h"

static int g_hNode = -1;

/* The open flags are the cookie, for O_NONBLOCK in read() */
static status_t random_open( void *pNode, uint32 nFlags, void **ppCookie )
{
	*ppCookie = ( void * )nFlags;
	return 0;
}

static status_t random_close( void *pNode, void *pCookie )
{
	return 0;
}

static int random_read( void *pNode, void *pCookie, off_t nPos, void *pBuf, size_t nLen )
{
	status_t nError = rng_wait_seeded( ( ( uint32 )pCookie & O_NONBLOCK ) != 0 );

	if ( nError < 0 )
		return nError;
	return rng_read( pBuf, nLen );
}

static int random_write( void *pNode, void *pCookie, off_t nPos, const void *pBuf, size_t nLen )
{
	return rng_write( pBuf, nLen );
}

static DeviceOperations_s g_sRandomOps = {
	random_open,
	random_close,
	NULL,		/* ioctl */
	random_read,
	random_write,
	NULL,		/* readv */
	NULL,		/* writev */
	NULL,		/* add_select_req */
	NULL,		/* rem_select_req */
	NULL		/* mmap */
};

status_t device_init( int nDeviceID )
{
	status_t nError = rng_init( "random" );

	if ( nError < 0 )
		return nError;
	g_hNode = create_device_node( nDeviceID, -1, "random", &g_sRandomOps, NULL );
	if ( g_hNode < 0 )
	{
		rng_uninit();
		return g_hNode;
	}
	return 0;
}

status_t device_uninit( int nDeviceID )
{
	delete_device_node( g_hNode );
	rng_uninit();
	return 0;
}
