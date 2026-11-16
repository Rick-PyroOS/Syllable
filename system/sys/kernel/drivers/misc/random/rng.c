/*
 * The random number generator behind the random and urandom drivers
 * (random.c: /dev/random, ../urandom/urandom.c: /dev/urandom).  Each driver
 * compiles its own copy, so each device has its own pool, key and seeding.
 *
 * A cryptographic generator after the design of Linux
 * drivers/char/random.c, on libkcrypto (system/libs/libkcrypto):
 *
 *   - An entropy pool: a running BLAKE2s hash of everything mixed in
 *     (hardware random numbers, CPU timestamp jitter, the time of every
 *     read and write, and whatever is written to the device, such as the
 *     seed file saved by the previous boot).
 *   - A ChaCha20 generator with fast key erasure: every read replaces the
 *     key before any output is returned, so a later compromise can't
 *     recover bytes already handed out.
 *   - The generator is reseeded from the pool every 60 seconds, or sooner
 *     once 256 more bits have been credited.
 *
 * Seeding at load (rng_init()): RDSEED/RDRAND when the CPU has them
 * (credited), then timestamp-counter jitter, a bounded number of rounds.
 * That normally credits the full 256 bits; if the counter is too steady, the
 * generator is keyed anyway and the kernel log gets a warning, and
 * rng_wait_seeded() (what /dev/random does before reading) keeps collecting
 * jitter until the 256 bits are there.  Writes are mixed in but never
 * credited.
 *
 * SPDX-License-Identifier: MIT
 */

#include <kernel/types.h>
#include <kernel/kdebug.h>
#include <kernel/device.h>
#include <kernel/semaphore.h>
#include <kernel/areas.h>
#include <kernel/time.h>
#include <posix/errno.h>

#include <kcrypto.h>

#include "rng.h"

#define POOL_BITS		256		/* credit needed to seed / reseed early */
#define RESEED_INTERVAL		60000000LL	/* microseconds */
#define JITTER_MAX_SAMPLES	65536		/* per jitter round */
#define JITTER_MAX_ROUNDS	16		/* at load, then seed anyway (with a warning) */
#define CHUNK_SIZE		512		/* bytes generated per lock hold */

static const char *g_pzName = "random";
static sem_id g_hLock = -1;

static struct kc_pool g_sPool;
static unsigned char g_anKey[KC_CRNG_KEY_SIZE];
static bool g_bSeeded = false;			/* the generator has a key */
static bool g_bFullySeeded = false;		/* ...from at least 256 credited bits */
static int g_nCredit = 0;			/* bits credited since the last reseed */
static bigtime_t g_nLastReseed = 0;

static bool g_bHaveRdrand = false;
static bool g_bHaveRdseed = false;

/* ---- CPU sources --------------------------------------------------------- */

static inline uint64 read_tsc( void )
{
	uint32 nLow, nHigh;

	__asm__ __volatile__( "rdtsc" : "=a"( nLow ), "=d"( nHigh ) );
	return ( ( uint64 )nHigh << 32 ) | nLow;
}

static void cpuid( uint32 nLeaf, uint32 anRegs[4] )
{
	__asm__ __volatile__( "cpuid"
			      : "=a"( anRegs[0] ), "=b"( anRegs[1] ), "=c"( anRegs[2] ), "=d"( anRegs[3] )
			      : "a"( nLeaf ), "c"( 0 ) );
}

/* RDRAND/RDSEED as opcodes: binutils 2.17 doesn't know the mnemonics.
   The carry flag says whether the value is valid. */
static bool rdrand32( uint32 *pnValue )
{
	unsigned char nOk;

	__asm__ __volatile__( ".byte 0x0f, 0xc7, 0xf0; setc %1"	/* rdrand %eax */
			      : "=a"( *pnValue ), "=qm"( nOk ) : : "cc" );
	return nOk;
}

static bool rdseed32( uint32 *pnValue )
{
	unsigned char nOk;

	__asm__ __volatile__( ".byte 0x0f, 0xc7, 0xf8; setc %1"	/* rdseed %eax */
			      : "=a"( *pnValue ), "=qm"( nOk ) : : "cc" );
	return nOk;
}

static void detect_cpu_sources( void )
{
	uint32 anRegs[4];
	uint32 nMaxLeaf;

	cpuid( 0, anRegs );
	nMaxLeaf = anRegs[0];
	if ( nMaxLeaf >= 1 )
	{
		cpuid( 1, anRegs );
		g_bHaveRdrand = ( anRegs[2] & ( 1 << 30 ) ) != 0;
	}
	if ( nMaxLeaf >= 7 )
	{
		cpuid( 7, anRegs );
		g_bHaveRdseed = ( anRegs[1] & ( 1 << 18 ) ) != 0;
	}
}

/* ---- Mixing (called with g_hLock held) ------------------------------------ */

/* Timing of an event: mixed in, never credited */
static void mix_event( uint32 nWhat )
{
	struct
	{
		uint64 nTsc;
		bigtime_t nSystemTime;
		uint32 nWhat;
	} sEvent;

	sEvent.nTsc = read_tsc();
	sEvent.nSystemTime = get_system_time();
	sEvent.nWhat = nWhat;
	kc_pool_mix( &g_sPool, &sEvent, sizeof( sEvent ) );
}

/* 64 bytes from RDSEED (preferred) or RDRAND; credited in full when all of
   them came through */
static void mix_hardware( void )
{
	uint32 anWords[16];
	int nGot = 0;
	int nTries;
	int i;

	for ( i = 0; i < 16; i++ )
	{
		for ( nTries = 0; nTries < 10; nTries++ )
		{
			if ( ( g_bHaveRdseed && rdseed32( &anWords[nGot] ) ) || ( g_bHaveRdrand && rdrand32( &anWords[nGot] ) ) )
			{
				nGot++;
				break;
			}
		}
	}
	if ( nGot > 0 )
	{
		kc_pool_mix( &g_sPool, anWords, nGot * sizeof( uint32 ) );
		if ( nGot == 16 )
			g_nCredit += POOL_BITS;
	}
	kc_memzero( anWords, sizeof( anWords ) );
}

/* Timestamp-counter jitter around a little work that touches memory.  A
   sample is credited one bit only when its delta and its delta-of-delta both
   differ from the previous ones (as Linux credits timer randomness), so a
   steady counter earns nothing. */
static void mix_jitter( int nWantBits )
{
	struct kc_blake2s sScratch;
	unsigned char anScratch[KC_BLAKE2S_HASH_SIZE];
	uint64 nLast, nNow;
	uint32 nDelta, nLastDelta = 0, nLastDelta2 = 0;
	uint32 nDelta2;
	int nSamples;
	int nBits = 0;

	nLast = read_tsc();
	for ( nSamples = 0; nSamples < JITTER_MAX_SAMPLES && nBits < nWantBits; nSamples++ )
	{
		kc_blake2s_init( &sScratch, sizeof( anScratch ) );
		kc_blake2s_update( &sScratch, &nLast, sizeof( nLast ) );
		kc_blake2s_final( &sScratch, anScratch );

		nNow = read_tsc();
		nDelta = ( uint32 )( nNow - nLast );
		nDelta2 = nDelta - nLastDelta;
		kc_pool_mix( &g_sPool, &nNow, sizeof( nNow ) );

		if ( nDelta != nLastDelta && nDelta2 != nLastDelta2 && nDelta != 0 )
			nBits++;
		nLastDelta = nDelta;
		nLastDelta2 = nDelta2;
		nLast = nNow;
	}
	g_nCredit += nBits;
	kc_memzero( anScratch, sizeof( anScratch ) );
	kc_memzero( &sScratch, sizeof( sScratch ) );
	kerndbg( KERN_DEBUG, "%s: %d jitter samples, %d bits credited\n", g_pzName, nSamples, nBits );
}

static void reseed( void )
{
	if ( g_nCredit >= POOL_BITS )
		g_bFullySeeded = true;
	kc_crng_reseed( g_anKey, &g_sPool );
	g_nLastReseed = get_system_time();
	g_nCredit = 0;
}
/* Key the generator.  bMustCredit: keep collecting jitter until 256 bits
   are credited; otherwise give up after JITTER_MAX_ROUNDS with a warning. */
static void seed( bool bMustCredit )
{
	int nRound;

	mix_event( 0 );
	{
		bigtime_t nRealTime = get_real_time();

		kc_pool_mix( &g_sPool, &nRealTime, sizeof( nRealTime ) );
	}
	if ( g_bHaveRdseed || g_bHaveRdrand )
		mix_hardware();
	for ( nRound = 0; ( bMustCredit || nRound < JITTER_MAX_ROUNDS ) && g_nCredit < POOL_BITS; nRound++ )
		mix_jitter( POOL_BITS - g_nCredit );
	if ( g_nCredit < POOL_BITS )
		printk( "%s: WARNING: only %d of %d bits of entropy at seeding; output may be predictable\n", g_pzName, g_nCredit, POOL_BITS );
	reseed();
	g_bSeeded = true;
}

/* ---- Interface for the drivers (rng.h) ------------------------------------- */

status_t rng_init( const char *pzName )
{
	g_pzName = pzName;
	g_hLock = create_semaphore( "rng_lock", 1, 0 );
	if ( g_hLock < 0 )
		return g_hLock;

	kc_pool_init( &g_sPool );
	detect_cpu_sources();
	LOCK( g_hLock );
	seed( false );
	UNLOCK( g_hLock );
	printk( "%s: seeded%s (rdseed %s, rdrand %s)\n", g_pzName, g_bFullySeeded ? "" : " WITHOUT full entropy",
		g_bHaveRdseed ? "yes" : "no", g_bHaveRdrand ? "yes" : "no" );
	return 0;
}

void rng_uninit( void )
{
	delete_semaphore( g_hLock );
	g_hLock = -1;
	kc_memzero( g_anKey, sizeof( g_anKey ) );
	kc_memzero( &g_sPool, sizeof( g_sPool ) );
	g_bSeeded = g_bFullySeeded = false;
}

status_t rng_wait_seeded( bool bNonBlocking )
{
	if ( g_bFullySeeded )
		return 0;
	if ( bNonBlocking )
		return -EAGAIN;
	LOCK( g_hLock );
	if ( !g_bFullySeeded )
		seed( true );
	UNLOCK( g_hLock );
	return 0;
}

int rng_read( void *pBuf, size_t nLen )
{
	unsigned char anChunk[CHUNK_SIZE];
	unsigned char *pDst = pBuf;
	size_t nDone = 0;
	int nError = 0;

	while ( nDone < nLen )
	{
		size_t nChunk = nLen - nDone < sizeof( anChunk ) ? nLen - nDone : sizeof( anChunk );

		LOCK( g_hLock );
		if ( !g_bSeeded )
			seed( false );
		mix_event( 1 );
		if ( g_nCredit >= POOL_BITS || get_system_time() - g_nLastReseed > RESEED_INTERVAL )
		{
			if ( g_bHaveRdseed || g_bHaveRdrand )
				mix_hardware();
			reseed();
		}
		kc_crng_generate( g_anKey, anChunk, nChunk );
		UNLOCK( g_hLock );

		nError = memcpy_to_user( pDst + nDone, anChunk, nChunk );
		if ( nError < 0 )
			break;
		nDone += nChunk;
	}
	kc_memzero( anChunk, sizeof( anChunk ) );

	if ( nDone == 0 && nError < 0 )
		return nError;
	return nDone;
}

int rng_write( const void *pBuf, size_t nLen )
{
	unsigned char anChunk[CHUNK_SIZE];
	const unsigned char *pSrc = pBuf;
	size_t nDone = 0;
	int nError = 0;

	while ( nDone < nLen )
	{
		size_t nChunk = nLen - nDone < sizeof( anChunk ) ? nLen - nDone : sizeof( anChunk );

		nError = memcpy_from_user( anChunk, pSrc + nDone, nChunk );
		if ( nError < 0 )
			break;
		LOCK( g_hLock );
		kc_pool_mix( &g_sPool, anChunk, nChunk );
		mix_event( 2 );
		UNLOCK( g_hLock );
		nDone += nChunk;
	}
	kc_memzero( anChunk, sizeof( anChunk ) );

	if ( nDone == 0 && nError < 0 )
		return nError;
	return nDone;
}
