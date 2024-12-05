


#include <os/types.h>
#include <os/kernel.h>
#include <os/image.h>

#include <gui/view.h>

#include <posix/errno.h>

#include <util/message.h>

#include "engine.h"


using namespace os;


Engine::Engine()
	: Application( "screen_saver" )
{
	CALLED();
}

Engine::~Engine()
{
	CALLED();
}

status_t Engine::LoadSaver( String &cSaver )
{
	CALLED();
	
	ScreenSaver *pcSav;
	int nLib = -1;
	
	/* Open the library */
	nLib = load_library(cSaver.c_str(), 0);
	
	if(nLib < 0) {
		dbprintf("%s: Filed to load screensaver '%s'\n", __PRETTY_FUNCTION__, cSaver.c_str());
		return -ESYSCFAILED;
	}
		
	instantiate_screensaver *pInitFunc;
	
	/* Get screen saver init symbol */
	int nError = get_symbol_address(nLib, "init_screen_saver", -1, (void **)&pInitFunc);
	
	if(nError < 0) {
		dbprintf("%s: screensaver '%s' does not export entry point init_screen_saver()\n", __PRETTY_FUNCTION__, cSaver.c_str());
		unload_library( nLib );
		return -EINITFAILED;
	}
	
	/* Try to initialize the screen saver */
	try {
		pcSav = pInitFunc();
		
		if(pcSav != NULL) {			
			if(pcSav->InitCheck() >= 0) {
				pcSav->SetPath(cSaver);
				pcSav->SetLibHandle(nLib);
				m_pcSaver = pcSav;
			} else {
				dbprintf("%s: screensaver '%s' failed InitCheck()\n", __PRETTY_FUNCTION__, cSaver.c_str());
				delete pcSav;
				
				unload_library(nLib);
			}
		} else {
			dbprintf("%s: screensaver '%s' does not contain an screensaver object\n", __PRETTY_FUNCTION__, cSaver.c_str());
			unload_library(nLib);
		}
	} catch(...) {
	}
	
	return EOK;
}

status_t Engine::UnloadSaver( void )
{
	CALLED();
	
	int32 nLib;
	
	m_pcSaver->StopSaver();
	m_pcSaver->StopConfig();
	nLib = m_pcSaver->GetLibHandle();
	delete m_pcSaver;
	m_pcSaver = NULL;
	
	unload_library(nLib);
	
	return EOK;
}

void Engine::Start( void )
{
	CALLED();
}

void Engine::SetView( View *pcView )
{
	CALLED();
	
	m_pcView = pcView;
}

status_t Engine::StartSaver( bool bPreview )
{
	CALLED();
	
	if(m_pcSaver)
		return( m_pcSaver->StartSaver(m_pcView, bPreview) );
	
	dbprintf("%s: No screensaver loaded\n", __PRETTY_FUNCTION__);
	
	return -ENXIO;
}

void Engine::StopSaver( void )
{
	CALLED();
	
	if(m_pcSaver)
		return( m_pcSaver->StopSaver() );
}

void Engine::Draw( int32 nFrame )
{
	CALLED();
}

void Engine::StartConfig( View *pcConfigView )
{
	CALLED();
	
	if(m_pcSaver)
		return( m_pcSaver->StartConfig(pcConfigView) );
}

void Engine::StopConfig( void )
{
	CALLED();
	
	if(m_pcSaver)
		return ( m_pcSaver->StopConfig() );
}

status_t Engine::SaveState( Message *pcState) const
{
	CALLED();
	
	return EOK;
}

void Engine::SupplyInfo( Message *pcInfo ) const
{
	CALLED();
}

void Engine::ModulesChanged( const Message *pcInfo )
{
	CALLED();
}

