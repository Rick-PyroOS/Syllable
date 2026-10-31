/*
 * /dev/random and /dev/urandom smoke test, run on Syllable.  Each check
 * prints a PYRO-RANDOM line; the last is "PYRO-RANDOM: pass" or
 * "PYRO-RANDOM: FAIL n", and the exit status is 0 only on pass.
 *
 * Statistical checks are loose sanity bounds (about 6 sigma), there to catch
 * a broken generator (constant, repeating or biased output), not to certify
 * a good one; the known-answer test for the algorithms is kcrypto-test.
 *
 * SPDX-License-Identifier: MIT
 */

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>

#define SAMPLE_SIZE	65536

static int g_nFailures = 0;
static unsigned char g_anA[SAMPLE_SIZE];
static unsigned char g_anB[SAMPLE_SIZE];

static void result( const char *pzName, int bOk, const char *pzDetail )
{
	if ( !bOk )
		g_nFailures++;
	printf( "PYRO-RANDOM: %s %s%s%s\n", bOk ? "ok" : "FAIL", pzName, pzDetail[0] ? ": " : "", pzDetail );
}

/* Read exactly nLen bytes (devices may return less per call) */
static int read_all( const char *pzPath, unsigned char *pBuf, size_t nLen )
{
	size_t nDone = 0;
	int nFile = open( pzPath, O_RDONLY );

	if ( nFile < 0 )
		return -1;
	while ( nDone < nLen )
	{
		ssize_t nGot = read( nFile, pBuf + nDone, nLen - nDone );

		if ( nGot < 0 && errno == EINTR )
			continue;
		if ( nGot <= 0 )
		{
			close( nFile );
			return -1;
		}
		nDone += nGot;
	}
	close( nFile );
	return 0;
}

static void check_stats( const char *pzPath, const unsigned char *pBuf )
{
	char zDetail[128];
	char zName[64];
	unsigned long anCount[256];
	long nOnes = 0;
	long nOnesExpect = SAMPLE_SIZE * 4L;
	double fChi = 0.0;
	double fExpect = SAMPLE_SIZE / 256.0;
	int i, b;

	memset( anCount, 0, sizeof( anCount ) );
	for ( i = 0; i < SAMPLE_SIZE; i++ )
	{
		anCount[pBuf[i]]++;
		for ( b = 0; b < 8; b++ )
			nOnes += ( pBuf[i] >> b ) & 1;
	}
	for ( i = 0; i < 256; i++ )
		fChi += ( anCount[i] - fExpect ) * ( anCount[i] - fExpect ) / fExpect;

	/* 524288 bits: sigma = 362; chi-square with 255 degrees of freedom:
	   mean 255, sigma 22.6 */
	snprintf( zName, sizeof( zName ), "%s monobit", pzPath );
	snprintf( zDetail, sizeof( zDetail ), "%ld ones of %ld bits", nOnes, SAMPLE_SIZE * 8L );
	result( zName, nOnes > nOnesExpect - 2200 && nOnes < nOnesExpect + 2200, zDetail );

	snprintf( zName, sizeof( zName ), "%s byte chi-square", pzPath );
	snprintf( zDetail, sizeof( zDetail ), "%.1f (expect about 255)", fChi );
	result( zName, fChi > 140.0 && fChi < 400.0, zDetail );
}

static void test_device( const char *pzPath )
{
	char zName[64];
	char zDetail[64];
	struct timeval sStart, sEnd;
	double fSeconds;
	unsigned char anSmall[3];

	snprintf( zName, sizeof( zName ), "%s read 64 KiB", pzPath );
	gettimeofday( &sStart, NULL );
	if ( read_all( pzPath, g_anA, SAMPLE_SIZE ) < 0 )
	{
		result( zName, 0, strerror( errno ) );
		return;
	}
	gettimeofday( &sEnd, NULL );
	fSeconds = ( sEnd.tv_sec - sStart.tv_sec ) + ( sEnd.tv_usec - sStart.tv_usec ) / 1e6;
	snprintf( zDetail, sizeof( zDetail ), "%.0f KiB/s", fSeconds > 0 ? 64.0 / fSeconds : 0.0 );
	result( zName, 1, zDetail );

	snprintf( zName, sizeof( zName ), "%s second read differs", pzPath );
	result( zName, read_all( pzPath, g_anB, SAMPLE_SIZE ) == 0 && memcmp( g_anA, g_anB, SAMPLE_SIZE ) != 0, "" );

	snprintf( zName, sizeof( zName ), "%s halves differ", pzPath );
	result( zName, memcmp( g_anA, g_anA + SAMPLE_SIZE / 2, SAMPLE_SIZE / 2 ) != 0, "" );

	snprintf( zName, sizeof( zName ), "%s 3-byte read", pzPath );
	result( zName, read_all( pzPath, anSmall, sizeof( anSmall ) ) == 0, "" );

	check_stats( pzPath, g_anA );
}

static void test_write( void )
{
	static const char azSeed[] = "random-test: mixed into the pool, never credited";
	int nFile = open( "/dev/urandom", O_WRONLY );
	ssize_t nWritten = -1;

	if ( nFile >= 0 )
	{
		nWritten = write( nFile, azSeed, sizeof( azSeed ) );
		close( nFile );
	}
	result( "/dev/urandom write", nWritten == ( ssize_t )sizeof( azSeed ), nFile < 0 || nWritten < 0 ? strerror( errno ) : "" );
}

int main( void )
{
	test_device( "/dev/random" );
	test_device( "/dev/urandom" );
	test_write();

	if ( g_nFailures == 0 )
	{
		printf( "PYRO-RANDOM: pass\n" );
		return 0;
	}
	printf( "PYRO-RANDOM: FAIL %d\n", g_nFailures );
	return 1;
}
