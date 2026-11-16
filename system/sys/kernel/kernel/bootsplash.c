/*
 *  The Syllable kernel
 *  Boot screen: the splash picture with a progress bar, and the boot
 *  console.
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of version 2 of the GNU Library
 *  General Public License as published by the Free Software
 *  Foundation.
 *
 *  How the screen looks from GRUB to the desktop:
 *
 *  - GRUB 2 starts the kernel in the graphics mode the Multiboot header asks
 *    for (intel.s: 1024x768x32, or whatever graphics mode it can set), and
 *    says so in the info block (MB_INFO_FRAMEBUFFER).  The screen is black
 *    for a moment while GRUB loads the kernel and modules.
 *  - Once areas work, the framebuffer is mapped and the splash is drawn: a
 *    boot module whose command line contains "splash" (/boot/grub/splash.img,
 *    made by system/sys/boot/splash/make-splash.py), centred, with the
 *    progress bar where the file says.  The bar moves with the boot modules,
 *    the drivers, and the root file system, and is full when init starts.
 *  - From init on the kernel leaves the screen alone: the appserver sets its
 *    own mode.
 *  - splash=false, or no splash module: the kernel log is drawn as text into
 *    the framebuffer instead (bootsplash_font.c, 8x16).
 *  - A panic shows the log on screen in either case.
 *  - In text mode (GRUB legacy, or no graphics mode) the kernel uses the old
 *    80x25 console, now white on black; quiet=true keeps the log off it.
 *    The log always goes to the serial port and the kernel log buffer.
 */

#include <kernel/types.h>
#include <kernel/kdebug.h>
#include <kernel/stdlib.h>
#include <kernel/areas.h>
#include <kernel/multiboot.h>
#include <macros.h>

#include <inc/sysbase.h>
#include <inc/bootsplash.h>

extern const uint8 g_anBootFont8x16[96][16];	/* bootsplash_font.c: ' ' to DEL */

/* The splash file (little-endian), made by make-splash.py */
typedef struct
{
	char sh_anMagic[4];	/* "PSPL" */
	uint16 sh_nVersion;	/* 1 */
	uint16 sh_nHeaderSize;
	uint16 sh_nWidth;
	uint16 sh_nHeight;
	uint32 sh_nBackground;	/* 0x00RRGGBB around the picture */
	/* the progress bar, in picture coordinates */
	uint16 sh_nBarX;
	uint16 sh_nBarY;
	uint16 sh_nBarWidth;
	uint16 sh_nBarHeight;
	uint32 sh_nBarTrack;	/* 0x00RRGGBB */
	uint32 sh_nBarBorder;
	uint32 sh_nBarFillLeft;	/* the fill is a gradient from left to right */
	uint32 sh_nBarFillRight;
	uint32 sh_nDataSize;	/* bytes of pixel data after the header */
	/* Then the pixels, row by row from the top, as runs of 4 bytes:
	   count (1-255), blue, green, red.  Runs may cross rows. */
} __attribute__ ((packed)) SplashHeader_s;

enum
{
	SCREEN_TEXT,		/* the 80x25 VGA text console */
	SCREEN_FB_WAIT,		/* framebuffer, not mapped yet: draw nothing */
	SCREEN_SPLASH,		/* framebuffer with the splash and progress bar */
	SCREEN_FB_TEXT,		/* framebuffer with the kernel log as text */
	SCREEN_HANDED_OVER	/* init started: the screen is the appserver's */
};

static int g_nScreen = SCREEN_TEXT;
static bool g_bQuiet = false;
static bool g_bSplash = true;
static bool g_bPanic = false;

static uint32 g_nFbPhys;
static uint32 g_nFbPitch;
static uint32 g_nFbWidth;
static uint32 g_nFbHeight;
static uint32 g_nFbBpp;		/* 32, 24 or 16 */
static uint8 g_anFbPos[3];	/* red, green, blue: bit position */
static uint8 g_anFbSize[3];	/* and size */
static uint8 *g_pFb = NULL;	/* mapped */

static const SplashHeader_s *g_psSplash = NULL;
static int g_nOriginX, g_nOriginY;	/* where the picture's corner is on screen */
static int g_nProgress = -1;	/* percent shown */
static int g_nDrivers = 0;

/* framebuffer console */
static int g_nTextX = 0, g_nTextY = 0, g_nTextCols, g_nTextRows;

static bool find_arg( const char *pzParams, const char *pzName, bool *pbValue )
{
	const char *p = pzParams;
	int nLen = strlen( pzName );

	while ( p != NULL && *p != '\0' )
	{
		while ( *p == ' ' )
			p++;
		if ( strncmp( p, pzName, nLen ) == 0 )
		{
			p += nLen;
			*pbValue = ( strncmp( p, "true", 4 ) == 0 || strncmp( p, "1", 1 ) == 0 || strncmp( p, "on", 2 ) == 0 );
			return ( true );
		}
		p = strchr( p, ' ' );
	}
	return ( false );
}

void bootsplash_early( const MultiBootHeader_s *psInfo, const char *pzParams )
{
	find_arg( pzParams, "quiet=", &g_bQuiet );
	find_arg( pzParams, "splash=", &g_bSplash );

	if ( ( psInfo->mbh_nFlags & MB_INFO_FRAMEBUFFER ) == 0 )
		return;
	if ( psInfo->mbh_nFramebufferType != MB_FRAMEBUFFER_TYPE_RGB )
		return;		/* text mode (type 2), or a palette mode we don't draw in */
	if ( psInfo->mbh_nFramebufferBpp != 32 && psInfo->mbh_nFramebufferBpp != 24 && psInfo->mbh_nFramebufferBpp != 16 )
		return;
	if ( psInfo->mbh_nFramebufferAddr >> 32 )
		return;		/* above 4 GiB: can't map it */

	g_nFbPhys = ( uint32 )psInfo->mbh_nFramebufferAddr;
	g_nFbPitch = psInfo->mbh_nFramebufferPitch;
	g_nFbWidth = psInfo->mbh_nFramebufferWidth;
	g_nFbHeight = psInfo->mbh_nFramebufferHeight;
	g_nFbBpp = psInfo->mbh_nFramebufferBpp;
	g_anFbPos[0] = psInfo->mbh_nRedPos;
	g_anFbSize[0] = psInfo->mbh_nRedSize;
	g_anFbPos[1] = psInfo->mbh_nGreenPos;
	g_anFbSize[1] = psInfo->mbh_nGreenSize;
	g_anFbPos[2] = psInfo->mbh_nBluePos;
	g_anFbSize[2] = psInfo->mbh_nBlueSize;
	g_nScreen = SCREEN_FB_WAIT;
}

/* ---- drawing ------------------------------------------------------------------- */

static inline uint32 fb_colour( uint32 nRGB )
{
	uint32 r = ( nRGB >> 16 ) & 0xff, g = ( nRGB >> 8 ) & 0xff, b = nRGB & 0xff;

	return ( ( ( r >> ( 8 - g_anFbSize[0] ) ) << g_anFbPos[0] ) |
		 ( ( g >> ( 8 - g_anFbSize[1] ) ) << g_anFbPos[1] ) |
		 ( ( b >> ( 8 - g_anFbSize[2] ) ) << g_anFbPos[2] ) );
}

static inline void put_pixel( int x, int y, uint32 nPixel )
{
	uint8 *p;

	if ( x < 0 || y < 0 || x >= g_nFbWidth || y >= g_nFbHeight )
		return;
	p = g_pFb + y * g_nFbPitch;
	switch ( g_nFbBpp )
	{
	case 32:
		( ( uint32 * )p )[x] = nPixel;
		break;
	case 24:
		p += x * 3;
		p[0] = nPixel;
		p[1] = nPixel >> 8;
		p[2] = nPixel >> 16;
		break;
	case 16:
		( ( uint16 * )p )[x] = nPixel;
		break;
	}
}

static void fill_rect( int x, int y, int w, int h, uint32 nRGB )
{
	uint32 nPixel = fb_colour( nRGB );
	int i, j;

	for ( j = y; j < y + h; ++j )
		for ( i = x; i < x + w; ++i )
			put_pixel( i, j, nPixel );
}

static void draw_splash( void )
{
	const SplashHeader_s *h = g_psSplash;
	const uint8 *pData = ( const uint8 * )h + h->sh_nHeaderSize;
	const uint8 *pEnd = pData + h->sh_nDataSize;
	int nX = 0, nY = 0;
	int i;

	g_nOriginX = ( ( int )g_nFbWidth - h->sh_nWidth ) / 2;
	g_nOriginY = ( ( int )g_nFbHeight - h->sh_nHeight ) / 2;

	/* around the picture (a screen bigger than the picture) */
	if ( g_nOriginX > 0 || g_nOriginY > 0 )
		fill_rect( 0, 0, g_nFbWidth, g_nFbHeight, h->sh_nBackground );

	while ( pData + 4 <= pEnd && nY < h->sh_nHeight )
	{
		int nCount = pData[0];
		uint32 nPixel = fb_colour( ( pData[3] << 16 ) | ( pData[2] << 8 ) | pData[1] );

		pData += 4;
		for ( i = 0; i < nCount; ++i )
		{
			put_pixel( g_nOriginX + nX, g_nOriginY + nY, nPixel );
			if ( ++nX == h->sh_nWidth )
			{
				nX = 0;
				nY++;
			}
		}
	}
}

static uint32 blend( uint32 a, uint32 b, int nPos, int nRange )
{
	int i;
	uint32 nOut = 0;

	for ( i = 0; i < 24; i += 8 )
	{
		int ca = ( a >> i ) & 0xff, cb = ( b >> i ) & 0xff;

		nOut |= ( ( ca + ( cb - ca ) * nPos / ( nRange > 0 ? nRange : 1 ) ) & 0xff ) << i;
	}
	return ( nOut );
}

static void draw_bar( int nFrom, int nTo )
{
	const SplashHeader_s *h = g_psSplash;
	int x0 = g_nOriginX + h->sh_nBarX, y0 = g_nOriginY + h->sh_nBarY;
	int w = h->sh_nBarWidth, ht = h->sh_nBarHeight;
	int nInner = w - 4;
	int a = nInner * nFrom / 100, b = nInner * nTo / 100;
	int x;

	if ( nFrom < 0 )
	{
		/* the empty bar: a 1-pixel border, a 1-pixel gap, then the track */
		fill_rect( x0, y0, w, ht, h->sh_nBarBorder );
		fill_rect( x0 + 1, y0 + 1, w - 2, ht - 2, h->sh_nBarTrack );
		a = 0;
	}
	for ( x = a; x < b; ++x )
	{
		uint32 nPixel = fb_colour( blend( h->sh_nBarFillLeft, h->sh_nBarFillRight, x, nInner ) );
		int y;

		for ( y = 2; y < ht - 2; ++y )
			put_pixel( x0 + 2 + x, y0 + y, nPixel );
	}
}

/* ---- framebuffer console -------------------------------------------------------- */

static void fbcon_clear( void )
{
	fill_rect( 0, 0, g_nFbWidth, g_nFbHeight, 0x000000 );
	g_nTextCols = g_nFbWidth / 8;
	g_nTextRows = g_nFbHeight / 16;
	g_nTextX = g_nTextY = 0;
}

static void fbcon_scroll( void )
{
	int nRow = 16 * g_nFbPitch;

	memmove( g_pFb, g_pFb + nRow, ( g_nTextRows - 1 ) * nRow );
	memset( g_pFb + ( g_nTextRows - 1 ) * nRow, 0, nRow );
	g_nTextY--;
}

static void fbcon_char( int c )
{
	uint32 nFg = fb_colour( 0xd8d8d8 ), nBg = fb_colour( 0x000000 );
	const uint8 *pGlyph;
	int x, y;

	if ( c < 32 || c > 127 )
		c = '?';
	pGlyph = g_anBootFont8x16[c - 32];
	for ( y = 0; y < 16; ++y )
		for ( x = 0; x < 8; ++x )
			put_pixel( g_nTextX * 8 + x, g_nTextY * 16 + y, ( pGlyph[y] & ( 0x80 >> x ) ) ? nFg : nBg );
}

bool bootsplash_fb_write( const char *pData, int nSize )
{
	int i;

	if ( g_nScreen == SCREEN_TEXT )
		return ( false );	/* dbconsole.c writes the text console */
	if ( g_nScreen != SCREEN_FB_TEXT || g_pFb == NULL )
		return ( true );	/* nothing on screen */

	for ( i = 0; i < nSize; ++i )
	{
		char c = pData[i];

		if ( c == '\r' )
			continue;
		if ( c == '\n' || g_nTextX >= g_nTextCols )
		{
			g_nTextX = 0;
			if ( ++g_nTextY >= g_nTextRows )
				fbcon_scroll();
			if ( c == '\n' )
				continue;
		}
		fbcon_char( ( uint8 )c );
		g_nTextX++;
	}
	return ( true );
}

bool bootsplash_text_wanted( void )
{
	return ( g_bQuiet == false || g_bPanic );
}

/* ---- the boot ---------------------------------------------------------------------- */

static const SplashHeader_s *find_splash( void )
{
	int i;

	for ( i = 0; i < g_sSysBase.ex_nBootModuleCount; ++i )
	{
		BootModule_s *psModule = g_sSysBase.ex_asBootModules + i;
		const SplashHeader_s *h = ( const SplashHeader_s * )psModule->bm_pAddress;

		if ( strstr( psModule->bm_pzModuleArgs, "splash" ) == NULL )
			continue;
		if ( psModule->bm_nSize < sizeof( SplashHeader_s ) || memcmp( h->sh_anMagic, "PSPL", 4 ) != 0 || h->sh_nVersion != 1 ||
		     h->sh_nHeaderSize < sizeof( SplashHeader_s ) || h->sh_nHeaderSize + h->sh_nDataSize > psModule->bm_nSize )
		{
			printk( "bootsplash: %s is not a splash file\n", psModule->bm_pzModuleArgs );
			continue;
		}
		return ( h );
	}
	return ( NULL );
}

void bootsplash_init( void )
{
	uint32 nOffset, nSize;
	uint8 *pMap = NULL;
	area_id hArea;

	if ( g_nScreen != SCREEN_FB_WAIT )
		return;

	/* the framebuffer need not start on a page */
	nOffset = g_nFbPhys & ~PAGE_MASK;
	nSize = ( nOffset + g_nFbPitch * g_nFbHeight + PAGE_SIZE - 1 ) & PAGE_MASK;
	hArea = create_area( "boot_framebuffer", ( void ** )&pMap, nSize, nSize, AREA_ANY_ADDRESS | AREA_KERNEL, AREA_FULL_LOCK );
	if ( hArea < 0 || remap_area( hArea, ( void * )( g_nFbPhys & PAGE_MASK ) ) < 0 )
	{
		printk( "bootsplash: can't map the framebuffer at %08x\n", g_nFbPhys );
		g_nScreen = SCREEN_HANDED_OVER;
		return;
	}
	g_pFb = pMap + nOffset;
	printk( "bootsplash: %dx%dx%d framebuffer at %08x, pitch %d, red %d:%d green %d:%d blue %d:%d\n", g_nFbWidth, g_nFbHeight, g_nFbBpp, g_nFbPhys,
		g_nFbPitch, g_anFbPos[0], g_anFbSize[0], g_anFbPos[1], g_anFbSize[1], g_anFbPos[2], g_anFbSize[2] );

	g_psSplash = g_bSplash ? find_splash() : NULL;
	if ( g_psSplash != NULL )
	{
		draw_splash();
		g_nScreen = SCREEN_SPLASH;
		draw_bar( -1, 0 );
		g_nProgress = 0;
		bootsplash_progress( 3 );
	}
	else
	{
		/* splash=false or no splash module: the log as text */
		fbcon_clear();
		g_nScreen = SCREEN_FB_TEXT;
	}
}

void bootsplash_progress( int nPercent )
{
	if ( g_nScreen != SCREEN_SPLASH )
		return;
	if ( nPercent > 100 )
		nPercent = 100;
	if ( nPercent <= g_nProgress )
		return;
	draw_bar( g_nProgress, nPercent );
	g_nProgress = nPercent;
}

void bootsplash_driver_step( void )
{
	/* The driver count isn't known in advance: 25% to 90%, closing in on 90%
	   (half way after 24 drivers, 90% of the way after about 200). */
	g_nDrivers++;
	bootsplash_progress( 25 + 65 * g_nDrivers / ( g_nDrivers + 24 ) );
}

void bootsplash_done( void )
{
	bootsplash_progress( 100 );
	if ( g_nScreen == SCREEN_SPLASH || g_nScreen == SCREEN_FB_TEXT || g_nScreen == SCREEN_FB_WAIT )
		g_nScreen = SCREEN_HANDED_OVER;
}

void bootsplash_panic( void )
{
	g_bPanic = true;
	if ( g_pFb != NULL && g_nScreen != SCREEN_FB_TEXT )
	{
		/* the screen may be the appserver's by now, in any mode; this assumes
		   it still has the one the kernel started in */
		fbcon_clear();
		g_nScreen = SCREEN_FB_TEXT;
	}
}
