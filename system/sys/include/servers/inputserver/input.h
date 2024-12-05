/*
 *  The Syllable input server
 *	input.h - Copyright (C) 2008 Dee Sharpe
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

#ifndef __INPUT_H__
#define __INPUT_H__

#include <list>

#include <os/types.h>

#include <util/messenger.h>

#include <servers/inputserver/inputservercontainer.h>

namespace os
{

typedef enum InputDeviceType_e
{
	DEVICE_POINTING = 0,
	DEVICE_KEYBOARD,
	DEVICE_GAMEPAD,
	DEVICE_JOYSTICK,
	DEVICE_UNDEFINED = 255
} InputDeviceType_t;

typedef enum InputMethodOp_e
{
	INPUT_METHOD_STARTED = 1,
	INPUT_METHOD_STOPPED,
	INPUT_METHOD_CHANGED,
	INPUT_METHOD_LOCATION_REQUEST
} InputMethodOp_t;

typedef enum InputDeviceNotification_e
{
	INPUT_DEVICE_ADDED   = 1,
	INPUT_DEVICE_STARTED = 2,
	INPUT_DEVICE_STOPPED = 4,
	INPUT_DEVICE_REMOVED = 8
} InputDeviceNotification_t;

class InputDevice;

InputDevice* find_input_device( const char *pName );
status_t get_input_devices( InputServerContainer<InputDevice> *ptList);
status_t watch_input_devices( Messenger cTarget, bool bStart );

class InputDevice
{
	virtual ~InputDevice();
	
	status_t Control( uint32 nCommand, Message *pcMessage );
	
	const char* Name( void ) const;
	InputDeviceType_t Type(void) const;
	
	status_t Start( void );
	status_t Stop( void );
	
	bool IsRunning( void ) const;

static status_t Start( InputDeviceType_t eType );
static status_t Stop( InputDeviceType_t eType );
static status_t Control( InputDeviceType_t eType, uint32 nCommand, Message *pcMessage );

private:

friend InputDevice* find_input_device( const char *pName );
friend status_t get_input_devices( InputServerContainer<InputDevice> *ptList);
	
	InputDevice();
	
	void		Identify( const char *zName, InputDeviceType_t eType );
	
	char*				m_zName;
	InputDeviceType_t	m_eType;
	
};

}
#endif /* __INPUT_H__ */




