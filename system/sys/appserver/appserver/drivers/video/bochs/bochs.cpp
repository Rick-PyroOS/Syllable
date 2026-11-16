/*
 *  Syllable appserver driver for the Bochs/QEMU "DISPI" display interface
 *
 *  Drives QEMU's standard VGA (-vga std, PCI 1234:1111), bochs-display
 *  (-device bochs-display, same ID, no VGA ports) and QXL while it is in its
 *  VGA mode (-vga qxl, PCI 1b36:0100).  They all implement the Bochs VBE
 *  extension registers, which set any resolution directly without the VGA
 *  BIOS.  QEMU tracks writes to the framebuffer itself, so screen updates
 *  reach VNC and SPICE viewers without the driver reporting damage.
 *
 *  Register reference: QEMU docs/specs/standard-vga.rst and
 *  include/hw/display/bochs-vbe.h; Linux drivers/gpu/drm/tiny/bochs.c.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include <vector>

#include <atheos/types.h>
#include <atheos/pci.h>
#include <atheos/kernel.h>
#include <atheos/areas.h>
#include <atheos/isa_io.h>
#include <appserver/pci_graphics.h>

#include "../../../server/ddriver.h"
#include "../../../server/bitmap.h"

#include <gui/bitmap.h>

using namespace os;

/* Set to 0 to keep every window single-buffered (mode A), as under VESA */
#ifndef BOCHS_BACKBUFFERS
#define BOCHS_BACKBUFFERS 1
#endif

/* Bochs VBE DISPI registers */
#define VBE_DISPI_IOPORT_INDEX		0x01CE
#define VBE_DISPI_IOPORT_DATA		0x01CF

#define VBE_DISPI_INDEX_ID		0x0
#define VBE_DISPI_INDEX_XRES		0x1
#define VBE_DISPI_INDEX_YRES		0x2
#define VBE_DISPI_INDEX_BPP		0x3
#define VBE_DISPI_INDEX_ENABLE		0x4
#define VBE_DISPI_INDEX_BANK		0x5
#define VBE_DISPI_INDEX_VIRT_WIDTH	0x6
#define VBE_DISPI_INDEX_VIRT_HEIGHT	0x7
#define VBE_DISPI_INDEX_X_OFFSET	0x8
#define VBE_DISPI_INDEX_Y_OFFSET	0x9
#define VBE_DISPI_INDEX_VIDEO_MEMORY_64K 0xa

#define VBE_DISPI_ID0			0xB0C0
#define VBE_DISPI_ID5			0xB0C5

#define VBE_DISPI_DISABLED		0x00
#define VBE_DISPI_ENABLED		0x01
#define VBE_DISPI_GETCAPS		0x02
#define VBE_DISPI_8BIT_DAC		0x20
#define VBE_DISPI_LFB_ENABLED		0x40
#define VBE_DISPI_NOCLEARMEM		0x80

/* Layout of the MMIO BAR (BAR2) of std VGA and bochs-display */
#define BOCHS_MMIO_EDID			0x000	/* 256 bytes, when QEMU's edid=on */
#define BOCHS_MMIO_VGA_PORTS		0x400	/* VGA ports 0x3c0 - 0x3df */
#define BOCHS_MMIO_DISPI		0x500	/* DISPI registers, 16 bit each */
#define BOCHS_MMIO_SIZE			0x1000

/* VGA registers used to unblank the screen */
#define VGA_ATT_W			0x3C0
#define VGA_IS1_RC			0x3DA

#define PCI_VENDOR_ID_BOCHS		0x1234
#define PCI_VENDOR_ID_REDHAT		0x1b36

#define BOCHS_MIN_BACKBUFFER_MEM	( 4 * 1024 * 1024 )

/*
 * The most framebuffer memory mapped into the appserver: 1 GB.  The driver
 * maps the device's video memory up to this, so it is the device that
 * limits it: QEMU's std VGA gives at most 512 MB (a larger vgamem_mb is cut
 * to 512).  The mapping is one area in the appserver's 2 GB of address space.
 */
#define BOCHS_MAX_MAPPED_VRAM		( 1024 * 1024 * 1024 )

/* MMIO access.  A host test build can replace these to emulate the device */
#ifndef BOCHS_MMIO_ACCESSORS
static inline uint8 mmio_read8( vuint8 *pBase, uint32 nOffset ) { return ( pBase[nOffset] ); }
static inline void mmio_write8( vuint8 *pBase, uint32 nOffset, uint8 nValue ) { pBase[nOffset] = nValue; }
static inline uint16 mmio_read16( vuint8 *pBase, uint32 nOffset ) { return ( *( vuint16 * )( pBase + nOffset ) ); }
static inline void mmio_write16( vuint8 *pBase, uint32 nOffset, uint16 nValue ) { *( vuint16 * )( pBase + nOffset ) = nValue; }
#endif

static uint32 pci_size( uint32 nBase, uint32 nMask )
{
	uint32 nSize = nBase & nMask;
	return ( nSize & ~( nSize - 1 ) );
}

/* Size a 32-bit memory BAR by writing all ones to it, as the i855 driver does */
static uint32 get_pci_memory_size( int nFd, const PCI_Info_s *psInfo, int nResource )
{
	int nBus = psInfo->nBus;
	int nDev = psInfo->nDevice;
	int nFnc = psInfo->nFunction;
	int nOffset = PCI_BASE_REGISTERS + nResource * 4;
	uint32 nBase = pci_gfx_read_config( nFd, nBus, nDev, nFnc, nOffset, 4 );

	pci_gfx_write_config( nFd, nBus, nDev, nFnc, nOffset, 4, ~0 );
	uint32 nSize = pci_gfx_read_config( nFd, nBus, nDev, nFnc, nOffset, 4 );
	pci_gfx_write_config( nFd, nBus, nDev, nFnc, nOffset, 4, nBase );

	if( nSize == 0 || nSize == 0xffffffff || ( nSize & PCI_ADDRESS_SPACE ) )
		return ( 0 );
	return ( pci_size( nSize, PCI_ADDRESS_MEMORY_32_MASK ) );
}

static int bytes_per_pixel( color_space eSpace )
{
	switch( eSpace )
	{
	case CS_RGB16:
		return ( 2 );
	case CS_RGB32:
		return ( 4 );
	default:
		return ( 0 );
	}
}

class BochsDriver : public DisplayDriver
{
public:
	BochsDriver( int nFd );
	~BochsDriver();

	virtual area_id Open();
	virtual void Close();

	virtual int GetScreenModeCount();
	virtual bool GetScreenModeDesc( int nIndex, os::screen_mode *psMode );
	virtual int SetScreenMode( os::screen_mode sMode );
	virtual os::screen_mode GetCurrentScreenMode();
	virtual int GetFramebufferOffset() { return ( 0 ); }

	bool IsInitiated() const { return ( m_bIsInitiated ); }

private:
	uint16 ReadReg( int nIndex );
	void WriteReg( int nIndex, uint16 nValue );
	void Unblank();
	void ReadCaps();
	void AddMode( int nWidth, int nHeight );
	void AddEdidMode();
	void BuildModeList();

	bool m_bIsInitiated;
	PCI_Info_s m_sPCIInfo;
	const char *m_pzName;

	bool m_bHasVGA;			/* VGA ports exist (not bochs-display) */
	area_id m_hMmioArea;
	vuint8 *m_pMmio;		/* NULL: use the 0x1ce/0x1cf I/O ports */

	area_id m_hFbArea;
	uint8 *m_pFb;
	uint32 m_nFbPhys;
	uint32 m_nVramSize;		/* Bytes of framebuffer memory mapped */
	uint32 m_nScreenLimit;		/* Largest screen we offer, in bytes */
	uint32 m_nScreenReserve;	/* Bytes kept for the screen when backbuffers are on */

	int m_nMaxWidth;
	int m_nMaxHeight;
	int m_nMaxBpp;

	std::vector<os::screen_mode> m_cModes;
	os::screen_mode m_sCurrentMode;
};

BochsDriver::BochsDriver( int nFd )
{
	m_bIsInitiated = false;
	m_pzName = "Bochs DISPI";
	m_bHasVGA = true;
	m_hMmioArea = -1;
	m_pMmio = NULL;
	m_hFbArea = -1;
	m_pFb = NULL;
	m_nFbPhys = 0;
	m_nVramSize = 0;
	m_nScreenLimit = 0;
	m_nScreenReserve = 0;
	m_nMaxWidth = m_nMaxHeight = m_nMaxBpp = 0;
	m_sCurrentMode = os::screen_mode( 0, 0, 0, CS_RGB32, 60.0f );

	if( ioctl( nFd, PCI_GFX_GET_PCI_INFO, &m_sPCIInfo ) != 0 )
	{
		dbprintf( "Bochs: PCI_GFX_GET_PCI_INFO failed\n" );
		return;
	}

	bool bQXL = ( m_sPCIInfo.nVendorID == PCI_VENDOR_ID_REDHAT );
	if( bQXL )
		m_pzName = "QXL (VGA mode)";
	else if( m_sPCIInfo.nClassBase == 0x03 && m_sPCIInfo.nClassSub == 0x00 )
		m_pzName = "QEMU standard VGA";
	else
	{
		m_pzName = "QEMU bochs-display";
		m_bHasVGA = false;
	}

	/*
	 * Registers.  std VGA and bochs-display have a 4 KB MMIO BAR2 holding the
	 * DISPI registers (bochs-display has nothing else).  On QXL, BAR2 is the
	 * device ROM, so it uses the classic I/O ports.
	 */
	uint32 nBar2 = m_sPCIInfo.u.h0.nBase2;
	if( !bQXL && nBar2 != 0 && !( nBar2 & PCI_ADDRESS_SPACE ) )
	{
		m_hMmioArea = create_area( "bochs_mmio", ( void ** )&m_pMmio, BOCHS_MMIO_SIZE, AREA_FULL_ACCESS, AREA_NO_LOCK );
		if( m_hMmioArea < 0 || remap_area( m_hMmioArea, ( void * )( nBar2 & PCI_ADDRESS_MEMORY_32_MASK ) ) < 0 )
		{
			dbprintf( "Bochs: Failed to map the MMIO registers\n" );
			if( m_hMmioArea >= 0 )
				delete_area( m_hMmioArea );
			m_hMmioArea = -1;
			m_pMmio = NULL;
		}
	}
	if( m_pMmio == NULL && !m_bHasVGA )
	{
		dbprintf( "Bochs: bochs-display without MMIO registers, giving up\n" );
		return;
	}

	uint16 nId = ReadReg( VBE_DISPI_INDEX_ID );
	if( ( nId & 0xfff0 ) != VBE_DISPI_ID0 )
	{
		dbprintf( "Bochs: %s has no DISPI interface (ID 0x%04x)\n", m_pzName, nId );
		return;
	}

	/* Framebuffer: BAR0.  The DISPI register tells how much of it is VGA memory */
	m_nFbPhys = m_sPCIInfo.u.h0.nBase0 & PCI_ADDRESS_MEMORY_32_MASK;
	uint32 nBarSize = get_pci_memory_size( nFd, &m_sPCIInfo, 0 );
	uint32 nDispiSize = ( uint32 )ReadReg( VBE_DISPI_INDEX_VIDEO_MEMORY_64K ) * 64 * 1024;

	m_nVramSize = nDispiSize;
	if( m_nVramSize == 0 || ( nBarSize != 0 && nBarSize < m_nVramSize ) )
		m_nVramSize = nBarSize;
	if( m_nVramSize > BOCHS_MAX_MAPPED_VRAM )
		m_nVramSize = BOCHS_MAX_MAPPED_VRAM;
	if( m_nFbPhys == 0 || m_nVramSize == 0 )
	{
		dbprintf( "Bochs: No framebuffer (BAR0 0x%08x, %u bytes)\n", ( uint )m_nFbPhys, ( uint )m_nVramSize );
		return;
	}

	m_hFbArea = create_area( "bochs_framebuffer", ( void ** )&m_pFb, m_nVramSize, AREA_FULL_ACCESS, AREA_NO_LOCK );
	if( m_hFbArea < 0 || remap_area( m_hFbArea, ( void * )m_nFbPhys ) < 0 )
	{
		dbprintf( "Bochs: Failed to map the framebuffer\n" );
		if( m_hFbArea >= 0 )
			delete_area( m_hFbArea );
		m_hFbArea = -1;
		m_pFb = NULL;
		return;
	}

	ReadCaps();
	BuildModeList();
	if( m_cModes.empty() )
	{
		dbprintf( "Bochs: No usable screen modes\n" );
		return;
	}

	dbprintf( "Bochs: %s, DISPI ID 0x%04x, %u KB video memory at 0x%08x, %s registers, max %dx%d %d bpp, %d modes\n",
		m_pzName, nId, ( uint )( m_nVramSize / 1024 ), ( uint )m_nFbPhys, m_pMmio ? "MMIO" : "I/O port",
		m_nMaxWidth, m_nMaxHeight, m_nMaxBpp, ( int )m_cModes.size() );

#if BOCHS_BACKBUFFERS
	/*
	 * The screen always starts at offset 0 and the memory manager is set up
	 * only once, so keep room for the largest mode we list and give the rest
	 * to per-window backbuffers (toplayer.cpp).  With too little left over,
	 * leave it off; windows then stay single-buffered as under VESA.
	 */
	if( m_nVramSize >= m_nScreenReserve + BOCHS_MIN_BACKBUFFER_MEM )
	{
		InitMemory( m_nScreenReserve, m_nVramSize - m_nScreenReserve, PAGE_SIZE - 1, 63 );
		dbprintf( "Bochs: %u KB for window backbuffers\n", ( uint )( ( m_nVramSize - m_nScreenReserve ) / 1024 ) );
	}
	else
	{
		dbprintf( "Bochs: Not enough video memory for window backbuffers; give the VM more display memory to enable them\n" );
		m_nScreenReserve = m_nVramSize;
	}
#else
	m_nScreenReserve = m_nVramSize;
#endif

	m_bIsInitiated = true;
}

BochsDriver::~BochsDriver()
{
	if( m_hFbArea >= 0 )
		delete_area( m_hFbArea );
	if( m_hMmioArea >= 0 )
		delete_area( m_hMmioArea );
}

uint16 BochsDriver::ReadReg( int nIndex )
{
	if( m_pMmio != NULL )
		return ( mmio_read16( m_pMmio, BOCHS_MMIO_DISPI + nIndex * 2 ) );

	outw( nIndex, VBE_DISPI_IOPORT_INDEX );
	return ( inw( VBE_DISPI_IOPORT_DATA ) );
}

void BochsDriver::WriteReg( int nIndex, uint16 nValue )
{
	if( m_pMmio != NULL )
	{
		mmio_write16( m_pMmio, BOCHS_MMIO_DISPI + nIndex * 2, nValue );
		return;
	}

	outw( nIndex, VBE_DISPI_IOPORT_INDEX );
	outw( nValue, VBE_DISPI_IOPORT_DATA );
}

/* Turn the VGA attribute controller's video output on (bit 5 of the index) */
void BochsDriver::Unblank()
{
	if( !m_bHasVGA )
		return;

	if( m_pMmio != NULL )
	{
		( void )mmio_read8( m_pMmio, BOCHS_MMIO_VGA_PORTS + VGA_IS1_RC - 0x3c0 );
		mmio_write8( m_pMmio, BOCHS_MMIO_VGA_PORTS + VGA_ATT_W - 0x3c0, 0x20 );
	}
	else
	{
		( void )inb( VGA_IS1_RC );
		outb( 0x20, VGA_ATT_W );
	}
}

/* With GETCAPS set, XRES, YRES and BPP read back as the maximum values */
void BochsDriver::ReadCaps()
{
	uint16 nEnable = ReadReg( VBE_DISPI_INDEX_ENABLE );

	WriteReg( VBE_DISPI_INDEX_ENABLE, nEnable | VBE_DISPI_GETCAPS );
	m_nMaxWidth = ReadReg( VBE_DISPI_INDEX_XRES );
	m_nMaxHeight = ReadReg( VBE_DISPI_INDEX_YRES );
	m_nMaxBpp = ReadReg( VBE_DISPI_INDEX_BPP );
	WriteReg( VBE_DISPI_INDEX_ENABLE, nEnable | ( ( nEnable & VBE_DISPI_ENABLED ) ? VBE_DISPI_NOCLEARMEM : 0 ) );

	/* Very old Bochs/QEMU versions don't support GETCAPS */
	if( m_nMaxWidth < 640 || m_nMaxHeight < 480 )
	{
		m_nMaxWidth = 1600;
		m_nMaxHeight = 1200;
	}
	if( m_nMaxBpp < 16 )
		m_nMaxBpp = 32;
}

void BochsDriver::AddMode( int nWidth, int nHeight )
{
	static const color_space aeSpaces[] = { CS_RGB16, CS_RGB32 };

	/* QEMU ignores an XRES that isn't a multiple of 8 */
	if( ( nWidth & 7 ) != 0 || nWidth > m_nMaxWidth || nHeight > m_nMaxHeight )
		return;

	for( uint i = 0; i < sizeof( aeSpaces ) / sizeof( aeSpaces[0] ); ++i )
	{
		int nBpp = bytes_per_pixel( aeSpaces[i] );

		if( nBpp * 8 > m_nMaxBpp )
			continue;
		if( ( uint32 )nWidth * nHeight * nBpp > m_nScreenLimit )
			continue;

		bool bDuplicate = false;
		for( uint j = 0; j < m_cModes.size(); ++j )
		{
			if( m_cModes[j].m_nWidth == nWidth && m_cModes[j].m_nHeight == nHeight && m_cModes[j].m_eColorSpace == aeSpaces[i] )
				bDuplicate = true;
		}
		if( bDuplicate )
			continue;

		os::screen_mode sMode( nWidth, nHeight, nWidth * nBpp, aeSpaces[i], 60.0f );
		sMode.m_vHPos = sMode.m_vVPos = sMode.m_vHSize = sMode.m_vVSize = 0.0f;
		m_cModes.push_back( sMode );

		uint32 nSize = ( ( uint32 )nWidth * nHeight * nBpp + 0xffff ) & ~0xffff;
		if( nSize > m_nScreenReserve )
			m_nScreenReserve = nSize;
	}
}

/*
 * QEMU puts an EDID block at the start of the MMIO BAR (edid=on, the default
 * since QEMU 5.0).  Its first detailed timing is the preferred resolution,
 * set with -device VGA,xres=...,yres=...
 */
void BochsDriver::AddEdidMode()
{
	static const uint8 anHeader[8] = { 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00 };
	uint8 anEdid[128];

	if( m_pMmio == NULL )
		return;

	for( int i = 0; i < 128; ++i )
		anEdid[i] = mmio_read8( m_pMmio, BOCHS_MMIO_EDID + i );

	if( memcmp( anEdid, anHeader, sizeof( anHeader ) ) != 0 )
		return;

	const uint8 *pDtd = &anEdid[54];
	if( pDtd[0] == 0 && pDtd[1] == 0 )	/* Not a timing descriptor */
		return;

	int nWidth = pDtd[2] | ( ( pDtd[4] & 0xf0 ) << 4 );
	int nHeight = pDtd[5] | ( ( pDtd[7] & 0xf0 ) << 4 );

	nWidth &= ~7;
	if( nWidth >= 640 && nHeight >= 480 )
	{
		dbprintf( "Bochs: EDID preferred mode %dx%d\n", nWidth, nHeight );
		AddMode( nWidth, nHeight );
	}
}

void BochsDriver::BuildModeList()
{
	static const struct { int w, h; } asSizes[] = {
		{ 640, 480 }, { 800, 600 }, { 1024, 768 }, { 1152, 864 },
		{ 1280, 720 }, { 1280, 768 }, { 1280, 800 }, { 1280, 960 }, { 1280, 1024 },
		{ 1360, 768 }, { 1400, 1050 }, { 1440, 900 }, { 1600, 900 }, { 1600, 1200 },
		{ 1680, 1050 }, { 1920, 1080 }, { 1920, 1200 }, { 2048, 1152 },
		{ 2560, 1080 }, { 2560, 1440 }, { 2560, 1600 }, { 3440, 1440 }, { 3840, 2160 },
	};

	/*
	 * With 32 MB or more, offer only modes that fit in half of video memory,
	 * so that the other half is left for window backbuffers.  With less
	 * there's not enough for both, so offer everything that fits.
	 */
	m_nScreenLimit = m_nVramSize;
#if BOCHS_BACKBUFFERS
	if( m_nVramSize >= 32 * 1024 * 1024 )
		m_nScreenLimit = m_nVramSize / 2;
#endif

	m_nScreenReserve = 0;
	for( uint i = 0; i < sizeof( asSizes ) / sizeof( asSizes[0] ); ++i )
		AddMode( asSizes[i].w, asSizes[i].h );
	AddEdidMode();
}

area_id BochsDriver::Open()
{
	return ( m_hFbArea );
}

void BochsDriver::Close()
{
	WriteReg( VBE_DISPI_INDEX_ENABLE, VBE_DISPI_DISABLED );
}

int BochsDriver::GetScreenModeCount()
{
	return ( m_cModes.size() );
}

bool BochsDriver::GetScreenModeDesc( int nIndex, os::screen_mode *psMode )
{
	if( nIndex < 0 || uint( nIndex ) >= m_cModes.size() )
		return ( false );

	*psMode = m_cModes[nIndex];
	return ( true );
}

os::screen_mode BochsDriver::GetCurrentScreenMode()
{
	return ( m_sCurrentMode );
}

/*
 * Any size is accepted, not only the listed ones, so a custom resolution in
 * the appserver config works as long as it fits.
 */
int BochsDriver::SetScreenMode( os::screen_mode sMode )
{
	int nBpp = bytes_per_pixel( sMode.m_eColorSpace );

	if( nBpp == 0 )
	{
		dbprintf( "Bochs: Unsupported colour space %d\n", sMode.m_eColorSpace );
		return ( -1 );
	}
	if( sMode.m_nWidth < 320 || sMode.m_nHeight < 200 || ( sMode.m_nWidth & 7 ) != 0 ||
		sMode.m_nWidth > m_nMaxWidth || sMode.m_nHeight > m_nMaxHeight || nBpp * 8 > m_nMaxBpp )
	{
		dbprintf( "Bochs: Mode %dx%d %d bpp is not supported\n", sMode.m_nWidth, sMode.m_nHeight, nBpp * 8 );
		return ( -1 );
	}
	if( ( uint32 )sMode.m_nWidth * sMode.m_nHeight * nBpp > m_nScreenReserve )
	{
		dbprintf( "Bochs: Mode %dx%d %d bpp needs more than the %u KB of video memory kept for the screen\n",
			sMode.m_nWidth, sMode.m_nHeight, nBpp * 8, ( uint )( m_nScreenReserve / 1024 ) );
		return ( -1 );
	}

	/* The same sequence as Linux's bochs driver */
	WriteReg( VBE_DISPI_INDEX_ENABLE, VBE_DISPI_DISABLED );
	WriteReg( VBE_DISPI_INDEX_BPP, nBpp * 8 );
	WriteReg( VBE_DISPI_INDEX_XRES, sMode.m_nWidth );
	WriteReg( VBE_DISPI_INDEX_YRES, sMode.m_nHeight );
	WriteReg( VBE_DISPI_INDEX_BANK, 0 );
	WriteReg( VBE_DISPI_INDEX_VIRT_WIDTH, sMode.m_nWidth );
	WriteReg( VBE_DISPI_INDEX_VIRT_HEIGHT, sMode.m_nHeight );
	WriteReg( VBE_DISPI_INDEX_X_OFFSET, 0 );
	WriteReg( VBE_DISPI_INDEX_Y_OFFSET, 0 );
	WriteReg( VBE_DISPI_INDEX_ENABLE, VBE_DISPI_ENABLED | VBE_DISPI_LFB_ENABLED );

	int nWidth = ReadReg( VBE_DISPI_INDEX_XRES );
	int nHeight = ReadReg( VBE_DISPI_INDEX_YRES );
	int nBits = ReadReg( VBE_DISPI_INDEX_BPP );
	int nVirtWidth = ReadReg( VBE_DISPI_INDEX_VIRT_WIDTH );

	if( nWidth != sMode.m_nWidth || nHeight != sMode.m_nHeight || nBits != nBpp * 8 || nVirtWidth < nWidth )
	{
		dbprintf( "Bochs: The device refused %dx%d %d bpp (got %dx%d %d bpp, virtual width %d)\n",
			sMode.m_nWidth, sMode.m_nHeight, nBpp * 8, nWidth, nHeight, nBits, nVirtWidth );
		return ( -1 );
	}

	/*
	 * Std VGA clears the screen on enable, but bochs-display doesn't, and its
	 * firmware (GRUB, SeaBIOS) has drawn into this memory at another size.
	 */
	memset( m_pFb, 0, ( size_t )sMode.m_nHeight * nVirtWidth * nBpp );

	Unblank();

	m_sCurrentMode = sMode;
	m_sCurrentMode.m_nBytesPerLine = nVirtWidth * nBpp;
	m_sCurrentMode.m_vRefreshRate = 60.0f;

	dbprintf( "Bochs: Set %dx%d %d bpp, %d bytes per line\n", nWidth, nHeight, nBits, m_sCurrentMode.m_nBytesPerLine );
	return ( 0 );
}

extern "C" DisplayDriver *init_gfx_driver( int nFd )
{
	try
	{
		BochsDriver *pcDriver = new BochsDriver( nFd );

		if( pcDriver->IsInitiated() )
			return ( pcDriver );

		delete pcDriver;
		return ( NULL );
	}
	catch( ... )
	{
		dbprintf( "Bochs: Got an exception during initialisation\n" );
		return ( NULL );
	}
}
