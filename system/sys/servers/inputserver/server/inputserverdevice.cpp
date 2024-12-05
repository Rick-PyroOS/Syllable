#include <servers/inputserver/inputserverdevice.h>

#include "inputserver.h"



InputDeviceRef::InputDeviceRef( void )
{
	CALLED();
	
	m_eType = DEVICE_POINTING;
	m_zName = NULL;
	m_pCookie = NULL;
}

InputDeviceRef::~InputDeviceRef( void )
{
	CALLED();
	
	m_pCookie = NULL;	
	m_zName = NULL;
}

InputServerDevice::InputServerDevice( void )
{
	CALLED();
	
	m_pcPath = new Path();
}

InputServerDevice::~InputServerDevice( void )
{
	CALLED();
	
	if(m_pcPath)
		delete m_pcPath;
}

status_t InputServerDevice::Control( const char *zName, uint32 nCommand, Message *pcMessage, void *pCookie )
{
	CALLED();
	return EOK;
}

status_t InputServerDevice::InitCheck( void )
{
	CALLED();
	return EOK;
}

status_t InputServerDevice::Start( const char *zName, void *pCookie )
{
	CALLED();
	return EOK;
}

status_t InputServerDevice::StartAll( void )
{
	CALLED();
	return EOK;
}

status_t InputServerDevice::Stop( const char *zName, void *pCookie )
{
	CALLED();
	return EOK;
}

status_t InputServerDevice::StopAll( void )
{
	CALLED();
	return EOK;
}

bool InputServerDevice::IsRunning( void ) const
{
	CALLED();
	return true;
}

status_t InputServerDevice::SystemShuttingDown( void )
{
	CALLED();
	return EOK;
}

status_t InputServerDevice::ControlAll( InputDeviceType_t m_eType, const char *zName, uint32 nCommand, Message *pcMessage, void *pCookie )
{
	CALLED();
	
	InputDeviceRef *pcDevRef;
	bool bFound = false;
	
	if(!m_tDevices.IsEmpty()) {
		pcDevRef = m_tDevices.GetFirstObj();
		
		while(pcDevRef != m_tDevices.End()) {
			if(pcDevRef->GetType() == m_eType) {
				bFound = true;
				Control(pcDevRef->GetName(), nCommand, pcMessage, pcDevRef->GetCookie());
			}
			
			pcDevRef = m_tDevices.GetNextObj(pcDevRef);
		}
	}
	
	if(m_tDevices.IsEmpty() || (!bFound && (pcDevRef == m_tDevices.End()))) {
		dbprintf("%s: No device references of type '%u' loaded\n", __PRETTY_FUNCTION__, m_eType);
		return -EINVAL;
	}
	
	return EOK;
}

status_t InputServerDevice::EnqueueMessage( Message *cMessage )
{
	CALLED();
	return g_pcInServ->EnqueueDeviceMessage(cMessage);
}

status_t InputServerDevice::RegisterDevices( void )
{
	CALLED();
	return g_pcInServ->RegisterDevices(this);
}

status_t InputServerDevice::UnregisterDevices( void )
{
	CALLED();
	return g_pcInServ->UnregisterDevices(this);
}

status_t InputServerDevice::AddDeviceRefs( InputDeviceRef *pcDevice )
{
	CALLED();
	
	if(!pcDevice)
		return -ENODEV;
	
	m_tDevices.Insert(pcDevice);
}

status_t InputServerDevice::RemoveDeviceRefs( InputDeviceRef *pcDevice )
{
	CALLED();
	
	if(!pcDevice)
		return -ENODEV;
	
	m_tDevices.Remove(pcDevice);
}

bool InputServerDevice::HandlesDevice( InputDeviceRef *pcDevice ) const
{
	CALLED();
	
	if(!pcDevice)
		return -ENODEV;
	
	if(m_tDevices.HasObj(pcDevice))
		return true;
	else
		return false;
}

bool InputServerDevice::HasDevice( const char *zName, InputDeviceType_t eType )
{
	CALLED();

	// Is it already being monitored?
	InputDeviceRef *pcRef = m_tDevices.GetFirstObj();
	
	if(zName == NULL) {
		while(pcRef != m_tDevices.End()) {
			if(pcRef->GetType() == eType)
				return true;
			
			pcRef = m_tDevices.GetNextObj(pcRef);
		}
	} else {
		while(pcRef != m_tDevices.End()) {
			if((pcRef->GetName() == zName) && (pcRef->GetType() == eType))
				return true;
			
			pcRef = m_tDevices.GetNextObj(pcRef);
		}
	}
	
	return false;
}

status_t InputServerDevice::StartMonitoringDevice( const char *zDevDir, InputDeviceRef *pcDevice )
{
	CALLED();
	return g_pcInServ->StartMonitoringDevice(zDevDir, pcDevice);
}

status_t InputServerDevice::StopMonitoringDevice( const char *zDevDir, InputDeviceRef *pcDevice )
{
	CALLED();
	return g_pcInServ->StopMonitoringDevice(zDevDir, pcDevice);
}

String InputServerDevice::GetPath( void ) const
{
	CALLED();
	
	return( m_pcPath->GetPath() );
}

void InputServerDevice::SetPath( String& cPath )
{
	CALLED();
	
	m_pcPath->SetTo(cPath);
}

bool InputServerDevice::operator!( void ) const
{
	CALLED();
	return (this == NULL);
}

bool InputServerDevice::operator==( const InputServerDevice& cDev ) const
{
	CALLED();
	return (*this == cDev);
}

bool InputServerDevice::operator!=( const InputServerDevice& cDev ) const
{
	CALLED();
	return (*this != cDev);
}

