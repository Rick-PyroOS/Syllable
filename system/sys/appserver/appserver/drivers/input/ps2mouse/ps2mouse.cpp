
/*
 *  The Atheos application server
 *  Copyright (C) 1999 - 2001 Kurt Skauen
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of version 2 of the GNU Library
 *  General Public License as published by the Free Software
 *  Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 *  2003-11-18 - Damien Danneels <ddanneels@free.fr>
 *   improved protocol detection (based on GPM for linux)
 *
 *  2001-08-11 - Intellimouse mouse-wheel support added by a patch
 *  from Catalin Climov <xxl@climov.com>
 */



#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <errno.h>
#include <string.h>

#include <inputnode.h>

#include <appserver/protocol.h>
#include <util/message.h>
#include <atheos/kernel.h>

using namespace os;

#define GPM_AUX_SEND_ID    0xF2
#define GPM_AUX_ID_ERROR   -1
#define GPM_AUX_ID_PS2     0
#define GPM_AUX_ID_IMPS2   3
#define GPM_AUX_SET_RES        0xE8	/* Set resolution */
#define GPM_AUX_SET_SCALE11    0xE6	/* Set 1:1 scaling */
#define GPM_AUX_SET_SCALE21    0xE7	/* Set 2:1 scaling */
#define GPM_AUX_GET_SCALE      0xE9	/* Get scaling factor */
#define GPM_AUX_SET_STREAM     0xEA	/* Set stream mode */
#define GPM_AUX_SET_SAMPLE     0xF3	/* Set sample rate */
#define GPM_AUX_ENABLE_DEV     0xF4	/* Enable aux device */
#define GPM_AUX_DISABLE_DEV    0xF5	/* Disable aux device */
#define GPM_AUX_RESET          0xFF	/* Reset aux device */
#define GPM_AUX_ACK            0xFA	/* Command byte ACK. */


// (xxl) The PS/2 protocol (regular PS/2 mice)
#define PS2_PROTOCOL 1
// (xxl) The IMPS/2 protocol (IntelliMouse)
#define IMPS2_PROTOCOL 2
// (xxl) The number of "out of sync"'s needed in order to switch
// the mouse protocol. 2 seems a good default
#define SYNC_THRESHOLD 2

static int g_nSerialDevice = -1;
static int g_nProtocol = PS2_PROTOCOL;

/*
 * VMware's absolute pointer ("vmmouse"), which QEMU's pc machine also has
 * (vmport).  It is driven through the VMware backdoor I/O port, and the
 * hypervisor still raises a PS/2 interrupt (an empty packet) for each event,
 * so it rides on this driver: once it is on, each PS/2 packet only tells us
 * to read the pointer's absolute position from the backdoor.  The protocol
 * is the one in Linux's drivers/input/mouse/vmmouse.c.  When there is no
 * hypervisor, or it refuses, the mouse stays plain PS/2.
 */
#define VMMOUSE_MAGIC			0x564D5868
#define VMMOUSE_PORT			0x5658

#define VMMOUSE_CMD_GETVERSION		10
#define VMMOUSE_CMD_ABSPOINTER_DATA	39
#define VMMOUSE_CMD_ABSPOINTER_STATUS	40
#define VMMOUSE_CMD_ABSPOINTER_COMMAND	41

#define VMMOUSE_ENABLE			0x45414552
#define VMMOUSE_DISABLE			0x000000f5
#define VMMOUSE_REQUEST_ABSOLUTE	0x53424152
#define VMMOUSE_VERSION_ID		0x3442554a
#define VMMOUSE_ERROR			0xffff0000
#define VMMOUSE_RELATIVE_PACKET		0x00010000

#define VMMOUSE_LEFT_BUTTON		0x20
#define VMMOUSE_RIGHT_BUTTON		0x10
#define VMMOUSE_MIDDLE_BUTTON		0x08

static bool g_bVMMouse = false;

/* One backdoor call: EAX magic, EBX argument, ECX command, EDX port; the
 * reply comes back in all four.  EBX is swapped through ESI by hand, as it
 * may be the PIC register. */
static void vmmouse_cmd( uint32 nCmd, uint32 nArg, uint32 *pnA, uint32 *pnB, uint32 *pnC, uint32 *pnD )
{
	uint32 nA = VMMOUSE_MAGIC, nB = nArg, nC = nCmd, nD = VMMOUSE_PORT;

	__asm__ __volatile__( "xchgl %%ebx, %%esi\n\t"
			      "inl %%dx, %%eax\n\t"
			      "xchgl %%ebx, %%esi"
			      : "+a"( nA ), "+S"( nB ), "+c"( nC ), "+d"( nD ) : : "memory" );
	if( pnA ) *pnA = nA;
	if( pnB ) *pnB = nB;
	if( pnC ) *pnC = nC;
	if( pnD ) *pnD = nD;
}

/* Is there a VMware-compatible hypervisor?  On real hardware the port
 * reads as junk and EBX keeps the argument, so the magic can't match. */
static bool vmmouse_present()
{
	uint32 nVersion, nMagic;

	vmmouse_cmd( VMMOUSE_CMD_GETVERSION, 0, &nVersion, &nMagic, NULL, NULL );
	return ( nMagic == VMMOUSE_MAGIC && nVersion != 0xffffffff );
}

/* Switch the hypervisor's pointer to absolute mode; false if it refuses */
static bool vmmouse_enable()
{
	uint32 nStatus, nVersion;

	vmmouse_cmd( VMMOUSE_CMD_ABSPOINTER_COMMAND, VMMOUSE_ENABLE, NULL, NULL, NULL, NULL );
	vmmouse_cmd( VMMOUSE_CMD_ABSPOINTER_STATUS, 0, &nStatus, NULL, NULL, NULL );
	if( ( nStatus & VMMOUSE_ERROR ) == VMMOUSE_ERROR || ( nStatus & 0x0000ffff ) == 0 )
		return ( false );
	vmmouse_cmd( VMMOUSE_CMD_ABSPOINTER_DATA, 1, &nVersion, NULL, NULL, NULL );
	if( nVersion != VMMOUSE_VERSION_ID )
	{
		vmmouse_cmd( VMMOUSE_CMD_ABSPOINTER_COMMAND, VMMOUSE_DISABLE, NULL, NULL, NULL, NULL );
		return ( false );
	}
	vmmouse_cmd( VMMOUSE_CMD_ABSPOINTER_COMMAND, VMMOUSE_REQUEST_ABSOLUTE, NULL, NULL, NULL, NULL );
	return ( true );
}

static void vmmouse_disable()
{
	vmmouse_cmd( VMMOUSE_CMD_ABSPOINTER_COMMAND, VMMOUSE_DISABLE, NULL, NULL, NULL, NULL );
}

class PS2MouseDriver:public InputNode
{
      public:
	PS2MouseDriver();
	~PS2MouseDriver();

	virtual bool Start();
	virtual int GetType()
	{
		return ( IN_MOUSE );
	}

      private:
	static int32 EventLoopEntry( void *pData );
	void EventLoop();

	// (xxl) Added vertical and horizontal scroll parameters
	// Currently only the vertical scroll is implemented
	// bAbsolute: nDeltaX and nDeltaY are an absolute position, 0-65535
	void DispatchEvent( int nDeltaX, int nDeltaY, uint32 nButtons, int nVScroll, int nHScroll, bool bAbsolute = false );
	void ReadVMMouse();
	void SwitchToProtocol( int nProto );

	thread_id m_hThread;
	int m_nMouseDevice;

	// (xxl) protocol type (PS2_PROTOCOL or IMPS2_PROTOCOL)
	int m_nProtocol;

	// (xxl) the number of bytes in the protocol:
	// 3 for PS/2 and 4 for IMPS/2
	int m_nProtoByteCount;

	// (xxl) "out of sync" countdown
	int m_nSyncCount;
};



//----------------------------------------------------------------------------
// NAME:
// DESC:
// NOTE:
// SEE ALSO:
//----------------------------------------------------------------------------

PS2MouseDriver::PS2MouseDriver()
{
	/* Switch to the protocol detected by init_input_node() */
	SwitchToProtocol( g_nProtocol );
}

//----------------------------------------------------------------------------
// NAME:
// DESC:
// NOTE:
// SEE ALSO:
//----------------------------------------------------------------------------

PS2MouseDriver::~PS2MouseDriver()
{
}

//----------------------------------------------------------------------------
// NAME:
// DESC:
// NOTE:
// SEE ALSO:
//----------------------------------------------------------------------------

void PS2MouseDriver::DispatchEvent( int nDeltaX, int nDeltaY, uint32 nButtons, int nVScroll, int nHScroll, bool bAbsolute )
{
	Point cDeltaMove( nDeltaX, nDeltaY );
	static uint32 nLastButtons = 0;
	static Point cLastAbsPos( -1, -1 );
	uint32 nButtonFlg;

	/* An absolute position moves the pointer before any button changes, so
	 * a click lands where it was made */
	if( bAbsolute && cDeltaMove != cLastAbsPos ) {
		Message *pcEvent = new Message( M_MOUSE_MOVED );

		cLastAbsPos = cDeltaMove;
		pcEvent->AddPoint( "abs_pos", cDeltaMove );
		EnqueueEvent( pcEvent );
	}

	nButtonFlg = nButtons ^ nLastButtons;
	nLastButtons = nButtons;

	if( nButtonFlg != 0 ) {
		Message *pcEvent;

		if( nButtonFlg & 0x01 ) {
			if( nButtons & 0x01 ) {
				pcEvent = new Message( M_MOUSE_DOWN );
			} else {
				pcEvent = new Message( M_MOUSE_UP );
			}
			pcEvent->AddInt32( "_button", 1 );
			pcEvent->AddInt32( "_buttons", 1 );	// To be removed
			EnqueueEvent( pcEvent );
		}
		if( nButtonFlg & 0x02 ) {
			if( nButtons & 0x02 ) {
				pcEvent = new Message( M_MOUSE_DOWN );
			} else {
				pcEvent = new Message( M_MOUSE_UP );
			}
			pcEvent->AddInt32( "_button", 2 );
			pcEvent->AddInt32( "_buttons", 2 );	// To be removed
			EnqueueEvent( pcEvent );
		}
		// (xxl) Middle mouse button
		if( nButtonFlg & 0x04 ) {
			if( nButtons & 0x04 ) {
				pcEvent = new Message( M_MOUSE_DOWN );
			} else {
				pcEvent = new Message( M_MOUSE_UP );
			}
			pcEvent->AddInt32( "_button", 3 );
			pcEvent->AddInt32( "_buttons", 3 );	// To be removed
			EnqueueEvent( pcEvent );
		}
	}
	if( !bAbsolute && ( nDeltaX != 0 || nDeltaY != 0 ) ) {
		Message *pcEvent = new Message( M_MOUSE_MOVED );

		pcEvent->AddPoint( "delta_move", cDeltaMove );
		EnqueueEvent( pcEvent );
	}
	// (xxl) Vertical and/or horizontal scroll
	if( nVScroll != 0 || nHScroll != 0 ) {
		Point cScroll( nHScroll, nVScroll );

		// send a specific scroll message: M_MOUSE_SCROLL
		Message *pcEvent = new Message( M_WHEEL_MOVED );

		// the "delta_move" key contains the scroll amount
		// as a os::Point structure ("x" for horizontal and
		// "y" for vertical). Usually the scroll amount is
		// either -1, 0 or 1, but I noticed that sometimes
		// the mouse send other values as well (-2 and 2 are
		// the most common).
		pcEvent->AddPoint( "delta_move", cScroll );
		EnqueueEvent( pcEvent );
	}
}

//----------------------------------------------------------------------------
// NAME:
// DESC: Read every event queued in the vmmouse and dispatch it
// NOTE:
// SEE ALSO:
//----------------------------------------------------------------------------

void PS2MouseDriver::ReadVMMouse()
{
	/* Bounded, as Linux does, so a confused hypervisor can't hold us here */
	for( int nPackets = 0; nPackets < 32; nPackets++ ) {
		uint32 nStatus, nX, nY, nZ;

		vmmouse_cmd( VMMOUSE_CMD_ABSPOINTER_STATUS, 0, &nStatus, NULL, NULL, NULL );
		if( ( nStatus & VMMOUSE_ERROR ) == VMMOUSE_ERROR ) {
			dbprintf( "ps2mouse: vmmouse error, enabling it again\n" );
			if( !vmmouse_enable() ) {
				dbprintf( "ps2mouse: vmmouse lost, back to the plain PS/2 mouse\n" );
				g_bVMMouse = false;
			}
			return;
		}
		uint32 nQueued = nStatus & 0x0000ffff;
		if( nQueued < 4 )
			return;

		vmmouse_cmd( VMMOUSE_CMD_ABSPOINTER_DATA, 4, &nStatus, &nX, &nY, &nZ );

		uint32 nButtons = 0;
		if( nStatus & VMMOUSE_LEFT_BUTTON )
			nButtons |= 0x01;
		if( nStatus & VMMOUSE_RIGHT_BUTTON )
			nButtons |= 0x02;
		if( nStatus & VMMOUSE_MIDDLE_BUTTON )
			nButtons |= 0x04;
		/* The wheel has the PS/2 packet's sign: a signed byte */
		int nVScroll = ( int8 )( nZ & 0xff );

		if( nStatus & VMMOUSE_RELATIVE_PACKET )
			DispatchEvent( ( int32 )nX, ( int32 )nY, nButtons, nVScroll, 0 );
		else
			DispatchEvent( nX & 0xffff, nY & 0xffff, nButtons, nVScroll, 0, true );
	}
}

//----------------------------------------------------------------------------
// NAME:
// DESC:
// NOTE:
// SEE ALSO:
//----------------------------------------------------------------------------

void PS2MouseDriver::EventLoop()
{
	char anBuf[4];
	int nIndex = 0;

	for( ;; ) {
		uint8 nByte;

		if( read( g_nSerialDevice, &nByte, 1 ) != 1 ) {
			dbprintf( "Error: PS2MouseDriver::EventLoop() failed to read from serial device\n" );
			continue;
		}

		if( nIndex == 0 ) {
			// (xxl) The driver chokes on this test if you enable the
			// middle mouse button.
			//if ( (nByte & 0x08) == 0 || (nByte & 0x04) != 0 ) {
			if( ( nByte & 0x08 ) == 0 ) {
				// (xxl) try to switch the mouse protocol on every
				// SYNC_THRESHOLD "out of sync"'s
				// not very smart, but good enough
				m_nSyncCount--;
				if( m_nSyncCount == 0 ) {
					int nProto = PS2_PROTOCOL + IMPS2_PROTOCOL - m_nProtocol;

					dbprintf( "Mouse out of sync. Switching protocol to: %d\n", nProto );
					SwitchToProtocol( nProto );
				}
				continue;	// Out of sync.
			}
		}
		anBuf[nIndex++] = nByte;
		// (xxl) The IntelliMouse uses a 4-byte protocol, as opposite to the
		// regular ps2 mouse, which uses 3-byte.
		// the 4th byte contains scroll information
		// please note that this driver is (yet) incompatible with the
		// regular ps2 mice - I plan to add some sort of autodection ASAP
		// and make it work with both ps2 and imps2 (IntelliMouse)
		if( nIndex >= m_nProtoByteCount && g_bVMMouse ) {
			/* The packet only signals that the vmmouse has events */
			ReadVMMouse();
			nIndex = 0;
		} else if( nIndex >= m_nProtoByteCount ) {
			int x = anBuf[1];
			int y = anBuf[2];
			int nVScroll = 0;

			if( anBuf[0] & 0x10 ) {
				x |= 0xffffff00;
			}
			if( anBuf[0] & 0x20 ) {
				y |= 0xffffff00;
			}
			uint32 nButtons = 0;

			if( anBuf[0] & 0x01 ) {
				nButtons |= 0x01;
			}
			if( anBuf[0] & 0x02 ) {
				nButtons |= 0x02;
			}
			// (xxl) check the middle button
			if( anBuf[0] & 0x04 ) {
				nButtons |= 0x04;
			}
			if( m_nProtoByteCount > 3 ) {
				// (xxl) scroll wheel - vertical
				if( anBuf[3] != 0 ) {
					// if the most significant bit is set, we're most likely
					// dealing with a negative value
//                  if( anBuf[3] & 0x80 )
					// zzz - check if this is how to transform
					// 0..255 to -127..127
//                      nVScroll = anBuf[3] - 256;
//                  else
					nVScroll = anBuf[3];
				}
			} else
				nVScroll = 0;
			DispatchEvent( x, -y, nButtons, nVScroll, 0 );
			nIndex = 0;
		}
	}
}

//----------------------------------------------------------------------------
// NAME:
// DESC:
// NOTE:
// SEE ALSO:
//----------------------------------------------------------------------------

int32 PS2MouseDriver::EventLoopEntry( void *pData )
{
	PS2MouseDriver *pcThis = ( PS2MouseDriver * ) pData;

	pcThis->EventLoop();
	return ( 0 );
}

//----------------------------------------------------------------------------
// NAME:
// DESC:
// NOTE:
// SEE ALSO:
//----------------------------------------------------------------------------

bool PS2MouseDriver::Start()
{
	thread_id hEventThread;

	hEventThread = spawn_thread( "ps2mouse_event_thread", (void*)EventLoopEntry, 120, 0, this );
	resume_thread( hEventThread );
	return ( true );
}

//----------------------------------------------------------------------------
// NAME:
// DESC:
// NOTE:
// SEE ALSO:
//----------------------------------------------------------------------------

void PS2MouseDriver::SwitchToProtocol( int nProto )
{
	m_nSyncCount = SYNC_THRESHOLD;
	switch ( nProto ) {
	case PS2_PROTOCOL:
		m_nProtocol = nProto;
		m_nProtoByteCount = 3;
		break;
	case IMPS2_PROTOCOL:
		m_nProtocol = nProto;
		m_nProtoByteCount = 4;
		break;
	default:
		dbprintf( "Unknown mouse protocol requested: %d\n", nProto );
		break;
	}
}



/*
 * Sends the SEND_ID command to the ps2-type mouse.
 * Return one of GPM_AUX_ID_...
 */
static int read_mouse_id( int fd )
{
	unsigned char c = GPM_AUX_SEND_ID;
	unsigned char id;

	write( fd, &c, 1 );
	read( fd, &c, 1 );
	if( c != GPM_AUX_ACK ) {
		return ( GPM_AUX_ID_ERROR );
	}
	read( fd, &id, 1 );

	return ( id );
}

/**
 * Writes the given data to the ps2-type mouse.
 * Checks for an ACK from each byte.
 * 
 * Returns 0 if OK, or >0 if 1 or more errors occurred.
 */
static int write_to_mouse( int fd, unsigned char *data, size_t len )
{
	size_t i;
	int error = 0;

	for( i = 0; i < len; i++ ) {
		unsigned char c;

		write( fd, &data[i], 1 );
		read( fd, &c, 1 );
		if( c != GPM_AUX_ACK )
			error++;
	}

	/* flush any left-over input */
	usleep( 30000 );
	tcflush( fd, TCIFLUSH );
	return ( error );
}


//----------------------------------------------------------------------------
// NAME:
// DESC:
// NOTE:
// SEE ALSO:
//----------------------------------------------------------------------------

extern "C" bool init_input_node()
{
	int id;
	static unsigned char basic_init[] = { GPM_AUX_ENABLE_DEV, GPM_AUX_SET_SAMPLE, 100 };
	static unsigned char imps2_init[] = { GPM_AUX_SET_SAMPLE, 200, GPM_AUX_SET_SAMPLE, 100, GPM_AUX_SET_SAMPLE, 80, };
	static unsigned char ps2_init[] = { GPM_AUX_SET_SCALE11, GPM_AUX_ENABLE_DEV, GPM_AUX_SET_SAMPLE, 100, GPM_AUX_SET_RES, 3, };
	static unsigned char enable[] = { GPM_AUX_ENABLE_DEV, };

	g_nSerialDevice = open( "/dev/misc/ps2aux", O_RDWR );
	if( g_nSerialDevice < 0 ) {
		dbprintf( "No PS2 pointing device found\n" );
		return ( false );
	}
	/* dbprintf( "Found PS2 pointing device. Init mouse...\n" ); */

	/* Do a basic init in case the mouse is confused */
	write_to_mouse( g_nSerialDevice, basic_init, sizeof( basic_init ) );

	/* Now try again and make sure we have a PS/2 mouse */
	if( write_to_mouse( g_nSerialDevice, basic_init, sizeof( basic_init ) ) != 0 ) {
		dbprintf( "PS2 mouse init failed !\n" );
		return ( false );
	}

	/* Try to switch to 3 button mode */
	if( write_to_mouse( g_nSerialDevice, imps2_init, sizeof( imps2_init ) ) != 0 ) {
		dbprintf( "Switch to 3 button mode failed !\n" );
		return ( false );
	}

	/* Read the mouse id */
	id = read_mouse_id( g_nSerialDevice );
	if( id == GPM_AUX_ID_ERROR ) {
		dbprintf( "Read mouse id failed !\n" );
		id = GPM_AUX_ID_PS2;
	}

	/* And do the real initialisation */
	if( write_to_mouse( g_nSerialDevice, ps2_init, sizeof( ps2_init ) ) != 0 ) {
		dbprintf( "PS2 mouse real init failed !\n" );
	}

	if( id == GPM_AUX_ID_IMPS2 ) {
		/* Really an intellipoint, so initialise 3 button mode (4 byte packets) */
		dbprintf( "IMPS2 mouse detected.\n" );
		g_nProtocol = IMPS2_PROTOCOL;
	} else {
		if( id != GPM_AUX_ID_PS2 ) {
			dbprintf( "PS2 mouse detected with unknown id (%d).\n", id );
		} else {
			dbprintf( "PS2 mouse detected.\n" );
		}
	}
	if( write_to_mouse( g_nSerialDevice, enable, sizeof( enable ) ) != 0 ) {
		dbprintf( "PS2 mouse: enabling failed !\n" );
		return ( false );
	}

	/* Under VMware or QEMU: the absolute pointer, so the pointer follows the
	 * host's (over VNC too) */
	if( vmmouse_present() ) {
		if( vmmouse_enable() ) {
			g_bVMMouse = true;
			dbprintf( "ps2mouse: VMware vmmouse absolute pointer enabled\n" );
		} else {
			dbprintf( "ps2mouse: VMware vmmouse found but could not be enabled, using plain PS/2\n" );
		}
	}
	return ( true );
}

//----------------------------------------------------------------------------
// NAME:
// DESC:
// NOTE:
// SEE ALSO:
//----------------------------------------------------------------------------

extern "C" int uninit_input_node()
{
	if( g_bVMMouse ) {
		vmmouse_disable();
		g_bVMMouse = false;
	}
	if( g_nSerialDevice >= 0 ) {
		close( g_nSerialDevice );
	}
	return ( 0 );
}

//----------------------------------------------------------------------------
// NAME:
// DESC:
// NOTE:
// SEE ALSO:
//----------------------------------------------------------------------------

extern "C" int get_node_count()
{
	return ( 1 );
}

//----------------------------------------------------------------------------
// NAME:
// DESC:
// NOTE:
// SEE ALSO:
//----------------------------------------------------------------------------

extern "C" InputNode * get_input_node( int nIndex )
{
	if( nIndex != 0 ) {
		dbprintf( "PS2 mouse driver: get_input_node() called with invalid index %d\n", nIndex );
		return ( NULL );
	}
	return ( new PS2MouseDriver() );
}
