#include "settings.h"
#include "atheos/filesystem.h"
#include "storage/nodemonitor.h"
#include "util/handler.h"
#include "util/message.h"
#include "util/settings.h"
#include "messages.h"
#include "provider_thread.h"
#include "providers/helpers.h"

// Define and initialize the static pointer
SettingsHandler* SettingsHandler::s_mpcInstance = NULL;


SettingsHandler::SettingsHandler() : os::Handler( "SettingsHandler" ), m_cSettings( ( os::SeekableIO* )NULL ), m_pcWatchMonitor( NULL ), m_pcFileMonitor( NULL )
{
	BindSettings( m_cSettings, SettingsDirectory() );
	m_cSettings.Load();
	s_mpcInstance = this;
}

SettingsHandler::~SettingsHandler()
{
	delete m_pcFileMonitor;
	delete m_pcWatchMonitor;
	s_mpcInstance = NULL;
}

status_t SettingsHandler::Watch()
{
	os::String cPath = SettingsDirectory();
	status_t nError = MakeDirectories( cPath );
	if( nError != EOK )
	{
		return nError;
	}
	m_pcWatchMonitor = new os::NodeMonitor( cPath, NWATCH_ALL, this );
	if( !m_pcWatchMonitor->IsValid() )
	{
		delete m_pcWatchMonitor;
		m_pcWatchMonitor = NULL;
		return EINVAL;
	}
	_WatchFile();
	return EOK;
}

/* The directory monitor sees the file being created or replaced, this one sees it being rewritten */
void SettingsHandler::_WatchFile()
{
	if( m_pcFileMonitor != NULL && m_pcFileMonitor->IsValid() )
	{
		return;
	}
	delete m_pcFileMonitor;
	m_pcFileMonitor = NULL;

	os::String cFile = SettingsDirectory() + "/Settings";
	struct stat sStat;
	if( stat( cFile.c_str(), &sStat ) == 0 )
	{
		m_pcFileMonitor = new os::NodeMonitor( cFile, NWATCH_STAT, this );
	}
}

const os::Settings& SettingsHandler::get() const
{
	return m_cSettings;
}

void SettingsHandler::HandleMessage( os::Message* pcMessage )
{
	switch( pcMessage->GetCode() )
	{
		case os::M_NODE_MONITOR:
			//reload
			m_cSettings.Load();
			_WatchFile();
			if( ProviderThread::GetInstance() != NULL )
			{
				ProviderThread::GetInstance()->PostMessage( M_SETTINGS_CHANGED, ProviderThread::GetInstance() );
			}
			break;
		default:
			Handler::HandleMessage( pcMessage );
			break;
	}
}
