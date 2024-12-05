
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
#include <string>

#include <sys/ioctl.h>

#include <util/message.h>
#include <util/messagequeue.h>

#include <os/kernel.h>

#include <servers/appserver/protocol.h>
#include <servers/inputserver/inputserverdevice.h>
#include <servers/inputserver/input.h>

using namespace os;

static int read_mouse_id( int fd );
static int write_to_mouse( int fd, unsigned char *data, size_t len );

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

static int g_nProtocol = PS2_PROTOCOL;

class PS2MouseDriver:public InputServerDevice
{
public:
	PS2MouseDriver();
	~PS2MouseDriver();
    
    virtual status_t	Control( const char *zName, uint32 nCommand, Message *pcMessage, void *pCookie = NULL );
	virtual status_t	InitCheck( void );
	virtual status_t	Start( const char *zName, void *pCookie = NULL );
	virtual status_t	StartAll( void );
	virtual status_t	Stop( const char *zName, void *pCookie = NULL );
	virtual status_t	StopAll( void );
	virtual bool		IsRunning( void ) const;
	virtual status_t	SystemShuttingDown( void );

private:
	static int32 EventLoopEntry( void *pData );
	void EventLoop();

	// (xxl) Added vertical and horizontal scroll parameters
	// Currently only the vertical scroll is implemented
	void DispatchEvent( int nDeltaX, int nDeltaY, uint32 nButtons, int nVScroll, int nHScroll );
	void SwitchToProtocol( int nProto );

	// (xxl) protocol type (PS2_PROTOCOL or IMPS2_PROTOCOL)
	int m_nProtocol;

	// (xxl) the number of bytes in the protocol:
	// 3 for PS/2 and 4 for IMPS/2
	int m_nProtoByteCount;

	// (xxl) "out of sync" countdown
	int m_nSyncCount;
    
    static bool			m_bActive;
    static thread_id	m_nThread;
    int					m_nMouseDevice;
	InputDeviceRef		*m_pcDevRef;
};


bool		PS2MouseDriver::m_bActive = false;
thread_id	PS2MouseDriver::m_nThread = 0;



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
	
	// register your device(s) with the input_server
	// the last device in the list should be NULL
	m_pcDevRef = new InputDeviceRef();
	
	m_pcDevRef->SetName("USB Pointing Device");
	m_pcDevRef->SetType(DEVICE_POINTING);
	m_pcDevRef->SetCookie((void*)"USB Pointing Device");
	
	AddDeviceRefs(m_pcDevRef);
	RegisterDevices();
}

//----------------------------------------------------------------------------
// NAME:
// DESC:
// NOTE:
// SEE ALSO:
//----------------------------------------------------------------------------

PS2MouseDriver::~PS2MouseDriver()
{
	close( m_nMouseDevice );
	
	// cleanup
	UnregisterDevices();
	RemoveDeviceRefs(m_pcDevRef);
	
	delete m_pcDevRef;
}

//----------------------------------------------------------------------------
// NAME:
// DESC:
// NOTE:
// SEE ALSO:
//----------------------------------------------------------------------------

void PS2MouseDriver::DispatchEvent( int nDeltaX, int nDeltaY, uint32 nButtons, int nVScroll, int nHScroll )
{
	Point cDeltaMove( nDeltaX, nDeltaY );
	static uint32 nLastButtons = 0;
	uint32 nButtonFlg;

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
			EnqueueMessage( pcEvent );
		}
		if( nButtonFlg & 0x02 ) {
			if( nButtons & 0x02 ) {
				pcEvent = new Message( M_MOUSE_DOWN );
			} else {
				pcEvent = new Message( M_MOUSE_UP );
			}
			pcEvent->AddInt32( "_button", 2 );
			pcEvent->AddInt32( "_buttons", 2 );	// To be removed
			EnqueueMessage( pcEvent );
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
			EnqueueMessage( pcEvent );
		}
	}
	if( nDeltaX != 0 || nDeltaY != 0 ) {
		Message *pcEvent = new Message( M_MOUSE_MOVED );

		pcEvent->AddPoint( "delta_move", cDeltaMove );
		EnqueueMessage( pcEvent );
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
		EnqueueMessage( pcEvent );
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

	while(m_bActive) {
		uint8 nByte;

		if( read( m_nMouseDevice, &nByte, 1 ) != 1 ) {
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
		if( nIndex >= m_nProtoByteCount ) {
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

status_t PS2MouseDriver::Control( const char *zName, uint32 nCommand, Message *pcMessage, void *pCookie )
{
}//----------------------------------------------------------------------------
// NAME:
// DESC:
// NOTE:
// SEE ALSO:
//----------------------------------------------------------------------------

status_t PS2MouseDriver::InitCheck( void )
{
	int id;
	static unsigned char basic_init[] = { GPM_AUX_ENABLE_DEV, GPM_AUX_SET_SAMPLE, 100 };
	static unsigned char imps2_init[] = { GPM_AUX_SET_SAMPLE, 200, GPM_AUX_SET_SAMPLE, 100, GPM_AUX_SET_SAMPLE, 80, };
	static unsigned char ps2_init[] = { GPM_AUX_SET_SCALE11, GPM_AUX_ENABLE_DEV, GPM_AUX_SET_SAMPLE, 100, GPM_AUX_SET_RES, 3, };

	m_nMouseDevice = open( "/dev/misc/ps2aux", O_RDWR );
	if( m_nMouseDevice < 0 ) {
		dbprintf( "No PS2 pointing device found\n" );
		return m_nMouseDevice;
	}
	/* dbprintf( "Found PS2 pointing device. Init mouse...\n" ); */

	/* Do a basic init in case the mouse is confused */
	write_to_mouse( m_nMouseDevice, basic_init, sizeof( basic_init ) );

	/* Now try again and make sure we have a PS/2 mouse */
	if( write_to_mouse( m_nMouseDevice, basic_init, sizeof( basic_init ) ) != 0 ) {
		dbprintf( "PS2 mouse init failed !\n" );
		return -EACCES;
	}

	/* Try to switch to 3 button mode */
	if( write_to_mouse( m_nMouseDevice, imps2_init, sizeof( imps2_init ) ) != 0 ) {
		dbprintf( "Switch to 3 button mode failed !\n" );
		return -EACCES;
	}

	/* Read the mouse id */
	id = read_mouse_id( m_nMouseDevice );
	if( id == GPM_AUX_ID_ERROR ) {
		dbprintf( "Read mouse id failed !\n" );
		id = GPM_AUX_ID_PS2;
	}

	/* And do the real initialisation */
	if( write_to_mouse( m_nMouseDevice, ps2_init, sizeof( ps2_init ) ) != 0 ) {
		dbprintf( "PS2 mouse real init failed !\n" );
	}

	if( id == GPM_AUX_ID_IMPS2 ) {
		/* Really an intellipoint, so initialise 3 button mode (4 byte packets) */
		dbprintf( "IMPS2 mouse detected.\n" );
	} else {
		if( id != GPM_AUX_ID_PS2 ) {
			dbprintf( "PS2 mouse detected with unknown id (%d).\n", id );
		} else {
			dbprintf( "PS2 mouse detected.\n" );
		}
	}
	return EOK;
}

//----------------------------------------------------------------------------
// NAME:
// DESC:
// NOTE:
// SEE ALSO:
//----------------------------------------------------------------------------

status_t PS2MouseDriver::Start( const char *zName, void *pCookie )
{
	thread_id hEventThread;

	hEventThread = spawn_thread( "ps2mouse_event_thread", (void*)EventLoopEntry, 120, 0, this );
	resume_thread( hEventThread );
    m_bActive = true;
	return EOK;
}

//----------------------------------------------------------------------------
// NAME:
// DESC:
// NOTE:
// SEE ALSO:
//----------------------------------------------------------------------------

status_t PS2MouseDriver::StartAll( void )
{
	return( Start(m_pcDevRef->GetName(), m_pcDevRef->GetCookie()));
}

//----------------------------------------------------------------------------
// NAME:
// DESC:
// NOTE:
// SEE ALSO:
//----------------------------------------------------------------------------

status_t PS2MouseDriver::Stop( const char *zName, void *pCookie )
{
	// stop generating events
	// this is a hook function, it is called for you
	// (you should not call it yourself)

	m_bActive = false;
	wait_for_thread(m_nThread);

	return EOK;
}

//----------------------------------------------------------------------------
// NAME:
// DESC:
// NOTE:
// SEE ALSO:
//----------------------------------------------------------------------------

status_t PS2MouseDriver::StopAll( void )
{
	return( Stop(m_pcDevRef->GetName(), m_pcDevRef->GetCookie()));
}

//----------------------------------------------------------------------------
// NAME:
// DESC:
// NOTE:
// SEE ALSO:
//----------------------------------------------------------------------------

bool PS2MouseDriver::IsRunning( void ) const
{
	return m_bActive;
}

//----------------------------------------------------------------------------
// NAME:
// DESC:
// NOTE:
// SEE ALSO:
//----------------------------------------------------------------------------

status_t PS2MouseDriver::SystemShuttingDown( void )
{
	// do any cleanup (ie. saving a settings file) when the
	// system is about to shut down

	return (InputServerDevice::SystemShuttingDown());
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

extern "C" InputServerDevice* init_isd_addon()
{
	CALLED();
		
	// this is where it all starts
	// make sure this function is exported!
	
    try {
		InputServerDevice* pcISD = new PS2MouseDriver();
		return( pcISD );
    } catch( std::exception&  cExc ) {
		return( NULL );
    }
}



