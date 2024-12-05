





#include <stdlib.h>
#include <stdio.h>
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <stdexcept>

#include <os/msgport.h>
#include <os/kernel.h>
#include <os/semaphore.h>
#include <os/threads.h>
#include <os/types.h>
#include <os/image.h>

#include <gui/guidefines.h>

#include <storage/nodemonitor.h>

#include <util/locker.h>
#include <util/string.h>

#include <servers/inputserver/protocol.h>

#include "inputserver.h"
#include "inputnode.h"
#include "offlinewindow.h"

using namespace os;

InputServer* g_pcInServ;

bool InputServer::s_bRestart = false;
bool InputServer::s_bQuit = false;
bool InputServer::s_bInputMethodAware = false;

AttributeSettings* InputServer::s_pcSettings = NULL;
AttrBundle_s* InputServer::s_psSettingsBundle = NULL;
keymap* InputServer::s_psKeymap = NULL;
keymap* InputServer::s_psDefaultKeymap = NULL;
String* InputServer::s_pcKeymapPath = NULL;

MessageQueue InputServer::s_cEventQueue;
MessageQueue InputServer::s_cMethodEventQueue;


InputServer::InputServer()
			: Application( "input_server", APP_SERVER )
{
	CALLED();
	
	// Initialize port to useless value
	m_hAppServerPort = -1;
	
	// ****** Make sure that the maps, queue & sets are empty
	
	// Device set
	if(!m_tDevices.IsEmpty())
		m_tDevices.MakeEmpty();
	
	// Method set
	if(!m_tMethods.IsEmpty())
		m_tMethods.MakeEmpty();
	
	// Filter set
	if(!m_tFilters.IsEmpty())
		m_tFilters.MakeEmpty();
	
	// Monitored input map
	if(!m_tDevNodes.IsEmpty())
		m_tDevNodes.MakeEmpty();
	
	// Monitored addons map
	if(!m_tAddonNodes.IsEmpty())
		m_tAddonNodes.MakeEmpty();
}

InputServer::~InputServer()
{
	CALLED();
	
	UnloadAllDevices();
	UnloadAllMethods();
	UnloadAllFilters();
}

bool InputServer::OkToQuit()
{
	CALLED();
	return false;
}


thread_id InputServer::Start( void )
{
	CALLED();
	
	// setup our port & get port for appserver
	m_hInputServerPort = GetMsgPort();
	
#ifdef __ENABLE_DEBUG__
	while(m_hAppServerPort < 0) {
		dbprintf("%s: Debug mode, looking for 'is_test_app' port\n", __PRETTY_FUNCTION__);
		m_hAppServerPort = find_port("l:is_test_app");
	}
#else
	while(m_hAppServerPort < 0) {
		m_hAppServerPort = get_app_server_port();
	}
#endif
	
	make_port_public(m_hInputServerPort);
	dbprintf("%s: Inputserver running at port %i\n", __PRETTY_FUNCTION__, m_hInputServerPort);
	
	// setup messenger
	m_pcMessenger = new Messenger(m_hAppServerPort);
	
	// Start monitoring addons
	StartMonitoringAddon("methods");
	StartMonitoringAddon("filters");
	StartMonitoringAddon("devices");
	
	// Load configuration file, devices, filters & methods
	LoadConfigFile();
	LoadAllFilters();
	LoadAllMethods();
	LoadAllDevices();
	
	// Start Pipe thread here
	m_hPipeline = spawn_thread( "__pipeline__", (void*)InputServer::Pipeline, URGENT_DISPLAY_PRIORITY, 0, this );
	
	resume_thread(m_hPipeline);
	
	return Application::Run();
}

void InputServer::HandleMessage( Message* pcMessage )
{
	CALLED();
	
	if(!pcMessage) {
		dbprintf("%s: Inputserver does not understand this NULL message!\n", __PRETTY_FUNCTION__);
		return;
	}
	
	// server messages
	if(HandleServerMessage(pcMessage) >=0 )
		return;
	
	// nodemonitor generated messages
	if(HandleNodeMessage(pcMessage) >=0 )
		return;
	
	// inputserver (only) messages
	if(HandleISMessage(pcMessage) >=0 )
		return;
	
	// device (only) messages
	if(HandleDeviceMessage(pcMessage) >=0 )
		return;
	
	Application::HandleMessage(pcMessage);	
}

status_t InputServer::EnqueueDeviceMessage( Message* pcMessage )
{
	CALLED();
	
	s_cEventQueue.Lock();
	s_cEventQueue.AddMessage(pcMessage);
	s_cEventQueue.Unlock();
	
	return EOK;
}

status_t InputServer::EnqueueMethodMessage( Message* pcMessage )
{
	CALLED();
	
	s_cMethodEventQueue.Lock();
	s_cMethodEventQueue.AddMessage(pcMessage);
	s_cMethodEventQueue.Unlock();
	
	return EOK;
}

status_t InputServer::RegisterDevices( InputServerDevice* pcDevice )
{
	CALLED();
	
	bool bFound = false;
	
	if(!pcDevice)
		return -ENODEV;
	
	// Check to see if device is already registered
	if(m_tDevices.HasObj(pcDevice)) {
		dbprintf("%s: Input device is already registered\n", __PRETTY_FUNCTION__);
		bFound = true;
	}
	
	// If device is not found, then add it to device set
	if(!bFound) {
		dbprintf("%s: Registering input device\n", __PRETTY_FUNCTION__);
		m_tDevices.Insert(pcDevice);
	}
		
	return EOK;
}

status_t InputServer::UnregisterDevices( InputServerDevice* pcDevice )
{
	CALLED();
	
	bool bFound = true;
	
	if(!pcDevice)
		return -ENODEV;
	
	// Check to see if device is registered
	if(!m_tDevices.HasObj(pcDevice)) {
		dbprintf("%s: Input device is not registered\n", __PRETTY_FUNCTION__);		
		bFound = false;
	}
	
	// If device is not found, then add it to device set
	if(bFound) {
		dbprintf("%s: Unregistering Input device\n", __PRETTY_FUNCTION__);
		m_tDevices.Remove(pcDevice);
	}
		
	return EOK;
}

status_t InputServer::StartStopDevices( const char *zName, InputDeviceType_t eType, bool bStart )
{
	CALLED();
	
	InputServerDevice *pcISD = m_tDevices.GetFirstObj();
	
	while(pcISD != m_tDevices.End()) {
		if(pcISD->HasDevice(zName, eType)) {
			if(bStart)
				pcISD->Start(zName);
			else {
				pcISD->Stop(zName);
				return EOK;
			}
		}
		pcISD = m_tDevices.GetNextObj(pcISD);
	}
	
	return -ENODEV;
}

status_t InputServer::StartStopDevices( InputServerDevice& cDevice, bool bStart )
{
	CALLED();
	
	if(&cDevice != NULL) {
		if(bStart)
			cDevice.StartAll();
		else {
			cDevice.StopAll();
			return EOK;
		}
	}
	
	return -ENODEV;	
}

status_t InputServer::ControlDevices( const char *zName, InputDeviceType_t eType, uint32 nCode, Message* pcMessage, void *pCookie )
{
	CALLED();
	
	InputServerDevice *pcISD = m_tDevices.GetFirstObj();
	
	while(pcISD != m_tDevices.End()) {
		if(pcISD->HasDevice(zName, eType)) {
			pcISD->Control(zName, nCode, pcMessage, pCookie);
			return EOK;
		}
		pcISD = m_tDevices.GetNextObj(pcISD);
	}
	
	return -ENODEV;
}

status_t InputServer::StartMonitoringDevice( const char *zDevDir, InputDeviceRef *pcDevice )
{
	CALLED();
	
	NodeMonitor *pcMonitor;
	
	String cPath = "/dev/input/";
	
	// Build path
	if((cPath += zDevDir) == NULL) {
		dbprintf("%s: String::operator+=() error\n", __PRETTY_FUNCTION__);
		return -ENOMEM;
	}
	
	// Is it already being monitored?
	InputNode *pcNode = m_tDevNodes.GetFirstObj();
	
	while(pcNode != m_tDevNodes.End()) {
		if(pcNode->GetPath() == cPath)
			return -EALREADY;
		
		pcNode = m_tDevNodes.GetNextObj(pcNode);
	}
	
	// Create inputnode
	InputNode *pcInputNode = new InputNode(cPath, NWATCH_ALL | NWATCH_FULL_DST_PATH, this, pcDevice);
	
	// Add to monitored list
	m_tDevNodes.Insert(pcInputNode);
	
	return EOK;
}

status_t InputServer::StopMonitoringDevice( const char *zDevDir, InputDeviceRef *pcDevice )
{
	CALLED();
	
	NodeMonitor *pcMonitor;
	String cPath = "/dev/input";
	
	// Build path
	if((cPath += zDevDir) == NULL) {
		dbprintf("%s: String::operator+=() error\n", __PRETTY_FUNCTION__);
		return -ENOMEM;
	}
	
	// Is it already being monitored?
	InputNode *pcNode = m_tDevNodes.GetFirstObj();
	
	while(pcNode != m_tDevNodes.End()) {
		if(pcNode->GetPath() == cPath)
			break;
		
		pcNode = m_tDevNodes.GetNextObj(pcNode);
	}
	
	if(pcNode == m_tDevNodes.End())
		return -EINVAL;
	
	// Remove from monitored list
	m_tDevNodes.Remove(pcNode);
	
	// Stop monitoring
	delete pcNode;
	
	return EOK;
}

status_t InputServer::StartMonitoringAddon( const char *zDevDir )
{
	CALLED();
	
	NodeMonitor *pcMonitor;
	InputNode *pcNode;
	
	String cPath = "/system/drivers/servers/inputserver/";
	
	// Build path
	cPath += zDevDir;
		
	// Is it already being monitored?
	if(!m_tAddonNodes.IsEmpty()) {
		pcNode = m_tAddonNodes.GetFirstObj();
		
		while(pcNode != m_tAddonNodes.End()) {
			if(pcNode->GetPath() == cPath)
				return -EALREADY;
			
			pcNode = m_tAddonNodes.GetNextObj(pcNode);
		}
	}
	
	// Create inputnode
	InputNode *pcInputNode = new InputNode(cPath, NWATCH_DIR | NWATCH_FULL_DST_PATH, this);
	
	// Add to monitored list
	m_tAddonNodes.Insert(pcInputNode);
	
	return EOK;
}

status_t InputServer::StopMonitoringAddon( const char *zDevDir )
{
	CALLED();
	
	NodeMonitor *pcMonitor;
	InputNode *pcNode;
	String cPath = "/system/drivers/servers/inputserver/";
	
	// Build path
	cPath += zDevDir;
	
	// Is it already being monitored?
	if(!m_tAddonNodes.IsEmpty()) {
		pcNode = m_tAddonNodes.GetFirstObj();
		
		while(pcNode != m_tAddonNodes.End()) {
			if(pcNode->GetPath() == cPath)
				return -EALREADY;
			
			pcNode = m_tAddonNodes.GetNextObj(pcNode);
		}
	}
	
	if(pcNode == m_tAddonNodes.End())
		return -EINVAL;
	
	// Remove from monitored list
	m_tAddonNodes.Remove(pcNode);
	
	// Stop monitoring
	delete pcNode;
	
	return EOK;
}

status_t InputServer::Pipeline( void *pData )
{
	CALLED();
	
	InputServer *pcThis = (InputServer*)pData;
	
	pcThis->Pipe();
	
	return EOK;
}

void InputServer::Pipe( void )
{
	CALLED();
	
	while(!s_bRestart && !s_bQuit) {
		if(s_cEventQueue.Lock()) {			
			if(!s_cEventQueue.IsEmpty() || !s_cMethodEventQueue.IsEmpty()) {
				CleanupEvents(&s_cEventQueue);
				MethodizeEvents(&s_cEventQueue);
				FilterEvents(&s_cEventQueue);
				DispatchEvents(&s_cEventQueue);
			}
			
			s_cEventQueue.Unlock();
		}
	}
	
	dbprintf("%s: Message pipeline terminating....Is something wrong?\n", __PRETTY_FUNCTION__);
}

bool InputServer::CleanupEvents( MessageQueue* pcEvents )
{
	CALLED();
	
	return true;
}

bool InputServer::MethodizeEvents( MessageQueue* pcEvents )
{
	CALLED();
	
	Message* pcMsg;
	Message cMsgTemp;
	Message cTranslated;
	MessageList tNewEvents;
	int32 nCount;
	int32 nIndex;
	int32 nOpCode;
	bool bRemoveEvent;
	bool bInlineOnly;
	bool bConfirmed;
	
	if(!m_pcActiveMethod)
		return true;
	
	nCount = pcEvents->GetMessageCount();
	
	for(nIndex = 0; nIndex < nCount;) {
		FilterEvent(m_pcActiveMethod, pcEvents, nIndex, nCount);
	}
	
	while(!s_cMethodEventQueue.Lock()) {
	}
	
	// Move method events into main event queue - they aren't methodized
	if(!s_cMethodEventQueue.IsEmpty()) {
		pcMsg = s_cMethodEventQueue.NextMessage();
		pcEvents->AddMessage(pcMsg);
		s_cMethodEventQueue.RemoveMessage(pcMsg);
	}
	
	s_cMethodEventQueue.Unlock();
	
	if(!s_bInputMethodAware) {
		// Handle non-inputmethod aware views
		nCount = pcEvents->GetMessageCount();
		
		// Possibly add new events that don't need to be rechecked
		for(nIndex = 0; nIndex < nCount; nIndex++) {
			pcMsg = pcEvents->FindMessage(nIndex);
			if(pcMsg->GetCode() != M_INPUT_METHOD_EVENT)
				continue;
			
			dbprintf("%s: Input method event received\n", __PRETTY_FUNCTION__);
			
			bRemoveEvent = true;
			
			if(pcMsg->FindInt32("os:opcode", &nOpCode) == EOK) {
				if(pcMsg->FindBool("os:inline_only", &bInlineOnly) != EOK)
					bInlineOnly = false;
				
				if(bInlineOnly) {
					if(nOpCode = INPUT_METHOD_CHANGED && pcMsg->FindBool("os:confirmed", &bConfirmed) == EOK &&
					   bConfirmed && pcMsg->FindMessage("os:translated", &cTranslated) == EOK) {
					
						// Translate for non-aware view
						*pcMsg = cTranslated;
						bRemoveEvent = false;
					}
				} else {
					if(!m_pcOfflineWindow & nOpCode == INPUT_METHOD_STARTED)
						m_pcOfflineWindow = new OfflineWindow();
					
					if(m_pcOfflineWindow) {
						m_pcOfflineWindow->HandleInputEventMessage(pcMsg, tNewEvents);
						
						if(!tNewEvents.empty()) {
							while(!tNewEvents.empty()) {
								cMsgTemp = tNewEvents.front();
								pcEvents->AddMessage(&cMsgTemp);
								tNewEvents.pop_front();
							}
							
							nOpCode = INPUT_METHOD_STOPPED;
						}
						
						if(nOpCode == INPUT_METHOD_STOPPED) {
							m_pcOfflineWindow->PostMessage(M_QUIT_REQUESTED);
							m_pcOfflineWindow = NULL;
						}
					}
				}
			}
			
			if(bRemoveEvent) {
				// Offline window ate the event
				pcEvents->RemoveMessage(pcMsg);
				nIndex--;
				nCount--;
			}
		}
	}
	
	return (pcEvents->GetMessageCount() > 0);
}

bool InputServer::FilterEvents( MessageQueue* pcEvents )
{
	CALLED();
	
	int32 nCount = m_tFilters.GetTabCount();
	int32 nEventCount = pcEvents->GetMessageCount();
	
	InputServerFilter *pcFilter = m_tFilters.GetFirstObj();
	
	while(pcFilter != m_tFilters.End()) {
		// Apply filter identified by nIndex to all messages
		for(int32 nEventIndex = 0; nEventIndex < nEventCount;) {
			FilterEvent(pcFilter, pcEvents, nEventIndex, nEventCount);
		}
		
		pcFilter = m_tFilters.GetNextObj(pcFilter);
	}
	
	return (nEventCount != 0);
}

bool InputServer::DispatchEvents( MessageQueue* pcEvents )
{
	CALLED();
	
	Message *pcEvent;
	
	dbprintf("%s: Before loop\n", __PRETTY_FUNCTION__);
	while((pcEvent = pcEvents->NextMessage()) != NULL) {
		dbprintf("%s: Send message out\n", __PRETTY_FUNCTION__);
		// Send message out
		DispatchEvent(pcEvent);
	}
	dbprintf("%s: After loop\n", __PRETTY_FUNCTION__);
	
	return true;
}

void InputServer::FilterEvent( InputServerFilter* pcFilter, MessageQueue* pcEvents,
							   int32& nIndex, int32& nCount )
{
	CALLED();
	
	Message *pcEvent = pcEvents->FindMessage(nIndex);
	
	MessageList tNewEvents;
	MessageFilterResult eResult = pcFilter->Filter(pcEvent, tNewEvents);
	
	if((eResult == MF_DISCARD_MESSAGE) || (tNewEvents.size() > 0)) {
		// Get rid of the current event, it's not needed
		pcEvents->RemoveMessage(pcEvent);
		
		delete pcEvent;
		
		if(eResult == MF_DISPATCH_MESSAGE) {
			MessageQueue cAddedEvents;
			
			cAddedEvents.Lock();
			
			while(!tNewEvents.empty()) {
				cAddedEvents.AddMessage(&tNewEvents.back());
				tNewEvents.pop_back();
			}
			
			// Add new events, but don't send them through Methodize() again
			
			int nTemp1;
			int nTemp2 = 0;
			int nTemp3;
			
			nTemp1 = nTemp3 = cAddedEvents.GetMessageCount();
			while(nTemp1) {
				pcEvents->AddMessage(cAddedEvents.FindMessage(nTemp2));
				nTemp1--;
			}
			nIndex += nTemp3;
			nCount = pcEvents->GetMessageCount();
			
			cAddedEvents.Unlock();
		} else
			nCount --;
	} else
		nIndex++;
}

status_t InputServer::DispatchEvent( Message* pcEvent )
{
	CALLED();
	
	return(m_pcMessenger->SendMessage(pcEvent));
}

status_t InputServer::HandleServerMessage( Message* pcMessage )
{
	CALLED();
	
	switch(pcMessage->GetCode()) {
		// server messages
		case M_SERVER_PING: {
		} break;
		case M_SERVER_RESTART: {
			s_bRestart = true;
			wait_for_thread(m_hPipeline);
		} break;
		case M_SERVER_QUIT: {
			s_bQuit = true;
			wait_for_thread(m_hPipeline);
		} break;
		// default for messages we don't handle
		default: {
			dbprintf("%s: Inputserver does not understand this 'server' message!\n", __PRETTY_FUNCTION__);
			return -EINVAL;
		}
	}
	
	return EOK;
}

status_t InputServer::HandleNodeMessage( Message* pcMessage )
{
	CALLED();
	
	InputServerDevice *pcOwner;
	FSNode cFNode1;
	FSNode cFNode2;
	String cPath;
	String cPath1;
	String cPath2;
	String cAddon;
	
	switch(pcMessage->GetCode()) {
		// nodemonitor generated messages
		case M_NODE_MONITOR: {
			int32 nEvent = -1;
			pcMessage->FindInt32("event", &nEvent);
			switch(nEvent) {
				case NWEVENT_CREATED: {
					dbprintf("%s: NWEVENT_CREATED\n", __PRETTY_FUNCTION__);
					
					int64 nDirNode = -1;
					String cName;
					
					pcMessage->FindInt64("dir_node", &nDirNode);
					pcMessage->FindString("name", &cName);
					
					// If a /dev directory triggered the message, figure out which directory
					InputNode *pcNode1 = m_tDevNodes.GetFirstObj();
					
					while(pcNode1 != m_tDevNodes.End()) {
						cFNode1.SetTo(cPath1 = pcNode1->GetPath());
						if(cFNode1.GetInode() == nDirNode) {
							cPath = cPath1;
							break;
						}
						
						pcNode1 = m_tDevNodes.GetNextObj(pcNode1);
					}
	
					// If a /dev directory triggered the message, figure out which directory
					InputNode *pcNode2 = m_tAddonNodes.GetFirstObj();
					
					while(pcNode2 != m_tAddonNodes.End()) {
						cFNode2.SetTo(cPath2 = pcNode2->GetPath());
						if(cFNode2.GetInode() == nDirNode) {
							cPath = cPath2;
							break;
						}
						
						pcNode2 = m_tAddonNodes.GetNextObj(pcNode2);
					}
	
					if(pcNode1 == m_tDevNodes.End() && pcNode2 == m_tAddonNodes.End()) {
						dbprintf("%s: File is neither an addon nor an input node\n", __PRETTY_FUNCTION__);
						return -EINVAL;
					}
					
					cPath += '/';
					cPath += cName;
					
					// If directory is for addons, determine which type of addon & load it
					if(strstr(cPath.c_str(), "devices") != NULL) {
						dbprintf("%s: Addon is an InputServerDevice\n", __PRETTY_FUNCTION__);
						
						LoadDevice(cPath);
					}
					if(strstr(cPath.c_str(), "filters") != NULL) {
						dbprintf("%s: Addon is an InputServerFilter\n", __PRETTY_FUNCTION__);
						
						LoadFilter(cPath);
					}
					if(strstr(cPath.c_str(), "methods") != NULL) {
						dbprintf("%s: Addon is an InputServerMethod\n", __PRETTY_FUNCTION__);
						
						LoadMethod(cPath);
					}
					
					// If directory is from /dev tree, notify correct InputServerDevice
					if(strstr(cPath.c_str(), "/dev/input/") != NULL) {
						dbprintf("%s: Device path is: %s\n", __PRETTY_FUNCTION__, cPath.c_str());
						
						// Find inputnode that contains cPath
						pcNode1 = m_tDevNodes.GetFirstObj();
						
						while(pcNode1 != m_tDevNodes.End()) {
							if(pcNode1->GetPath() == cPath)
								break;
							
							pcNode1 = m_tDevNodes.GetNextObj(pcNode1);
						}
						
						if(pcNode1 == m_tDevNodes.End()) {
							dbprintf("%s: Input node '%s' not monitored\n", __PRETTY_FUNCTION__, cPath.c_str());
							return -EINVAL;
						}
						
						// Get device reference from inputnode
						InputDeviceRef *pcDevRef = pcNode1->GetDevRef();
						
						// Send message to inputserver device via ControlDevices()
						ControlDevices(pcDevRef->GetName(), pcDevRef->GetType(), M_NODE_MONITOR, pcMessage, pcDevRef->GetCookie());
					}					
				} break;
				case NWEVENT_MOVED: {
					dbprintf("%s: NWEVENT_MOVED\n", __PRETTY_FUNCTION__);
					
					bool bMovedInto = true;
					int32 nDevice = -1;
					String cName;
					String cOldPath;
					String cNewPath;
					String cTempPath;
					
					pcMessage->FindInt32("device", &nDevice);
					pcMessage->FindString("name", &cName);
					pcMessage->FindString("old_path", &cOldPath);
					pcMessage->FindString("new_path", &cNewPath);
					
					dbprintf("%s: cOldPath = %s\n", __PRETTY_FUNCTION__, cOldPath.c_str());
					dbprintf("%s: cNewPath = %s\n", __PRETTY_FUNCTION__, cNewPath.c_str());
					
					// Since nothing is moved to/from /dev/*, only addons reach this path
					
					// Find out the direction of the move; if "old_path" is empty, then the file was moved out of the directory					
					if(strstr(cOldPath.c_str(), "/") == NULL)
						bMovedInto = false;
					// Now that we know the direction of the move, look for path in known locations
					if(bMovedInto) {
						// If direction is "in", then just use "old_path" + "name"
						cOldPath += '/';
						cPath = cOldPath + cName;
					} else {
						// If direction is "out", then iterate through addons & strstr() against "name"
												
						// Check the ISD's
						InputServerDevice *pcISD = m_tDevices.GetFirstObj();
						
						while(pcISD != m_tDevices.End()) {
							cTempPath = pcISD->GetPath();
							
							if(strstr(cTempPath.c_str(), cName.c_str()) != NULL) {
								// We've got a match
								cPath = cTempPath;
								break;
							}
							
							pcISD = m_tDevices.GetNextObj(pcISD);
						}
						
						// Check the ISF's
						InputServerFilter *pcISF = m_tFilters.GetFirstObj();
						
						while(pcISF != m_tFilters.End()) {
							cTempPath = pcISF->GetPath();
							
							if(strstr((cTempPath.c_str()), cName.c_str()) != NULL) {
								// We've got a match
								cPath = cTempPath;
								break;
							}
							
							pcISF = m_tFilters.GetNextObj(pcISF);
						}
						
						// Check the ISM's
						InputServerMethod *pcISM = m_tMethods.GetFirstObj();
						
						while(pcISM != m_tMethods.End()) {
							cTempPath = pcISM->GetPath();
							
							if(strstr((cTempPath.c_str()), cName.c_str()) != NULL) {
								// We've got a match
								cPath = cTempPath;
								break;
							}
							
							pcISM = m_tMethods.GetNextObj(pcISM);
						}
										
						if(pcISD == m_tDevices.End() && pcISF == m_tFilters.End() && pcISM == m_tMethods.End()) {
							dbprintf("%s: File '%s' is not an InputServer addon\n", __PRETTY_FUNCTION__, cName.c_str());
							return -EINVAL;
						}
					}
					
					// If directory is for addons, determine which type of addon & load it
					if(strstr(cPath.c_str(), "devices") != NULL) {
						dbprintf("%s: Addon is an InputServerDevice\n", __PRETTY_FUNCTION__);
						
						bMovedInto ? LoadDevice(cPath) : UnloadDevice(cPath);
					}
					if(strstr(cPath.c_str(), "filters") != NULL) {
						dbprintf("%s: Addon is an InputServerFilter\n", __PRETTY_FUNCTION__);
						
						bMovedInto ? LoadFilter(cPath) : UnloadFilter(cPath);
					}
					if(strstr(cPath.c_str(), "methods") != NULL) {
						dbprintf("%s: Addon is an InputServerMethod\n", __PRETTY_FUNCTION__);
						
						bMovedInto ? LoadMethod(cPath) : UnloadMethod(cPath);
					}			
				} break;
				case NWEVENT_DELETED: {
					dbprintf("%s: NWEVENT_DELETED\n", __PRETTY_FUNCTION__);
					
					int64 nDirNode = -1;
					String cName;
					
					pcMessage->FindInt64("dir_node", &nDirNode);
					pcMessage->FindString("name", &cName);
					
					// If a /dev directory triggered the message, figure out which directory
					InputNode *pcNode1 = m_tDevNodes.GetFirstObj();
					
					while(pcNode1 != m_tDevNodes.End()) {
						cFNode1.SetTo(cPath1 = pcNode1->GetPath());
						if(cFNode1.GetInode() == nDirNode) {
							cPath = cPath1;
							break;
						}
						
						pcNode1 = m_tDevNodes.GetNextObj(pcNode1);
					}
	
					// If a /dev directory triggered the message, figure out which directory
					InputNode *pcNode2 = m_tAddonNodes.GetFirstObj();
					
					while(pcNode2 != m_tAddonNodes.End()) {
						cFNode2.SetTo(cPath2 = pcNode2->GetPath());
						if(cFNode2.GetInode() == nDirNode) {
							cPath = cPath2;
							break;
						}
						
						pcNode2 = m_tAddonNodes.GetNextObj(pcNode2);
					}
	
					if(pcNode1 == m_tDevNodes.End() && pcNode2 == m_tAddonNodes.End()) {
						dbprintf("%s: File is neither an addon nor an input node\n", __PRETTY_FUNCTION__);
						return -EINVAL;
					}
					
					cPath += '/';
					cPath += cName;
					
					// If directory is an addon directory, determine type & load it
					if(strstr(cPath.c_str(), "devices") != NULL) {
						dbprintf("%s: Addon is an InputServerDevice\n", __PRETTY_FUNCTION__);
						
						UnloadDevice(cPath);
					}
					if(strstr(cPath.c_str(), "filters") != NULL) {
						dbprintf("%s: Addon is an InputServerFilter\n", __PRETTY_FUNCTION__);
						
						UnloadFilter(cPath);
					}
					if(strstr(cPath.c_str(), "methods") != NULL) {
						dbprintf("%s: Addon is an InputServerMethod\n", __PRETTY_FUNCTION__);
						
						UnloadMethod(cPath);
					}
					
					// If directory is from /dev tree, notify correct InputServerDevice
					if(strstr(cPath.c_str(), "/dev/input/") != NULL) {
						dbprintf("%s: Device path is: %s\n", __PRETTY_FUNCTION__, cPath.c_str());
						
						// Find inputnode that contains cPath
						pcNode1 = m_tDevNodes.GetFirstObj();
						
						while(pcNode1 != m_tDevNodes.End()) {
							if(pcNode1->GetPath() == cPath)
								break;
							
							pcNode1 = m_tDevNodes.GetNextObj(pcNode1);
						}
						
						if(pcNode1 == m_tDevNodes.End()) {
							dbprintf("%s: Input node '%s' not monitored\n", __PRETTY_FUNCTION__, cPath.c_str());
							return -EINVAL;
						}
						
						// Get device reference from inputnode
						InputDeviceRef *pcDevRef = pcNode1->GetDevRef();
						
						// Send message to inputserver device via ControlDevices()
						ControlDevices(pcDevRef->GetName(), pcDevRef->GetType(), M_NODE_MONITOR, pcMessage, pcDevRef->GetCookie());
					}
				} break;
				default:break;
			}
		} break;
		// default for messages we don't handle
		default: {
			dbprintf("%s: Inputserver does not understand this 'nodemonitor' message!\n", __PRETTY_FUNCTION__);
			return -EINVAL;
		}
	}
	
	return EOK;
}

status_t InputServer::HandleISMessage( Message* pcMessage )
{
	CALLED();
	
	switch(pcMessage->GetCode()) {
		// server messages
		/*case M_SERVER_PING:
		case M_SERVER_RESTART:
		case M_SERVER_QUIT:*/
		// default for messages we don't handle
		default: {
			dbprintf("%s: Inputserver does not understand this 'inputserver' message!\n", __PRETTY_FUNCTION__);
			return -EINVAL;
		}
	}
	
	return EOK;
}

status_t InputServer::HandleDeviceMessage( Message* pcMessage )
{
	CALLED();
	
	bool bFound = false;
	
	switch(pcMessage->GetCode()) {
		// device messages
		case M_INPUT_DEVICE: {
			int32 nEvent = -1;
			pcMessage->FindInt32("event", &nEvent);
			switch(nEvent) {
				/* Keyboard notifications */
				case KEY_MAP_CHANGED:
				case KEY_LOCKS_CHANGED:
				case KEY_REPEAT_DELAY_CHANGED:
				case KEY_REPEAT_RATE_CHANGED: {
					InputServerDevice *pcISD = m_tDevices.GetFirstObj();
	
					while(pcISD != m_tDevices.End()) {
						if(pcISD->HasDevice(NULL, DEVICE_KEYBOARD)) {
							bFound = true;
							pcISD->ControlAll(DEVICE_KEYBOARD, NULL, M_INPUT_DEVICE, pcMessage, NULL);
							return EOK;
						}
						pcISD = m_tDevices.GetNextObj(pcISD);
					}
					
					if(pcISD == m_tDevices.End() && !bFound) {
						dbprintf("%s: No device addons loaded\n", __PRETTY_FUNCTION__);
						return -EINVAL;
					}
				} break;
				 /* Mouse notifications */
				case MOUSE_TYPE_CHANGED:
				case MOUSE_MAP_CHANGED:
				case MOUSE_SPEED_CHANGED:
				case CLICK_SPEED_CHANGED:
				case MOUSE_ACCELERATION_CHANGED: {
					InputServerDevice *pcISD = m_tDevices.GetFirstObj();
	
					while(pcISD != m_tDevices.End()) {
						if(pcISD->HasDevice(NULL, DEVICE_POINTING)) {
							bFound = true;
							pcISD->ControlAll(DEVICE_POINTING, NULL, M_INPUT_DEVICE, pcMessage, NULL);
							return EOK;
						}
						pcISD = m_tDevices.GetNextObj(pcISD);
					}
					
					if(pcISD == m_tDevices.End() && !bFound) {
						dbprintf("%s: No device addons loaded\n", __PRETTY_FUNCTION__);
						return -EINVAL;
					}
				} break;
			}
		} break;
		// default for messages we don't handle
		default: {
			dbprintf("%s: Inputserver does not understand this 'device' message!\n", __PRETTY_FUNCTION__);
			return -EINVAL;
		}
	}
	
	return EOK;
}

status_t InputServer::LoadConfig( void )
{
	CALLED();
	
	s_pcSettings = new AttributeSettings(String(SERVER_PATH));
	s_psSettingsBundle = s_pcSettings->ListSettings();
	
	return EOK;
}
status_t InputServer::SaveConfig( void )
{
	CALLED();
	
	return EOK;
}
	
status_t InputServer::LoadAllDevices( void )
{
	CALLED();
	
	InputServerDevice *pcISD;
	String cISDDirPath = "/system/drivers/servers/inputserver/devices";
	String cDevice;
	status_t nError = EOK;
	int nLib = -1;
	
	DIR *pcISDDir = NULL;
	
	/* Iterate through the ISD nodes in /system/drivers/servers/inputserver/devices */
	pcISDDir = opendir(cISDDirPath.c_str());	
	
	if(pcISDDir != NULL) {
		struct dirent *psEntry;
		
		while((psEntry = readdir(pcISDDir)) != NULL) {
			if(strcmp(psEntry->d_name, ".") == 0 || strcmp(psEntry->d_name, "..") == 0) {
				continue;
			}
			
			/* Build complete path */
			String cISDPath = cISDDirPath;
			
			cISDPath += '/';
			cDevice = cISDPath + psEntry->d_name;
			
			/* Open the isd */
			nLib = load_library(cDevice.c_str(), 0);
			
			if(nLib < 0) {
				dbprintf("%s: Filed to load ISD '%s'\n", __PRETTY_FUNCTION__, cDevice.c_str());
				nError = -ESYSCFAILED;
				continue;
			}
				
			instantiate_input_device *pInitFunc;
			
			/* Get isd init symbol */
			int nError = get_symbol_address(nLib, "init_isd_addon", -1, (void **)&pInitFunc);
			
			if(nError < 0) {
				dbprintf("%s: ISD '%s' does not export entry point init_isd_addon()\n", __PRETTY_FUNCTION__, cDevice.c_str());
				unload_library( nLib );
				nError = -EINITFAILED;
				continue;
			}
			
			/* Try to initialize the device */
			try {
				pcISD = pInitFunc();
				
				if(pcISD != NULL) {
					// Add device to device set
					if(pcISD->InitCheck() >= 0) {
						pcISD->StartAll();
						pcISD->SetPath(cDevice);
						pcISD->SetLibHandle(nLib);
					} else {
						dbprintf("%s: ISD '%s' failed InitCheck()\n", __PRETTY_FUNCTION__, cDevice.c_str());
						delete pcISD;
						
						unload_library(nLib);
					}
				} else {
					dbprintf("%s: ISD '%s' does not contain an ISD object\n", __PRETTY_FUNCTION__, cDevice.c_str());
					unload_library(nLib);
				}
			} catch(...) {
			}
		}
	}
	
	return nError;
}

status_t InputServer::UnloadAllDevices( void )
{
	CALLED();
	
	InputServerDevice *pcISD = m_tDevices.GetFirstObj();
	int32 nLib;
	
	while(pcISD != m_tDevices.End()) {
		pcISD->StopAll();
		m_tDevices.Remove(pcISD);
		nLib = pcISD->GetLibHandle();
		delete pcISD;
		
		unload_library(nLib);
		pcISD = m_tDevices.GetNextObj(pcISD);
	}
	
	return EOK;
}
	
status_t InputServer::LoadDevice( String &cDevice )
{
	CALLED();
	
	InputServerDevice *pcISD;
	int nLib = -1;
	
	/* Open the isd */
	nLib = load_library(cDevice.c_str(), 0);
	
	if(nLib < 0) {
		dbprintf("%s: Filed to load ISD '%s'\n", __PRETTY_FUNCTION__, cDevice.c_str());
		return -ESYSCFAILED;
	}
		
	instantiate_input_device *pInitFunc;
	
	/* Get isd init symbol */
	int nError = get_symbol_address(nLib, "init_isd_addon", -1, (void **)&pInitFunc);
	
	if(nError < 0) {
		dbprintf("%s: ISD '%s' does not export entry point init_isd_addon()\n", __PRETTY_FUNCTION__, cDevice.c_str());
		unload_library( nLib );
		return -EINITFAILED;
	}
	
	/* Try to initialize the device */
	try {
		pcISD = pInitFunc();
		
		if(pcISD != NULL) {
			// Add device to device set
			if(pcISD->InitCheck() >= 0) {
				pcISD->StartAll();
				pcISD->SetPath(cDevice);
				pcISD->SetLibHandle(nLib);
			} else {
				dbprintf("%s: ISD '%s' failed InitCheck()\n", __PRETTY_FUNCTION__, cDevice.c_str());
				delete pcISD;
				
				unload_library(nLib);
			}
		} else {
			dbprintf("%s: ISD '%s' does not contain an ISD object\n", __PRETTY_FUNCTION__, cDevice.c_str());
			unload_library(nLib);
		}
	} catch(...) {
	}
	
	return EOK;
}

status_t InputServer::UnloadDevice( String &cDevice )
{
	CALLED();
	
	InputServerDevice *pcISD = m_tDevices.GetFirstObj();
	int32 nLib;
	
	while(pcISD != m_tDevices.End()) {
		if(pcISD->GetPath() == cDevice.c_str())
			break;
		pcISD = m_tDevices.GetNextObj(pcISD);
	}
	
	if(pcISD == m_tDevices.End())
		return -EINVAL;
	
	m_tDevices.Remove(pcISD);
	pcISD->StopAll();
	nLib = pcISD->GetLibHandle();
	delete pcISD;
	
	unload_library(nLib);
	
	return EOK;
}

status_t InputServer::LoadAllMethods( void )
{
	CALLED();
	
	InputServerMethod *pcISM;
	String cISMDirPath = "/system/drivers/servers/inputserver/methods";
	String cMethod;
	status_t nError = EOK;
	int nLib = -1;
	
	DIR *pcISMDir = NULL;
	
	/* Iterate through the ISM nodes in /system/drivers/servers/inputserver/methods */
	pcISMDir = opendir(cISMDirPath.c_str());	
	
	if(pcISMDir != NULL) {
		struct dirent *psEntry;
		
		while((psEntry = readdir(pcISMDir)) != NULL) {
			if(strcmp(psEntry->d_name, ".") == 0 || strcmp(psEntry->d_name, "..") == 0) {
				continue;
			}
			
			/* Build complete path */
			String cISMPath = cISMDirPath;
			
			cISMPath += '/';
			cMethod = cISMPath + psEntry->d_name;
			
			/* Open the ism */
			nLib = load_library(cMethod.c_str(), 0);
			
			if(nLib < 0) {
				dbprintf("%s: Filed to load ISM '%s'\n", __PRETTY_FUNCTION__, cMethod.c_str());
				nError = -ESYSCFAILED;
				continue;
			}
				
			instantiate_input_method *pInitFunc;
			
			/* Get ism init symbol */
			int nError = get_symbol_address(nLib, "init_ism_addon", -1, (void **)&pInitFunc);
			
			if(nError < 0) {
				dbprintf("%s: ISM '%s' does not export entry point init_ism_addon()\n", __PRETTY_FUNCTION__, cMethod.c_str());
				unload_library( nLib );
				nError = -EINITFAILED;
				continue;
			}
			
			/* Try to initialize the method */
			try {
				pcISM = pInitFunc();
				
				if(pcISM != NULL) {
					// Add method to methodr set
					if(pcISM->Activate(true) >= 0) {
						pcISM->SetPath(cMethod);
						pcISM->SetLibHandle(nLib);
						m_tMethods.Insert(pcISM);
					} else {
						dbprintf("%s: ISM '%s' failed to Activate()\n", __PRETTY_FUNCTION__, cMethod.c_str());
						delete pcISM;
						
						unload_library(nLib);
					}
				} else {
					dbprintf("%s: ISM '%s' does not contain an ISM object\n", __PRETTY_FUNCTION__, cMethod.c_str());
					unload_library(nLib);
				}
			} catch(...) {
			}
		}
	}
	
	return nError;
}

status_t InputServer::UnloadAllMethods( void )
{
	CALLED();
	
	InputServerMethod *pcISM = m_tMethods.GetFirstObj();
	int32 nLib;
	
	while(pcISM != m_tMethods.End()) {
		pcISM->Activate(false);
		m_tMethods.Remove(pcISM);
		nLib = pcISM->GetLibHandle();
		delete pcISM;
		
		unload_library(nLib);
		pcISM = m_tMethods.GetFirstObj();
	}
	
	return EOK;
}
	
status_t InputServer::LoadMethod( String &cMethod )
{
	CALLED();
	
	InputServerMethod *pcISM;
	int nLib = -1;
	
	/* Open the appserver driver */
	nLib = load_library(cMethod.c_str(), 0);
	
	if(nLib < 0) {
		dbprintf("%s: Filed to load ISM '%s'\n", __PRETTY_FUNCTION__, cMethod.c_str());
		return -ESYSCFAILED;
	}
		
	instantiate_input_method *pInitFunc;
	
	/* Get graphics driver init symbol */
	int nError = get_symbol_address(nLib, "init_ism_addon", -1, (void **)&pInitFunc);
	
	if(nError < 0) {
		dbprintf("%s: ISM '%s' does not export entry point init_ism_addon()\n", __PRETTY_FUNCTION__, cMethod.c_str());
		unload_library(nLib);
		return -EINITFAILED;
	}
	
	/* Try to initialize the method */
	try {
		pcISM = pInitFunc();
		
		if(pcISM != NULL) {
			// Add method to method set
			if(pcISM->Activate(true) >= 0) {
				pcISM->SetPath(cMethod);
				pcISM->SetLibHandle(nLib);
				m_tMethods.Insert(pcISM);
			} else {
				dbprintf("%s: ISM '%s' failed to Activate()\n", __PRETTY_FUNCTION__, cMethod.c_str());
				delete pcISM;
				
				unload_library(nLib);
			}
		} else {
			dbprintf("%s: ISM '%s' does not contain an ISM object\n", __PRETTY_FUNCTION__, cMethod.c_str());
			unload_library(nLib);
		}
	} catch(...) {
	}
	
	return EOK;
}

status_t InputServer::UnloadMethod( String &cMethod )
{
	CALLED();
	
	InputServerMethod *pcISM = m_tMethods.GetFirstObj();
	int32 nLib;
	
	while(pcISM != m_tMethods.End()) {
		if(pcISM->GetPath() == cMethod.c_str())
			break;
		pcISM = m_tMethods.GetNextObj(pcISM);
	}
	
	if(pcISM == m_tMethods.End())
		return -EINVAL;
	
	m_tMethods.Remove(pcISM);
	pcISM->Activate(false);
	nLib = pcISM->GetLibHandle();
	delete pcISM;
	
	unload_library(nLib);
	
	return EOK;
}

status_t InputServer::LoadAllFilters( void )
{
	CALLED();
	
	InputServerFilter *pcISF;
	String cISFDirPath = "/system/drivers/servers/inputserver/filters";
	String cFilter;
	status_t nError = EOK;
	int nLib = -1;
	
	DIR *pcISFDir = NULL;
	
	/* Iterate through the ISF nodes in /system/drivers/servers/inputserver/filters */
	pcISFDir = opendir(cISFDirPath.c_str());	
	
	if(pcISFDir != NULL) {
		struct dirent *psEntry;
		
		while((psEntry = readdir(pcISFDir)) != NULL) {
			if(strcmp(psEntry->d_name, ".") == 0 || strcmp(psEntry->d_name, "..") == 0) {
				continue;
			}
			
			/* Build complete path */
			String cISFPath = cISFDirPath;
			
			cISFPath += '/';
			cFilter = cISFPath + psEntry->d_name;
			
			/* Open the isf */
			nLib = load_library(cFilter.c_str(), 0);
			
			if(nLib < 0) {
				dbprintf("%s: Filed to load ISF '%s'\n", __PRETTY_FUNCTION__, cFilter.c_str());
				nError = -ESYSCFAILED;
				continue;
			}
				
			instantiate_input_filter *pInitFunc;
			
			/* Get isf init symbol */
			int nError = get_symbol_address(nLib, "init_isf_addon", -1, (void **)&pInitFunc);
			
			if(nError < 0) {
				dbprintf("%s: ISF '%s' does not export entry point init_isf_addon()\n", __PRETTY_FUNCTION__, cFilter.c_str());
				unload_library( nLib );
				nError = -EINITFAILED;
				continue;
			}
			
			/* Try to initialize the filter */
			try {
				pcISF = pInitFunc();
				
				if(pcISF != NULL) {
					// Add filter to filter set
					if(pcISF->InitCheck() >= 0) {
						pcISF->SetPath(cFilter);
						pcISF->SetLibHandle(nLib);
						m_tFilters.Insert(pcISF);
					} else {
						dbprintf("%s: ISF '%s' failed InitCheck()\n", __PRETTY_FUNCTION__, cFilter.c_str());
						delete pcISF;
						
						unload_library(nLib);
					}
				} else {
					dbprintf("%s: ISF '%s' does not contain an ISF object\n", __PRETTY_FUNCTION__, cFilter.c_str());
					unload_library(nLib);
				}
			} catch(...) {
			}
		}
	}
	
	return nError;
}
status_t InputServer::UnloadAllFilters( void )
{
	CALLED();
	
	InputServerFilter *pcISF = m_tFilters.GetFirstObj();
	int32 nLib;
	
	while(pcISF != m_tFilters.End()) {
		m_tFilters.Remove(pcISF);
		nLib = pcISF->GetLibHandle();
		delete pcISF;
		
		unload_library(nLib);
		pcISF = m_tFilters.GetFirstObj();
	}
	
	return EOK;
}
	
status_t InputServer::LoadFilter( String &cFilter )
{
	CALLED();
	
	InputServerFilter *pcISF;
	int nLib = -1;
	
	/* Open the appserver driver */
	nLib = load_library(cFilter.c_str(), 0);
	
	if(nLib < 0) {
		dbprintf("%s: Filed to load ISF '%s'\n", __PRETTY_FUNCTION__, cFilter.c_str());
		return -ESYSCFAILED;
	}
		
	instantiate_input_filter *pInitFunc;
	
	/* Get graphics driver init symbol */
	int nError = get_symbol_address(nLib, "init_isf_addon", -1, (void **)&pInitFunc);
	
	if(nError < 0) {
		dbprintf("%s: ISF '%s' does not export entry point init_isf_addon()\n", __PRETTY_FUNCTION__, cFilter.c_str());
		unload_library(nLib);
		return -EINITFAILED;
	}
	
	/* Try to initialize the driver */
	try {
		pcISF = pInitFunc();
		
		if(pcISF != NULL) {
			// Add filter to filter set
			if(pcISF->InitCheck() >= 0) {
				pcISF->SetPath(cFilter);
				pcISF->SetLibHandle(nLib);
				m_tFilters.Insert(pcISF);
			} else {
				dbprintf("%s: ISF '%s' failed InitCheck()\n", __PRETTY_FUNCTION__, cFilter.c_str());
				delete pcISF;
				
				unload_library(nLib);
			}
		} else {
			dbprintf("%s: ISF '%s' does not contain an ISF object\n", __PRETTY_FUNCTION__, cFilter.c_str());
			unload_library(nLib);
		}
	} catch(...) {
	}
	
	return EOK;
}

status_t InputServer::UnloadFilter( String &cFilter )
{
	CALLED();
	
	InputServerFilter *pcISF = m_tFilters.GetFirstObj();
	int32 nLib;
	
	while(pcISF != m_tFilters.End()) {
		if(pcISF->GetPath() == cFilter.c_str())
			break;
		pcISF = m_tFilters.GetNextObj(pcISF);
	}
	
	if(pcISF == m_tFilters.End())
		return -EINVAL;
	
	m_tFilters.Remove(pcISF);
	nLib = pcISF->GetLibHandle();
	delete pcISF;
	
	unload_library(nLib);
	
	return EOK;
}
	
int main( int argc, char **argv )
{
	dbprintf( "Inputserver Alive %d\n", get_thread_id( NULL ) );

	signal( SIGINT, SIG_IGN );
	signal( SIGQUIT, SIG_IGN );
	signal( SIGTERM, SIG_IGN );
	
	while(!InputServer::IsRestart() && !InputServer::IsQuit())
	{
		if(g_pcInServ)
			delete g_pcInServ;
		
		g_pcInServ = new InputServer();
		
		g_pcInServ->Start();
		
		if(InputServer::IsQuit()) {
			dbprintf( "WARNING : inputserver shuting down!!!\n" );
			break;
		} else if(InputServer::IsRestart())
			dbprintf( "WARNING : inputserver restarting!!!\n" );
	}
	
	return ( 0 );
}







