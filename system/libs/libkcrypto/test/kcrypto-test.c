/*
 * libkcrypto known-answer test.  Builds on the host (test/Makefile: make -C
 * system/libs/libkcrypto/test) and on Syllable (kcrypto-test).  Each check
 * prints a PYRO-KCRYPTO line; the last is "PYRO-KCRYPTO: pass" or
 * "PYRO-KCRYPTO: FAIL n", and the exit status is 0 only on pass.
 *
 *   ChaCha20: RFC 8439 section 2.3.2 and appendix A.1 vectors 1-4
 *   BLAKE2s:  RFC 7693 appendix B ("abc"), the BLAKE2 keyed KAT, and values
 *             from Python's hashlib.blake2s for the other cases
 *   pool/crng: the construction checked against hashlib (pool, reseed) and
 *             against the block function (generator)
 *
 * SPDX-License-Identifier: MIT
 */

#include <stdio.h>
#include <string.h>

#include "kcrypto.h"

static int g_nFailures = 0;

static int hex_value( char c )
{
	if ( c >= '0' && c <= '9' )
		return c - '0';
	if ( c >= 'a' && c <= 'f' )
		return c - 'a' + 10;
	return -1;
}

/* Hex string (spaces allowed) to bytes; returns the byte count */
static size_t unhex( const char *pzHex, unsigned char *pOut, size_t nMax )
{
	size_t n = 0;

	while ( *pzHex != '\0' && n < nMax )
	{
		if ( *pzHex == ' ' )
		{
			pzHex++;
			continue;
		}
		pOut[n++] = ( unsigned char )( hex_value( pzHex[0] ) << 4 | hex_value( pzHex[1] ) );
		pzHex += 2;
	}
	return n;
}

static void check( const char *pzName, const unsigned char *pGot, const char *pzExpectHex )
{
	unsigned char anExpect[256];
	size_t nLen = unhex( pzExpectHex, anExpect, sizeof( anExpect ) );
	size_t i;

	if ( memcmp( pGot, anExpect, nLen ) == 0 )
	{
		printf( "PYRO-KCRYPTO: ok %s\n", pzName );
		return;
	}
	g_nFailures++;
	printf( "PYRO-KCRYPTO: FAIL %s\n  got    ", pzName );
	for ( i = 0; i < nLen; i++ )
		printf( "%02x", pGot[i] );
	printf( "\n  expect %s\n", pzExpectHex );
}

static void check_true( const char *pzName, int bOk )
{
	if ( bOk )
		printf( "PYRO-KCRYPTO: ok %s\n", pzName );
	else
	{
		g_nFailures++;
		printf( "PYRO-KCRYPTO: FAIL %s\n", pzName );
	}
}

static void test_chacha20( void )
{
	unsigned char anKey[32], anNonce[12], anOut[64];
	int i;

	for ( i = 0; i < 32; i++ )
		anKey[i] = i;
	unhex( "000000090000004a00000000", anNonce, sizeof( anNonce ) );
	kc_chacha20_block( anOut, anKey, 1, anNonce );
	check( "chacha20 rfc8439 2.3.2", anOut,
	       "10f1e7e4d13b5915500fdd1fa32071c4c7d1f4c733c068030422aa9ac3d46c4e"
	       "d2826446079faa0914c2d705d98b02a2b5129cd1de164eb9cbd083e8a2503c4e" );

	memset( anKey, 0, sizeof( anKey ) );
	memset( anNonce, 0, sizeof( anNonce ) );
	kc_chacha20_block( anOut, anKey, 0, anNonce );
	check( "chacha20 rfc8439 A.1 #1", anOut,
	       "76b8e0ada0f13d90405d6ae55386bd28bdd219b8a08ded1aa836efcc8b770dc7"
	       "da41597c5157488d7724e03fb8d84a376a43b8f41518a11cc387b669b2ee6586" );

	kc_chacha20_block( anOut, anKey, 1, anNonce );
	check( "chacha20 rfc8439 A.1 #2", anOut,
	       "9f07e7be5551387a98ba977c732d080dcb0f29a048e3656912c6533e32ee7aed"
	       "29b721769ce64e43d57133b074d839d531ed1f28510afb45ace10a1f4b794d6f" );

	anKey[31] = 1;
	kc_chacha20_block( anOut, anKey, 1, anNonce );
	check( "chacha20 rfc8439 A.1 #3", anOut,
	       "3aeb5224ecf849929b9d828db1ced4dd832025e8018b8160b82284f3c949aa5a"
	       "8eca00bbb4a73bdad192b5c42f73f2fd4e273644c8b36125a64addeb006c13a0" );

	anKey[31] = 0;
	anKey[1] = 0xff;
	kc_chacha20_block( anOut, anKey, 2, anNonce );
	check( "chacha20 rfc8439 A.1 #4", anOut,
	       "72d54dfbf12ec44b362692df94137f328fea8da73990265ec1bbbea1ae9af0ca"
	       "13b25aa26cb4a648cb9b9d1be65b2c0924a66c54d545ec1b7374f4872e99f096" );
}

static void test_blake2s( void )
{
	unsigned char anKey[32], anIn[1000], anOut[32];
	struct kc_blake2s sState;
	int i;

	kc_blake2s( anOut, 32, "", 0, NULL, 0 );
	check( "blake2s empty", anOut, "69217a3079908094e11121d042354a7c1f55b6482ca1a51e1b250dfd1ed0eef9" );

	kc_blake2s( anOut, 32, "abc", 3, NULL, 0 );
	check( "blake2s abc (rfc7693)", anOut, "508c5e8c327c14e2e1a72ba34eeb452f37458b209ed63a294d999b4c86675982" );

	kc_blake2s( anOut, 16, "abc", 3, NULL, 0 );
	check( "blake2s-128 abc", anOut, "aa4938119b1dc7b87cbad0ffd200d0ae" );

	for ( i = 0; i < 32; i++ )
		anKey[i] = i;
	kc_blake2s( anOut, 32, "", 0, anKey, 32 );
	check( "blake2s keyed kat empty", anOut, "48a8997da407876b3d79c0d92325ad3b89cbb754d86ab71aee047ad345fd2c49" );

	for ( i = 0; i < 255; i++ )
		anIn[i] = i;
	kc_blake2s( anOut, 32, anIn, 255, anKey, 32 );
	check( "blake2s keyed kat 255", anOut, "3fb735061abc519dfe979e54c1ee5bfad0a9d858b3315bad34bde999efd724dd" );

	/* 1000 bytes fed in uneven pieces: exercises the buffering in update() */
	for ( i = 0; i < 1000; i++ )
		anIn[i] = i % 251;
	kc_blake2s_init( &sState, 32 );
	kc_blake2s_update( &sState, anIn, 1 );
	kc_blake2s_update( &sState, anIn + 1, 63 );
	kc_blake2s_update( &sState, anIn + 64, 65 );
	kc_blake2s_update( &sState, anIn + 129, 0 );
	kc_blake2s_update( &sState, anIn + 129, 871 );
	kc_blake2s_final( &sState, anOut );
	check( "blake2s 1000 bytes in pieces", anOut, "1c067a5e746fb0f6734efac9a8cdb0e11061f0077f255184365c690115392501" );
}

static void test_pool_and_crng( void )
{
	static const unsigned char anZeroNonce[12] = { 0 };
	unsigned char anKey[32], anKeyBefore[32], anOut[32], anBlock[64], anStreamKey[32];
	unsigned char anA[200], anB[200], anExpect[200];
	struct kc_pool sPool;
	unsigned int n;

	kc_pool_init( &sPool );
	kc_pool_mix( &sPool, "hello", 5 );
	kc_pool_mix( &sPool, "world", 5 );
	kc_pool_extract( &sPool, anOut );
	check( "pool extract", anOut, "96a9c4f3c823d54eeb9309b69fe19b4a28f8af924afdcb2234399e2281c46036" );

	kc_pool_mix( &sPool, "again", 5 );
	kc_pool_extract( &sPool, anOut );
	check( "pool extract again (pool keyed by its last output)", anOut,
	       "11c2d5b03a4954f1d81337012e29e00834a6924339c8c9dbb4cdd29680b4908d" );

	kc_pool_init( &sPool );
	kc_pool_mix( &sPool, "helloworld", 10 );
	memset( anKey, 0xaa, sizeof( anKey ) );
	kc_crng_reseed( anKey, &sPool );
	check( "crng reseed = blake2s(key || extract)", anKey,
	       "00cc286d5a9c7773839fd57c579e5c7bab37788f69140832f482bf3c842b8d0b" );

	/* generate(): block 0 under the key gives the next key and this
	   request's stream key; the output is that stream from counter 0 */
	memcpy( anKeyBefore, anKey, sizeof( anKey ) );
	kc_crng_generate( anKey, anA, sizeof( anA ) );

	kc_chacha20_block( anBlock, anKeyBefore, 0, anZeroNonce );
	check_true( "crng key replaced by block 0, first half", memcmp( anKey, anBlock, 32 ) == 0 );
	memcpy( anStreamKey, anBlock + 32, 32 );
	for ( n = 0; n * 64 < sizeof( anExpect ); n++ )
	{
		size_t nChunk = sizeof( anExpect ) - n * 64 < 64 ? sizeof( anExpect ) - n * 64 : 64;

		kc_chacha20_block( anBlock, anStreamKey, n, anZeroNonce );
		memcpy( anExpect + n * 64, anBlock, nChunk );
	}
	check_true( "crng output = stream under block 0, second half", memcmp( anA, anExpect, sizeof( anA ) ) == 0 );

	kc_crng_generate( anKey, anB, sizeof( anB ) );
	check_true( "crng two requests differ", memcmp( anA, anB, sizeof( anA ) ) != 0 );

	memset( anA, 0, sizeof( anA ) );
	kc_crng_generate( anKey, anA, 0 );
	kc_crng_generate( anKey, anA, 1 );
	kc_crng_generate( anKey, anA + 1, 63 );
	kc_crng_generate( anKey, anA + 64, 65 );
	check_true( "crng odd lengths", anA[129] == 0 );

	kc_memzero( anKey, sizeof( anKey ) );
	memset( anOut, 0, sizeof( anOut ) );
	check_true( "memzero", memcmp( anKey, anOut, 32 ) == 0 );
}

int main( void )
{
	test_chacha20();
	test_blake2s();
	test_pool_and_crng();

	if ( g_nFailures == 0 )
	{
		printf( "PYRO-KCRYPTO: pass\n" );
		return 0;
	}
	printf( "PYRO-KCRYPTO: FAIL %d\n", g_nFailures );
	return 1;
}
