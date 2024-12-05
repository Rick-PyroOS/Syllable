/*
 *  The Syllable application server
 *	USB mouse appserver driver
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

class USBMouseDriver : public InputServerDevice
{
public:
    USBMouseDriver();
    ~USBMouseDriver();
    
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
    // (xxl) Added vertical and horizontal scroll parameters
    // Currently only the vertical scroll is implemented
    void DispatchEvent( int nDeltaX, int nDeltaY, uint32 nButtons, int nVScroll, int nHScroll );
    void SwitchToProtocol( int nProto );
    
    static bool			m_bActive;
    static thread_id	m_nThread;
    int					m_nMouseDevice;
	InputDeviceRef		*m_pcDevRef;
};


bool		USBMouseDriver::m_bActive = false;
thread_id	USBMouseDriver::m_nThread = 0;



//----------------------------------------------------------------------------
// NAME:
// DESC:
// NOTE:
// SEE ALSO:
//----------------------------------------------------------------------------

USBMouseDriver::USBMouseDriver()
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

USBMouseDriver::~USBMouseDriver()
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

void USBMouseDriver::DispatchEvent( int nDeltaX, int nDeltaY, uint32 nButtons, int nVScroll, int nHScroll )
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
        // (xxl) Middle mouse button
	if ( nButtonFlg & 0x04 ) {
	    if ( nButtons & 0x04 ) {
		pcEvent = new Message( M_MOUSE_DOWN );
	    } else {
		pcEvent = new Message( M_MOUSE_UP );
	    }
	    pcEvent->AddInt32( "_button", 3 );
	    pcEvent->AddInt32( "_buttons", 3 ); // To be removed
	    EnqueueMessage( pcEvent );
	}
    }
    if ( nDeltaX != 0 || nDeltaY != 0 ) {
	Message* pcEvent = new Message( M_MOUSE_MOVED );
	pcEvent->AddPoint( "delta_move", cDeltaMove );
	EnqueueMessage( pcEvent );
    }
    // (xxl) Vertical and/or horizontal scroll
    if( nVScroll !=0 || nHScroll != 0 ) {
       Point cScroll( nHScroll, nVScroll );
       // send a specific scroll message: M_MOUSE_SCROLL
       Message* pcEvent = new Message( M_WHEEL_MOVED );
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

void USBMouseDriver::EventLoop()
{
    char anBuf[4];
    int  nIndex = 0;
    uint32 nButtons = 0;
  
    while(m_bActive) {
    
	if ( read( m_nMouseDevice, &anBuf, 4 ) != 4 ) {
	    dbprintf( "Error: USBMouseDriver::EventLoop() failed to read from serial device\n" );
	    continue;
	}
	    int x = (int)((float)anBuf[1] * 1.5);
	    int y = (int)((float)anBuf[2] * 1.5);
	    int nVScroll = 0;
	    
	    nButtons = 0;
	    if ( anBuf[0] & 0x01 ) {
		nButtons |= 0x01;
	    }
	    if ( anBuf[0] & 0x02 ) {
		nButtons |= 0x02;
	    }
	      // (xxl) check the middle button
	    if ( anBuf[0] & 0x04 ) {
	        nButtons |= 0x04;
	    }
	   
		nVScroll = anBuf[3];
	    DispatchEvent( x, y, nButtons, -nVScroll, 0 );
	    nIndex = 0;
	    //snooze( 1000 );
    }
}

//----------------------------------------------------------------------------
// NAME:
// DESC:
// NOTE:
// SEE ALSO:
//----------------------------------------------------------------------------

int32 USBMouseDriver::EventLoopEntry( void* pData )
{
    USBMouseDriver* pcThis = (USBMouseDriver*) pData;
  
    pcThis->EventLoop();
    return( 0 );
}

//----------------------------------------------------------------------------
// NAME:
// DESC:
// NOTE:
// SEE ALSO:
//----------------------------------------------------------------------------

status_t USBMouseDriver::Control( const char *zName, uint32 nCommand, Message *pcMessage, void *pCookie )
{
}

//----------------------------------------------------------------------------
// NAME:
// DESC:
// NOTE:
// SEE ALSO:
//----------------------------------------------------------------------------

status_t USBMouseDriver::InitCheck( void )
{
    m_nMouseDevice = open( "/dev/input/usb_mouse", O_RDWR );
    if ( m_nMouseDevice < 0 ) {
	dbprintf( "No USB mouse device node found\n" );
	return m_nMouseDevice;
    }
    dbprintf( "Found USB mouse device node\n" );
   
    return EOK;
}

//----------------------------------------------------------------------------
// NAME:
// DESC:
// NOTE:
// SEE ALSO:
//----------------------------------------------------------------------------

status_t USBMouseDriver::Start( const char *zName, void *pCookie )
{
    thread_id hEventThread;
    hEventThread = spawn_thread( "usbmouse_event_thread", (void*)EventLoopEntry, 120, 0, this );
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

status_t USBMouseDriver::StartAll( void )
{
	return( Start(m_pcDevRef->GetName(), m_pcDevRef->GetCookie()));
}

//----------------------------------------------------------------------------
// NAME:
// DESC:
// NOTE:
// SEE ALSO:
//----------------------------------------------------------------------------

status_t USBMouseDriver::Stop( const char *zName, void *pCookie )
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

status_t USBMouseDriver::StopAll( void )
{
	return( Stop(m_pcDevRef->GetName(), m_pcDevRef->GetCookie()));
}

//----------------------------------------------------------------------------
// NAME:
// DESC:
// NOTE:
// SEE ALSO:
//----------------------------------------------------------------------------

bool USBMouseDriver::IsRunning( void ) const
{
	return m_bActive;
}

//----------------------------------------------------------------------------
// NAME:
// DESC:
// NOTE:
// SEE ALSO:
//----------------------------------------------------------------------------

status_t USBMouseDriver::SystemShuttingDown( void )
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
		InputServerDevice* pcISD = new USBMouseDriver();
		return( pcISD );
    } catch( std::exception&  cExc ) {
		return( NULL );
    }
}




