/*
 *  The Syllable input server
 *	inputserverdevice.h - Copyright (C) 2008 Dee Sharpe
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

#ifndef __INPUT_SERVER_DEVICE_H__
#define __INPUT_SERVER_DEVICE_H__

#include <set>

#include <os/types.h>
#include <os/kernel.h>

#include <storage/path.h>

#include <util/message.h>
#include <util/string.h>

#include <servers/inputserver/input.h>
#include <servers/inputserver/inputservercontainer.h>

namespace os
{

typedef enum InputDeviceMessages_e
{
	/* Keyboard notifications */
	KEY_MAP_CHANGED = 1,
	KEY_LOCKS_CHANGED,
	KEY_REPEAT_DELAY_CHANGED,
	KEY_REPEAT_RATE_CHANGED,
	
	/* Mouse notifications */
	MOUSE_TYPE_CHANGED = 10,
	MOUSE_MAP_CHANGED,
	MOUSE_SPEED_CHANGED,
	CLICK_SPEED_CHANGED,
	MOUSE_ACCELERATION_CHANGED
} InputDeviceMessages_t;

class InputDeviceRef
{
public:
	InputDeviceRef( void );
	~InputDeviceRef( void );
	
	InputDeviceType_t	GetType( void ) const				{ CALLED();	return( m_eType );	}
	void				SetType( InputDeviceType_t eType )	{ CALLED();	m_eType = eType;		}
	
	char*				GetName( void ) const				{ CALLED();	return( m_zName );	}
	void				SetName( char *zName )				{ CALLED();	m_zName = zName;		}

	void*				GetCookie( void ) const				{ CALLED();	return( m_pCookie );	}
	void				SetCookie( void *pCookie )			{ CALLED();	m_pCookie = pCookie;	}

private:
	InputDeviceType_t m_eType;
	char *m_zName;
	void *m_pCookie;
};

class InputServerDevice
{
public:
	InputServerDevice( void );
	virtual ~InputServerDevice( void );

	virtual status_t	Control( const char *zName, uint32 nCommand, Message *pcMessage, void *pCookie = NULL );
	virtual status_t	InitCheck( void );
	virtual status_t	Start( const char *zName, void *pCookie = NULL );
	virtual status_t	Start( InputDeviceType_t eType, void *pCookie = NULL );
	virtual status_t	StartAll( void );
	virtual status_t	Stop( const char *zName, void *pCookie = NULL );
	virtual status_t	Stop( InputDeviceType_t eType, void *pCookie = NULL );
	virtual status_t	StopAll( void );
	virtual bool		IsRunning( void ) const;
	virtual status_t	SystemShuttingDown( void );
	
	status_t			ControlAll( InputDeviceType_t eType, const char *zName, uint32 nCommand, Message *pcMessage, void *pCookie = NULL );
	status_t			EnqueueMessage( Message *cMessage );
	status_t			RegisterDevices( void );
	status_t			UnregisterDevices( void );
	status_t			AddDeviceRefs( InputDeviceRef *pcDevice );
	status_t			RemoveDeviceRefs( InputDeviceRef *pcDevice );
	InputDeviceRef*		GetDeviceRef( const char *zName, InputDeviceType_t eType );
	bool				HandlesDevice( InputDeviceRef *pcDevice ) const;
	
	bool				HasDevice( const char *zName, InputDeviceType_t eType );
	
	status_t			StartMonitoringDevice( const char *zDevDir, InputDeviceRef *pcDevice );
	status_t			StopMonitoringDevice( const char *zDevDir, InputDeviceRef *pcDevice );
	
	String				GetPath( void ) const;
	void				SetPath( String& cPath );
	
	bool				operator!( void ) const;
	bool				operator==( const InputServerDevice& cDev ) const;
	bool				operator!=( const InputServerDevice& cDev ) const;

private:
	friend class InputServer;
	int32				GetLibHandle( void )				{	CALLED();	return( m_nLibHandle );		}
	void				SetLibHandle( int32 nLibHandle )	{	CALLED();	m_nLibHandle = nLibHandle;	}
	
	Path										*m_pcPath;
	InputServerContainer<InputDeviceRef>		m_tDevices;
	int32										m_nLibHandle;
};

typedef InputServerDevice* instantiate_input_device();

}
#endif /* __INPUT_SERVER_DEVICE_H__ */


