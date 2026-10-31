/*
 * libkcrypto: the public BLAKE2s entry points, the entropy pool and the
 * fast-key-erasure generator (after Linux drivers/char/random.c:
 * extract_entropy() and crng_fast_key_erasure()).
 *
 * SPDX-License-Identifier: MIT
 */

#include "blake2s.h"
#include "kcrypto.h"

/* struct kc_blake2s mirrors struct blake2s_state */
typedef char kcrypto_state_size_matches[ ( sizeof( struct kc_blake2s ) == sizeof( struct blake2s_state ) ) ? 1 : -1 ];

#define STATE( p )	( ( struct blake2s_state * )( p ) )

void kc_memzero( void *pBuf, size_t nLen )
{
	memzero_explicit( pBuf, nLen );
}

void kc_blake2s_init( struct kc_blake2s *psState, size_t nOutLen )
{
	blake2s_init( STATE( psState ), nOutLen );
}

void kc_blake2s_init_key( struct kc_blake2s *psState, size_t nOutLen, const void *pKey, size_t nKeyLen )
{
	blake2s_init_key( STATE( psState ), nOutLen, pKey, nKeyLen );
}

void kc_blake2s_update( struct kc_blake2s *psState, const void *pIn, size_t nInLen )
{
	blake2s_update( STATE( psState ), pIn, nInLen );
}

void kc_blake2s_final( struct kc_blake2s *psState, unsigned char *pOut )
{
	blake2s_final( STATE( psState ), pOut );
}

void kc_blake2s( unsigned char *pOut, size_t nOutLen, const void *pIn, size_t nInLen, const void *pKey, size_t nKeyLen )
{
	blake2s( pOut, pIn, pKey, nOutLen, nInLen, nKeyLen );
}

void kc_pool_init( struct kc_pool *psPool )
{
	kc_blake2s_init( &psPool->hash, KC_BLAKE2S_HASH_SIZE );
}

void kc_pool_mix( struct kc_pool *psPool, const void *pData, size_t nLen )
{
	kc_blake2s_update( &psPool->hash, pData, nLen );
}

void kc_pool_extract( struct kc_pool *psPool, unsigned char pOut[KC_CRNG_KEY_SIZE] )
{
	static const unsigned char azLabel[] = "libkcrypto pool extract";
	unsigned char anSeed[KC_BLAKE2S_HASH_SIZE];

	/* Finish the running hash, then keep the pool going keyed by it */
	kc_blake2s_final( &psPool->hash, anSeed );
	kc_blake2s_init_key( &psPool->hash, KC_BLAKE2S_HASH_SIZE, anSeed, sizeof( anSeed ) );

	/* The output is a separate keyed hash, never the pool key itself */
	kc_blake2s( pOut, KC_CRNG_KEY_SIZE, azLabel, sizeof( azLabel ), anSeed, sizeof( anSeed ) );
	memzero_explicit( anSeed, sizeof( anSeed ) );
}

void kc_crng_reseed( unsigned char pKey[KC_CRNG_KEY_SIZE], struct kc_pool *psPool )
{
	unsigned char anFresh[KC_CRNG_KEY_SIZE];
	struct kc_blake2s sHash;

	kc_pool_extract( psPool, anFresh );

	/* new key = BLAKE2s(old key || fresh): never weaker than either */
	kc_blake2s_init( &sHash, KC_CRNG_KEY_SIZE );
	kc_blake2s_update( &sHash, pKey, KC_CRNG_KEY_SIZE );
	kc_blake2s_update( &sHash, anFresh, sizeof( anFresh ) );
	kc_blake2s_final( &sHash, pKey );

	memzero_explicit( anFresh, sizeof( anFresh ) );
	memzero_explicit( &sHash, sizeof( sHash ) );
}

void kc_crng_generate( unsigned char pKey[KC_CRNG_KEY_SIZE], void *pOut, size_t nLen )
{
	static const unsigned char anNonce[KC_CHACHA20_NONCE_SIZE] = { 0 };
	unsigned char anBlock[KC_CHACHA20_BLOCK_SIZE];
	unsigned char anStreamKey[KC_CRNG_KEY_SIZE];
	unsigned char *pDst = pOut;
	unsigned int nCounter = 0;

	/* One block under the current key: the first half replaces the key, the
	   second half keys this request's stream.  The old key is gone before
	   any output is produced. */
	kc_chacha20_block( anBlock, pKey, 0, anNonce );
	memcpy( pKey, anBlock, KC_CRNG_KEY_SIZE );
	memcpy( anStreamKey, anBlock + KC_CRNG_KEY_SIZE, KC_CRNG_KEY_SIZE );

	while ( nLen > 0 )
	{
		size_t nChunk = nLen < sizeof( anBlock ) ? nLen : sizeof( anBlock );

		kc_chacha20_block( anBlock, anStreamKey, nCounter++, anNonce );
		memcpy( pDst, anBlock, nChunk );
		pDst += nChunk;
		nLen -= nChunk;
	}

	memzero_explicit( anBlock, sizeof( anBlock ) );
	memzero_explicit( anStreamKey, sizeof( anStreamKey ) );
}
