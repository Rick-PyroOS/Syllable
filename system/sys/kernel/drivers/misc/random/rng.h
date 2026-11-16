/*
 * The random number generator shared by the random and urandom drivers
 * (rng.c).  Each driver links its own copy: separate pools and keys.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef _RNG_H
#define _RNG_H

#include <kernel/types.h>

/* Create the lock and seed (bounded: never waits indefinitely).  pzName
   prefixes kernel log messages. */
status_t rng_init( const char *pzName );
void rng_uninit( void );

/* Wait until the generator has been seeded from 256 credited bits;
   -EAGAIN instead of waiting when bNonBlocking */
status_t rng_wait_seeded( bool bNonBlocking );

/* Generator output to / pool input from a user buffer: bytes done, or a
   negative error */
int rng_read( void *pBuf, size_t nLen );
int rng_write( const void *pBuf, size_t nLen );

#endif /* _RNG_H */
