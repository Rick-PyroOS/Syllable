/*
 * libkcrypto: the cryptographic primitives behind Syllable's random devices,
 * built as a static library for the kernel side (libkcrypto-kernel.a, linked
 * by drivers) and for userland (libkcrypto.a), and on the build host for the
 * test.
 *
 *   BLAKE2s   (RFC 7693) from Linux lib/crypto, used under the MIT licence
 *   ChaCha20  (RFC 8439) block function, written for libkcrypto
 *   kc_pool_* / kc_crng_*  an entropy pool and a fast-key-erasure generator
 *             on top of them, after the design of Linux drivers/char/random.c
 *
 * No libc or kernel headers are needed: the library only uses memcpy/memset.
 * This header deliberately pulls in nothing but <stddef.h>.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef _KCRYPTO_H
#define _KCRYPTO_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define KC_BLAKE2S_BLOCK_SIZE	64
#define KC_BLAKE2S_HASH_SIZE	32
#define KC_BLAKE2S_KEY_SIZE	32

#define KC_CHACHA20_KEY_SIZE	32
#define KC_CHACHA20_NONCE_SIZE	12
#define KC_CHACHA20_BLOCK_SIZE	64

#define KC_CRNG_KEY_SIZE	32

/* Same layout as struct blake2s_state in blake2s.h (checked in kcrypto.c) */
struct kc_blake2s
{
	unsigned int	h[8];
	unsigned int	t[2];
	unsigned int	f[2];
	unsigned char	buf[KC_BLAKE2S_BLOCK_SIZE];
	unsigned int	buflen;
	unsigned int	outlen;
};

/* BLAKE2s.  outlen is 1..32 bytes, keylen 0..32 bytes. */
void kc_blake2s_init( struct kc_blake2s *psState, size_t nOutLen );
void kc_blake2s_init_key( struct kc_blake2s *psState, size_t nOutLen, const void *pKey, size_t nKeyLen );
void kc_blake2s_update( struct kc_blake2s *psState, const void *pIn, size_t nInLen );
void kc_blake2s_final( struct kc_blake2s *psState, unsigned char *pOut );
void kc_blake2s( unsigned char *pOut, size_t nOutLen, const void *pIn, size_t nInLen, const void *pKey, size_t nKeyLen );

/* One 64-byte ChaCha20 block (RFC 8439 section 2.3: 32-bit block counter,
   96-bit nonce) */
void kc_chacha20_block( unsigned char pOut[KC_CHACHA20_BLOCK_SIZE], const unsigned char pKey[KC_CHACHA20_KEY_SIZE],
			unsigned int nCounter, const unsigned char pNonce[KC_CHACHA20_NONCE_SIZE] );

/* Entropy pool: a running BLAKE2s hash of everything mixed in. */
struct kc_pool
{
	struct kc_blake2s	hash;
};

void kc_pool_init( struct kc_pool *psPool );
void kc_pool_mix( struct kc_pool *psPool, const void *pData, size_t nLen );
/* 32 bytes derived from everything mixed in so far.  The pool goes on keyed
   by its own previous output, so earlier pool state can't be recovered from
   what comes out. */
void kc_pool_extract( struct kc_pool *psPool, unsigned char pOut[KC_CRNG_KEY_SIZE] );

/* Generator.  The key is replaced before any output is returned ("fast key
   erasure"), so neither the key nor a later compromise of it reveals bytes
   already handed out.  At most 256 GiB per call (32-bit block counter). */
void kc_crng_generate( unsigned char pKey[KC_CRNG_KEY_SIZE], void *pOut, size_t nLen );
/* Fold fresh pool output into the generator key. */
void kc_crng_reseed( unsigned char pKey[KC_CRNG_KEY_SIZE], struct kc_pool *psPool );

/* Clear a secret where the compiler can't optimise the stores away */
void kc_memzero( void *pBuf, size_t nLen );

#ifdef __cplusplus
}
#endif

#endif /* _KCRYPTO_H */
