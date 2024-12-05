

#include <os/types.h>
#include <os/kernel.h>

#include <posix/errno.h>

#include <waiters/screensaver.h>


using namespace os;


ScreenSaver::ScreenSaver( Message *pcState, int32 nHandle )
{
	CALLED();
}

ScreenSaver::~ScreenSaver()
{
	CALLED();
}

status_t ScreenSaver::InitCheck( void )
{
	CALLED();
	
	return EOK;
}

status_t ScreenSaver::StartSaver( View *pcView, bool bPreview )
{
	CALLED();
	
	return EOK;
}

void ScreenSaver::StopSaver( void )
{
	CALLED();
}

void ScreenSaver::Draw( View *pcView, int32 nFrame )
{
	CALLED();
}

void ScreenSaver::StartConfig( View *pcConfigView )
{
	CALLED();
}

void ScreenSaver::StopConfig( void )
{
	CALLED();
}

status_t ScreenSaver::SaveState( Message *pcState) const
{
	CALLED();
	
	return EOK;
}

void ScreenSaver::SupplyInfo( Message *pcInfo ) const
{
	CALLED();
}

void ScreenSaver::ModulesChanged( const Message *pcInfo )
{
	CALLED();
}

void ScreenSaver::SetLoop( int32 nOnCount, int32 nOffCount )
{
	CALLED();
	
	m_nOnCount = nOnCount;
	m_nOffCount = nOffCount;
}

int32 ScreenSaver::LoopOnCount( void ) const
{
	CALLED();
	
	return m_nOnCount;
}

int32 ScreenSaver::LoopOffCount( void ) const
{
	CALLED();
	
	return m_nOffCount;
}

void ScreenSaver::SetTickSize( bigtime_t nSize )
{
	CALLED();
	
	m_nTickSize = nSize;
}

bigtime_t ScreenSaver::TickSize( void ) const
{
	CALLED();
	
	return m_nTickSize;
}

String ScreenSaver::GetPath( void ) const
{
	CALLED();
	
	return( m_pcPath->GetPath() );
}

void ScreenSaver::SetPath( String& cPath )
{
	CALLED();
	
	m_pcPath->SetTo(cPath);
}

