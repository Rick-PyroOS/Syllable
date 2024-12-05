#include <os/types.h>
#include <os/kdebug.h>

#include <gui/desktop.h>
#include <gui/region.h>

#include <util/message.h>
#include <util/messagefilter.h>

#include <servers/inputserver/inputserverfilter.h>

using namespace os;

InputServerFilter::InputServerFilter( void )
{
	CALLED();
	m_pcPath = new Path();
}

InputServerFilter::~InputServerFilter()
{
	CALLED();
	
	if(m_pcPath)
		delete m_pcPath;
}

// turn filter on/off
status_t InputServerFilter::Activate( bool bActivate )
{
	CALLED();
	
	return EOK;
}

MessageFilterResult InputServerFilter::Filter( Message* pcMessage, std::list<Message> tOutList )
{
	CALLED();
	return MF_DISPATCH_MESSAGE;
}

status_t InputServerFilter::InitCheck( void )
{
	CALLED();
	return EOK;
}

status_t InputServerFilter::GetScreenRegion( Region *pcRegion ) const
{
	CALLED();
	
	Desktop cScreen;
	
	IRect cRect(IPoint(0, 0), cScreen.GetResolution());
	
	pcRegion->Set(cRect);
	
	return EOK;
}

String InputServerFilter::GetPath( void ) const
{
	CALLED();
	
	return( m_pcPath->GetPath() );
}

void InputServerFilter::SetPath( String& cPath )
{
	CALLED();
	
	m_pcPath->SetTo(cPath);
}

bool InputServerFilter::operator!( void ) const
{
	CALLED();
	return (this == NULL);
}

bool InputServerFilter::operator==( const InputServerFilter& cFlt ) const
{
	CALLED();
	return (*this == cFlt);
}

bool InputServerFilter::operator!=( const InputServerFilter& cFlt ) const
{
	CALLED();
	return (*this != cFlt);
}

