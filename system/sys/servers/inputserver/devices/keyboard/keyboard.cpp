/*
 *  The Syllable application server
 *  Standard keyboard appserver driver
 *  Copyright (C) 1999 - 2001 Kurt Skauen
 *  Copyright (C) 2005 Arno Klenke
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
#include <servers/inputserver/keyboard.h>

using namespace os;

class KeyboardDriver : public InputServerDevice
{
public:
    KeyboardDriver();
    ~KeyboardDriver();
    
    virtual status_t	Control( const char *zName, uint32 nCommand, Message *pcMessage, void *pCookie = NULL );
	virtual status_t	InitCheck( void );
	virtual status_t	Start( const char *zName, void *pCookie = NULL );
	virtual status_t	StartAll( void );
	virtual status_t	Stop( const char *zName, void *pCookie = NULL );
	virtual status_t	StopAll( void );
	virtual bool		IsRunning( void ) const;
	virtual status_t	SystemShuttingDown( void );
  
private:
	void SetLED( int nKeyCode );
    static int32 EventLoopEntry( void* pData );
    void EventLoop();
    void DispatchEvent( int nKeyCode );
    
    static bool			m_bActive;
    static thread_id	m_nThread;
    int					m_nDevice;
	InputDeviceRef		*m_pcDevRef;
};


bool		KeyboardDriver::m_bActive = false;
thread_id	KeyboardDriver::m_nThread = 0;


//----------------------------------------------------------------------------
// NAME:
// DESC:
// NOTE:
// SEE ALSO:
//----------------------------------------------------------------------------

KeyboardDriver::KeyboardDriver()
	: InputServerDevice()
{
	// register your device(s) with the input_server
	// the last device in the list should be NULL
	m_pcDevRef = new InputDeviceRef();
	
	m_pcDevRef->SetName("Keyboard Device");
	m_pcDevRef->SetType(DEVICE_KEYBOARD);
	m_pcDevRef->SetCookie((void*)"Keyboard Device");
	
	AddDeviceRefs(m_pcDevRef);
	RegisterDevices();
}

//----------------------------------------------------------------------------
// NAME:
// DESC:
// NOTE:
// SEE ALSO:
//----------------------------------------------------------------------------

KeyboardDriver::~KeyboardDriver()
{
	close( m_nDevice );
	
	// cleanup
	UnregisterDevices();
	RemoveDeviceRefs(m_pcDevRef);
	
	delete m_pcDevRef;
}


void KeyboardDriver::SetLED( int nKeyCode )
{
	switch ( nKeyCode )
	{
	case 0x3b:
		{
			ioctl( m_nDevice, IOCTL_KBD_CAPLOC );
			break;
		}

	case 0x22:
		{
			ioctl( m_nDevice, IOCTL_KBD_NUMLOC );
			break;
		}

	case 0x0f:
		{
			ioctl( m_nDevice, IOCTL_KBD_SCRLOC );
			break;
		}
	}
}

//----------------------------------------------------------------------------
// NAME:
// DESC:
// NOTE:
// SEE ALSO:
//----------------------------------------------------------------------------

void KeyboardDriver::DispatchEvent( int nKeyCode )
{
	SetLED( nKeyCode );
	Message* pcEvent = new Message( nKeyCode & 0x80 ? M_KEY_UP : M_KEY_DOWN );
	pcEvent->AddInt32( "_key", nKeyCode );
	EnqueueMessage( pcEvent );
}

//----------------------------------------------------------------------------
// NAME:
// DESC:
// NOTE:
// SEE ALSO:
//----------------------------------------------------------------------------

void KeyboardDriver::EventLoop()
{
    uint8 nKeyCode;
   
    while(m_bActive) {
    	snooze( 10000 );
		if ( read( m_nDevice, &nKeyCode, 1 ) == 1 ) {
		    DispatchEvent( nKeyCode );
    	}
    }
}

//----------------------------------------------------------------------------
// NAME:
// DESC:
// NOTE:
// SEE ALSO:
//----------------------------------------------------------------------------

int32 KeyboardDriver::EventLoopEntry( void* pData )
{
    KeyboardDriver* pcThis = (KeyboardDriver*) pData;
  
    pcThis->EventLoop();
    return( 0 );
}

//----------------------------------------------------------------------------
// NAME:
// DESC:
// NOTE:
// SEE ALSO:
//----------------------------------------------------------------------------

status_t KeyboardDriver::Control( const char *zName, uint32 nCommand, Message *pcMessage, void *pCookie )
{
}

//----------------------------------------------------------------------------
// NAME:
// DESC:
// NOTE:
// SEE ALSO:
//----------------------------------------------------------------------------

status_t KeyboardDriver::InitCheck( void )
{
	
	m_nDevice = open( "/dev/keybd", O_RDONLY );
	
	if(m_nDevice < 0)
		return m_nDevice;
	
	/* Reset keyboard */
	ioctl( m_nDevice, IOCTL_KBD_LEDRST );
	switch ( g_psKeymap->m_nLockSetting )
	{
	case 0x00:		// None
		break;

	case 0x01:		// Caps
		{
			ioctl( m_nDevice, IOCTL_KBD_CAPLOC );
			break;
		}

	case 0x02:		// Scroll
		{
			ioctl( m_nDevice, IOCTL_KBD_SCRLOC );
			break;
		}

	case 0x04:		// Num
		{
			ioctl( m_nDevice, IOCTL_KBD_NUMLOC );
			break;
		}

	default:
		dbprintf( "Unknown lock key 0x%2X\n", ( int )g_psKeymap->m_nLockSetting );
		break;
	}
	
	return EOK;
}

//----------------------------------------------------------------------------
// NAME:
// DESC:
// NOTE:
// SEE ALSO:
//----------------------------------------------------------------------------

status_t KeyboardDriver::Start( const char *zName, void *pCookie )
{
    thread_id hEventThread;
    hEventThread = spawn_thread( zName, (void*)EventLoopEntry, 120, 0, this );
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

status_t KeyboardDriver::StartAll( void )
{
	return( Start(m_pcDevRef->GetName(), m_pcDevRef->GetCookie()));
}

//----------------------------------------------------------------------------
// NAME:
// DESC:
// NOTE:
// SEE ALSO:
//----------------------------------------------------------------------------

status_t KeyboardDriver::Stop( const char *zName, void *pCookie )
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

status_t KeyboardDriver::StopAll( void )
{
	return( Stop(m_pcDevRef->GetName(), m_pcDevRef->GetCookie()));
}

//----------------------------------------------------------------------------
// NAME:
// DESC:
// NOTE:
// SEE ALSO:
//----------------------------------------------------------------------------

bool KeyboardDriver::IsRunning( void ) const
{
	return m_bActive;
}

//----------------------------------------------------------------------------
// NAME:
// DESC:
// NOTE:
// SEE ALSO:
//----------------------------------------------------------------------------

status_t KeyboardDriver::SystemShuttingDown( void )
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
		InputServerDevice* pcISD = new KeyboardDriver();
		return( pcISD );
    } catch( std::exception&  cExc ) {
		return( NULL );
    }
}



