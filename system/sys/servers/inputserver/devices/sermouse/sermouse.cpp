/*
 *  The AtheOS application server
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

class SerMouseDriver : public InputServerDevice
{
public:
    SerMouseDriver();
    ~SerMouseDriver();
    
    virtual status_t	Control( const char *zName, uint32 nCommand, Message *pcMessage, void *pCookie = NULL );
	virtual status_t	InitCheck( void );
	virtual status_t	Start( const char *zName, void *pCookie = NULL );
	virtual status_t	StartAll( void );
	virtual status_t	Stop( const char *zName, void *pCookie = NULL );
	virtual status_t	StopAll( void );
	virtual bool		IsRunning( void ) const;
	virtual status_t	SystemShuttingDown( void );
  
private:
    static int32 EventLoopEntry( void* pData );
    void EventLoop();
    void DispatchEvent( int nDeltaX, int nDeltaY, uint32 nButtons );
    
    static bool			m_bActive;
    static thread_id	m_nThread;
    int					m_nMouseDevice;
	InputDeviceRef		*m_pcDevRef;
};


bool		SerMouseDriver::m_bActive = false;
thread_id	SerMouseDriver::m_nThread = 0;



//----------------------------------------------------------------------------
// NAME:
// DESC:
// NOTE:
// SEE ALSO:
//----------------------------------------------------------------------------

SerMouseDriver::SerMouseDriver()
	: InputServerDevice()
{
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

SerMouseDriver::~SerMouseDriver()
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

void SerMouseDriver::DispatchEvent( int nDeltaX, int nDeltaY, uint32 nButtons )
{
    Point cDeltaMove( nDeltaX, nDeltaY );
    static uint32 nLastButtons = 0;
    uint32 	nButtonFlg;
  
    nButtonFlg	= nButtons ^ nLastButtons;
    nLastButtons	= nButtons;

    if ( nButtonFlg != 0 ) {
	Message* pcEvent;
    
	if ( nButtonFlg & 0x01 ) {
	    if ( nButtons & 0x01 ) {
		pcEvent = new Message( M_MOUSE_DOWN );
	    } else {
		pcEvent = new Message( M_MOUSE_UP );
	    }
	    pcEvent->AddInt32( "_button", 1 );
	    pcEvent->AddInt32( "_buttons", 1 ); // To be removed
	    EnqueueMessage( pcEvent );
	}
	if ( nButtonFlg & 0x02 ) {
	    if ( nButtons & 0x02 ) {
		pcEvent = new Message( M_MOUSE_DOWN );
	    } else {
		pcEvent = new Message( M_MOUSE_UP );
	    }
	    pcEvent->AddInt32( "_button", 2 );
	    pcEvent->AddInt32( "_buttons", 2 ); // To be removed
	    EnqueueMessage( pcEvent );
	}
    }
    if ( nDeltaX != 0 || nDeltaY != 0 ) {
	Message* pcEvent = new Message( M_MOUSE_MOVED );
	pcEvent->AddPoint( "delta_move", cDeltaMove );
	EnqueueMessage( pcEvent );
    }
}

//----------------------------------------------------------------------------
// NAME:
// DESC:
// NOTE:
// SEE ALSO:
//----------------------------------------------------------------------------

void SerMouseDriver::EventLoop()
{
    char anBuf[4];
    int  nIndex = 0;
  
    while(m_bActive) {
	uint8 nByte;
    
	if ( read( m_nMouseDevice, &nByte, 1 ) != 1 ) {
	    dbprintf( "Error: SerMouseDriver::EventLoop() failed to read from serial device\n" );
	    continue;
	}
	if ( nByte & 0x40 ) {
	    nIndex = 0;
	}
	if ( nIndex >= 4 ) {
	    continue;
	}
	anBuf[nIndex++] = nByte;

	if ( nIndex == 3 ) {
	    int x;
	    int y;
	    uint32 nButtons = 0;
      
	    x = (anBuf[1] & 0x3f) | ((anBuf[0] & 0x03) << 6 );
	    y = (anBuf[2] & 0x3f) | ((anBuf[0] & 0x0c) << 4 );
	    if ( x & 0x80 ) {
		x |= 0xffffff00;
	    }
	    if ( y & 0x80 ) {
		y |= 0xffffff00;
	    }
	    if ( (anBuf[0] & 0x10) ) {
		nButtons |= 0x02;
	    }
	    if ( (anBuf[0] & 0x20) ) {
		nButtons |= 0x01;
	    }
	    DispatchEvent( x, y, nButtons );
	}
    }
}

//----------------------------------------------------------------------------
// NAME:
// DESC:
// NOTE:
// SEE ALSO:
//----------------------------------------------------------------------------

int32 SerMouseDriver::EventLoopEntry( void* pData )
{
    SerMouseDriver* pcThis = (SerMouseDriver*) pData;
  
    pcThis->EventLoop();
    return( 0 );
}

//----------------------------------------------------------------------------
// NAME:
// DESC:
// NOTE:
// SEE ALSO:
//----------------------------------------------------------------------------

status_t SerMouseDriver::Control( const char *zName, uint32 nCommand, Message *pcMessage, void *pCookie )
{
}

//----------------------------------------------------------------------------
// NAME:
// DESC:
// NOTE:
// SEE ALSO:
//----------------------------------------------------------------------------

status_t SerMouseDriver::InitCheck( void )
{
    m_nMouseDevice = open( "/dev/misc/com/1", O_RDWR | O_NONBLOCK );
    if ( m_nMouseDevice < 0 ) {
	dbprintf( "Error: serial mode driver failed to open serial device: %s\n", strerror( errno ) );
	return m_nMouseDevice;
    }
    uint32 nValue;
      // DTR should always be set.
    nValue = TIOCM_DTR;
    ioctl( m_nMouseDevice, TIOCMBIS, &nValue );

    uint8 nByte;
      // Empty the input buffer.
    while( read( m_nMouseDevice, &nByte, 1 ) == 1 ) 
	  /*** EMPTY ***/;
      // Toggle the RTS line. This should cause the mouse to send an "M"
    nValue = TIOCM_RTS;
    ioctl( m_nMouseDevice, TIOCMBIS, &nValue );
    snooze( 100000 );
    ioctl( m_nMouseDevice, TIOCMBIC, &nValue );
    snooze( 100000 );
    ioctl( m_nMouseDevice, TIOCMBIS, &nValue );
    snooze( 100000 );

    bool bDetected = false;
  
    while( read( m_nMouseDevice, &nByte, 1 ) == 1 ) {
	if ( (nByte & 0x7f) == 'M' ) {
	    bDetected = true;
	}
    }
    close( m_nMouseDevice );

    if ( bDetected == false ) {
	return -ENXIO;
    }
  
    dbprintf( "Found serial mouse on port 2\n" );
      // Re-open the device in blocking mode
    m_nMouseDevice = open( "/dev/misc/com/1", O_RDWR );
    if ( m_nMouseDevice < 0 ) {
	dbprintf( "Error: serial mode driver failed to re-open serial device: %s\n", strerror( errno ) );
	return m_nMouseDevice;
    }
    return EOK;
}

//----------------------------------------------------------------------------
// NAME:
// DESC:
// NOTE:
// SEE ALSO:
//----------------------------------------------------------------------------

status_t SerMouseDriver::Start( const char *zName, void *pCookie )
{
    thread_id hEventThread;
    hEventThread = spawn_thread( "sermouse_event_thread", (void*)EventLoopEntry, 120, 0, this );
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

status_t SerMouseDriver::StartAll( void )
{
	return( Start(m_pcDevRef->GetName(), m_pcDevRef->GetCookie()));
}

//----------------------------------------------------------------------------
// NAME:
// DESC:
// NOTE:
// SEE ALSO:
//----------------------------------------------------------------------------

status_t SerMouseDriver::Stop( const char *zName, void *pCookie )
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

status_t SerMouseDriver::StopAll( void )
{
	return( Stop(m_pcDevRef->GetName(), m_pcDevRef->GetCookie()));
}

//----------------------------------------------------------------------------
// NAME:
// DESC:
// NOTE:
// SEE ALSO:
//----------------------------------------------------------------------------

bool SerMouseDriver::IsRunning( void ) const
{
	return m_bActive;
}

//----------------------------------------------------------------------------
// NAME:
// DESC:
// NOTE:
// SEE ALSO:
//----------------------------------------------------------------------------

status_t SerMouseDriver::SystemShuttingDown( void )
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

extern "C" InputServerDevice* init_isd_addon()
{
	CALLED();
		
	// this is where it all starts
	// make sure this function is exported!
	
    try {
		InputServerDevice* pcISD = new SerMouseDriver();
		return( pcISD );
    } catch( std::exception&  cExc ) {
		return( NULL );
    }
}


