/*
 * libkcrypto: the ChaCha20 block function, written from RFC 8439 sections
 * 2.1-2.3 (20 rounds, 32-bit block counter, 96-bit nonce).
 *
 * Linux has the same function in lib/crypto/chacha.c, but under
 * GPL-2.0-or-later; this one keeps libkcrypto MIT, so it can be linked into
 * the (Library GPL) kernel and into userland without changing their licence.
 *
 * SPDX-License-Identifier: MIT
 */

#include "compat.h"
#include "kcrypto.h"

#define QUARTERROUND( a, b, c, d ) do {			\
	x[a] += x[b]; x[d] = rol32( x[d] ^ x[a], 16 );	\
	x[c] += x[d]; x[b] = rol32( x[b] ^ x[c], 12 );	\
	x[a] += x[b]; x[d] = rol32( x[d] ^ x[a], 8 );	\
	x[c] += x[d]; x[b] = rol32( x[b] ^ x[c], 7 );	\
} while ( 0 )

void kc_chacha20_block( unsigned char pOut[KC_CHACHA20_BLOCK_SIZE], const unsigned char pKey[KC_CHACHA20_KEY_SIZE],
			unsigned int nCounter, const unsigned char pNonce[KC_CHACHA20_NONCE_SIZE] )
{
	u32 s[16];
	u32 x[16];
	int i;

	/* "expand 32-byte k" */
	s[0] = 0x61707865;
	s[1] = 0x3320646e;
	s[2] = 0x79622d32;
	s[3] = 0x6b206574;
	for ( i = 0; i < 8; i++ )
		s[4 + i] = get_unaligned_le32( pKey + 4 * i );
	s[12] = nCounter;
	s[13] = get_unaligned_le32( pNonce );
	s[14] = get_unaligned_le32( pNonce + 4 );
	s[15] = get_unaligned_le32( pNonce + 8 );

	memcpy( x, s, sizeof( x ) );

	for ( i = 0; i < 20; i += 2 )
	{
		/* column round */
		QUARTERROUND( 0, 4, 8, 12 );
		QUARTERROUND( 1, 5, 9, 13 );
		QUARTERROUND( 2, 6, 10, 14 );
		QUARTERROUND( 3, 7, 11, 15 );
		/* diagonal round */
		QUARTERROUND( 0, 5, 10, 15 );
		QUARTERROUND( 1, 6, 11, 12 );
		QUARTERROUND( 2, 7, 8, 13 );
		QUARTERROUND( 3, 4, 9, 14 );
	}

	for ( i = 0; i < 16; i++ )
		put_unaligned_le32( x[i] + s[i], pOut + 4 * i );

	memzero_explicit( s, sizeof( s ) );
	memzero_explicit( x, sizeof( x ) );
}
