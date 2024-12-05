/*
 *  The Syllable input server
 *  Copyright (C) 2008 Dee Sharpe
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

#ifndef	__INPUT_SERVER_H__
#define	__INPUT_SERVER_H__

#include <os/types.h>
#include <os/time.h>
#include <os/mouse.h>
#include <os/kernel.h>

#include <storage/fsnode.h>

#include <util/array.h>
#include <util/application.h>
#include <util/messagequeue.h>

#include <servers/inputserver/inputserverdevice.h>
#include <servers/inputserver/inputserverfilter.h>
#include <servers/inputserver/inputservermethod.h>
#include <servers/inputserver/inputservercontainer.h>

#include "inputnode.h"
#include "offlinewindow.h"

#define SERVER_PATH		"./inputserver"

namespace os
{

typedef std::list<Message> EventList;

class InputServer : public Application
{
public:
	InputServer();
	virtual ~InputServer();
	
	static InputServer*		GetInstance() { return s_pcInstance; }
	bool					OkToQuit();
	thread_id				Start( void );
	
	// Messages/Events
	void					HandleMessage( Message* pcMessage );
	status_t				EnqueueDeviceMessage( Message* pcMessage );
	status_t				EnqueueMethodMessage( Message* pcMessage );
	
	// Timing
	inline void				ResetEventTime()	{ CALLED();	m_nLastEvenTime  = get_system_time(); }
	inline bigtime_t		GetIdleTime() const	{ CALLED();	return( get_system_time() - m_nLastEvenTime ); }
	
	// Devices
	status_t				RegisterDevices( InputServerDevice* pcDevice );
	status_t				UnregisterDevices( InputServerDevice* pcDevice );
	status_t				StartStopDevices( const char *zName, InputDeviceType_t eType, bool bStart );
	status_t				StartStopDevices( InputServerDevice& cDevice, bool bStart );
	status_t				ControlDevices( const char *zName, InputDeviceType_t eType, uint32 nCode,
											Message* pcMessage, void *pCookie = NULL );
	status_t				StartMonitoringDevice( const char *zDevDir, InputDeviceRef *pcDevice = NULL );
	status_t				StopMonitoringDevice( const char *zDevDir, InputDeviceRef *pcDevice = NULL );
	
	status_t				StartMonitoringAddon( const char *zDevDir );
	status_t				StopMonitoringAddon( const char *zDevDir );
	
	// Methods
	inline status_t			SetActiveMethod( InputServerMethod *pcMethod) { CALLED();	m_pcActiveMethod = pcMethod; }
	
	static bool				IsRestart( void )		{	CALLED();	return( s_bRestart );	}
	static bool				IsQuit( void )			{	CALLED();	return( s_bQuit );		}
	
	// Settings
	String&					GetKeymapPath() const	{	CALLED();	return();	}
	
private:
	// Pipeline
	static status_t			Pipeline( void *pData );
	void					Pipe( void );
	
	bool					CleanupEvents( MessageQueue* pcEvents );
	bool					MethodizeEvents( MessageQueue* pcEvents );
	bool					FilterEvents( MessageQueue* pcEvents );
	bool					DispatchEvents( MessageQueue* pcEvents );
	
	void					FilterEvent( InputServerFilter* pcFilter, MessageQueue* pcEvents,
										 int32& nIndex, int32& nCount );
	status_t				DispatchEvent( Message* pcEvent );
	
	status_t				HandleServerMessage( Message* pcMessage );
	status_t				HandleNodeMessage( Message* pcMessage );
	status_t				HandleISMessage( Message* pcMessage );
	status_t				HandleDeviceMessage( Message* pcMessage );
	
	status_t				LoadConfig( void );
	status_t				SaveConfig( void );
	
	status_t				LoadAllDevices( void );
	status_t				UnloadAllDevices( void );
	
	status_t				LoadDevice( String &cDevice );
	status_t				UnloadDevice( String &cDevice );

	status_t				LoadAllMethods( void );
	status_t				UnloadAllMethods( void );
	
	status_t				LoadMethod( String &cMethod );
	status_t				UnloadMethod( String &cMethod );
	
	status_t				LoadAllFilters( void );
	status_t				UnloadAllFilters( void );
	
	status_t				LoadFilter( String &cFilter );
	status_t				UnloadFilter( String &cFilter );
	
	// ****** start static members
    static InputServer*		s_pcInstance;
    static bool				s_bRestart;
	static bool				s_bQuit;
	static bool				s_bInputMethodAware;
	// Settings
	static AttributeSettings*	s_pcSettings;
	static AttrBundle_s*		s_psSettingsBundle;
	static keymap*			s_psKeymap;
	static keymap*			s_psDefaultKeymap;
	static String*			s_pcKeymapPath;
	// Message queue for events
	static MessageQueue		s_cEventQueue;
	static MessageQueue		s_cMethodEventQueue;
	// ****** end of static members
	
	bigtime_t				m_nLastEvenTime;
	port_id		 			m_hInputServerPort;
	port_id					m_hAppServerPort;
	thread_id				m_hPipeline;
	
	// Container for add-ons
	InputServerContainer<InputServerDevice>		m_tDevices;	
	InputServerContainer<InputServerMethod>		m_tMethods;	
	InputServerContainer<InputServerFilter>		m_tFilters;	
	InputServerMethod							*m_pcActiveMethod;
	
	// Container for monitored dev directories
	InputServerContainer<InputNode>				m_tDevNodes;
	
	// Container for monitored add-on directories
	InputServerContainer<InputNode>				m_tAddonNodes;
	
	// Our own messenger
	Messenger									*m_pcMessenger;
	
	// Offline text replacement window
	OfflineWindow								*m_pcOfflineWindow;
};

} // namespace os

using namespace os;

extern InputServer* g_pcInServ;

#endif	/* __INPUT_SERVER_H__ */



