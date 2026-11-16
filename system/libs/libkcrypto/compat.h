/*
 * libkcrypto: the few Linux kernel definitions the imported lib/crypto files
 * use (blake2s.c, blake2s-generic.c), without any Linux headers.
 *
 * Only <stddef.h> and the string functions are included, so the same
 * sources build into the kernel (GCC 4.1.2, -ffreestanding, no <stdint.h>),
 * into Syllable userland and on the build host.  memcpy() and memset() come
 * from <kernel/string.h> in the kernel build (inline: libkernel.so exports
 * neither, so a call to them doesn't link into a driver) and from libc's
 * <string.h> otherwise.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef _KCRYPTO_COMPAT_H
#define _KCRYPTO_COMPAT_H

#include <stddef.h>
#ifdef __KERNEL__
#include <kernel/string.h>
#else
#include <string.h>
#endif

typedef unsigned char      u8;
typedef unsigned int       u32;
typedef unsigned long long u64;

/* u32 must be 32 bits wide: a negative array size stops the build if not */
typedef char kcrypto_u32_is_32_bits[ ( sizeof( u32 ) == 4 ) ? 1 : -1 ];

#ifndef __cplusplus
#ifndef bool
typedef int bool;
#define true  1
#define false 0
#endif
#endif

#define ARRAY_SIZE( a )		( sizeof( a ) / sizeof( ( a )[0] ) )
#define DIV_ROUND_UP( n, d )	( ( ( n ) + ( d ) - 1 ) / ( d ) )
#define unlikely( x )		__builtin_expect( !!( x ), 0 )

/* Debug checks and module glue in the Linux files: compiled out */
#define IS_ENABLED( option )	0
#define WARN_ON( cond )		( ( void )0 )
#define EXPORT_SYMBOL( sym )

static inline u32 rol32( u32 v, unsigned int n )
{
	return ( v << n ) | ( v >> ( 32 - n ) );
}

static inline u32 ror32( u32 v, unsigned int n )
{
	return ( v >> n ) | ( v << ( 32 - n ) );
}

static inline u32 get_unaligned_le32( const void *p )
{
	const u8 *b = p;

	return ( u32 )b[0] | ( ( u32 )b[1] << 8 ) | ( ( u32 )b[2] << 16 ) | ( ( u32 )b[3] << 24 );
}

static inline void put_unaligned_le32( u32 v, void *p )
{
	u8 *b = p;

	b[0] = v;
	b[1] = v >> 8;
	b[2] = v >> 16;
	b[3] = v >> 24;
}

/* Byte order conversions written with byte loads and stores, so they are
   right whatever the host's byte order (no-ops on x86) */
static inline void le32_to_cpu_array( u32 *buf, unsigned int words )
{
	while ( words-- )
	{
		*buf = get_unaligned_le32( buf );
		buf++;
	}
}

static inline void cpu_to_le32_array( u32 *buf, unsigned int words )
{
	while ( words-- )
	{
		put_unaligned_le32( *buf, buf );
		buf++;
	}
}

/* Clear secrets in a way the compiler can't drop as a dead store */
static inline void memzero_explicit( void *s, size_t count )
{
	memset( s, 0, count );
	__asm__ __volatile__( "" : : "r"( s ) : "memory" );
}

#endif /* _KCRYPTO_COMPAT_H */
