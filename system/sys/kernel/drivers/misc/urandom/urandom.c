/*
 * /dev/urandom: the generator in rng.c, never blocking.  It is seeded when
 * the driver loads (normally from the full 256 bits of entropy; if not, the
 * kernel log says so).  Writes are mixed into the pool, never credited.
 *
 * /dev/random is the random driver (../random/random.c), with its own
 * generator.
 *
 * SPDX-License-Identifier: MIT
 */

#include <kernel/types.h>
#include <kernel/device.h>
#include <posix/errno.h>

#include "rng.h"

static int g_hNode = -1;

static status_t urandom_open( void *pNode, uint32 nFlags, void **ppCookie )
{
	return 0;
}

static status_t urandom_close( void *pNode, void *pCookie )
{
	return 0;
}

static int urandom_read( void *pNode, void *pCookie, off_t nPos, void *pBuf, size_t nLen )
{
	return rng_read( pBuf, nLen );
}

static int urandom_write( void *pNode, void *pCookie, off_t nPos, const void *pBuf, size_t nLen )
{
	return rng_write( pBuf, nLen );
}

static DeviceOperations_s g_sURandomOps = {
	urandom_open,
	urandom_close,
	NULL,		/* ioctl */
	urandom_read,
	urandom_write,
	NULL,		/* readv */
	NULL,		/* writev */
	NULL,		/* add_select_req */
	NULL,		/* rem_select_req */
	NULL		/* mmap */
};

status_t device_init( int nDeviceID )
{
	status_t nError = rng_init( "urandom" );

	if ( nError < 0 )
		return nError;
	g_hNode = create_device_node( nDeviceID, -1, "urandom", &g_sURandomOps, NULL );
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
