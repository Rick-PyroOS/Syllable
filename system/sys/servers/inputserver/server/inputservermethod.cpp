#include <servers/inputserver/inputservermethod.h>

#include "inputserver.h"



InputServerMethod::InputServerMethod( const char *zName, Bitmap *pcIcon )
{
	CALLED();
	
	SetName(zName);
	SetIcon(m_pcIcon);
	m_pcPath = new Path();
}

InputServerMethod::~InputServerMethod()
{
	CALLED();
	
	if(m_pcIcon)
		delete m_pcIcon;
	
	if(m_pcPath)
		delete m_pcPath;
}

// turn method on/off
status_t InputServerMethod::Activate( bool bActivate )
{
	CALLED();
	
	return EOK;
}

status_t InputServerMethod::EnqueueMessage( Message *pcMessage )
{
	CALLED();
	
	return g_pcInServ->EnqueueMethodMessage(pcMessage);
}

status_t InputServerMethod::SetName( const char *zName )
{
	CALLED();
	
	while(zName != NULL)
	{
		strcpy(m_zName, zName);
	}
}

status_t InputServerMethod::SetIcon( Bitmap *pcIcon )
{
	CALLED();
	
	if(m_pcIcon)
		delete m_pcIcon;
	
	m_pcIcon = pcIcon;
}

status_t InputServerMethod::SetMenu( Menu *pcMenu, Messenger *pcTarget )
{
	CALLED();
	
	if(m_pcMenu)
		delete m_pcMenu;
	
	m_pcMenu = pcMenu;
}

String InputServerMethod::GetPath( void ) const
{
	CALLED();
	
	return( m_pcPath->GetPath() );
}

void InputServerMethod::SetPath( String& cPath )
{
	CALLED();
	
	m_pcPath->SetTo(cPath);
}

bool InputServerMethod::operator!( void ) const
{
	CALLED();
	return (this == NULL);
}

bool InputServerMethod::operator==( const InputServerMethod& cMeth ) const
{
	CALLED();
	return (*this == cMeth);
}

bool InputServerMethod::operator!=( const InputServerMethod& cMeth ) const
{
	CALLED();
	return (*this != cMeth);
}

