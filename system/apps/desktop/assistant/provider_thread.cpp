#include "provider_thread.h"
#include "posix/errno.h"
#include "providers/provider.h"
#include "providers/settings.h"
#include "providers/helpers.h"
#include "storage/directory.h"
#include "syllable/image.h"
#include "syllable/inttypes.h"
#include "util/typecodes.h"
#include "messages.h"
#include <sys/debug.h>
#include <algorithm>
#include <vector>

using namespace assistant;


ProviderThread* ProviderThread::s_pcInstance = NULL;


class ProviderThread::Private
{
public:
	struct Plugin
	{
		Provider* pcProvider;
		image_id nImage;
	};

	Private() : m_cSettings( ( os::SeekableIO* )NULL )
	{
	}

	~Private()
	{
	}

public:
	std::vector<Plugin> m_cPlugins;
	os::Settings m_cSettings;
};

ProviderThread::ProviderThread() : os::Looper( "ProviderThread" )
{
	m = new ProviderThread::Private;
	BindSettings( m->m_cSettings, SettingsDirectory() );
	m->m_cSettings.Load();
	s_pcInstance = this;
}

/* Runs on the provider thread when the looper quits */
ProviderThread::~ProviderThread()
{
	_UnloadPlugins();
	delete m;
	s_pcInstance = NULL;
}

ProviderThread* ProviderThread::GetInstance()
{
	return s_pcInstance;
}

void ProviderThread::HandleMessage( os::Message* pcMessage )
{
	switch( pcMessage->GetCode() )
	{
		case M_LOAD_PROVIDERS:
			_LoadPlugins();
			break;
		case M_QUERY:
			_Query( pcMessage );
			break;
		case M_LAUNCH:
			_Launch( pcMessage );
			break;
		case M_SETTINGS_CHANGED:
			//NOTE: a plugin that is disabled or enabled here takes effect on the next start
			m->m_cSettings.Load();
			break;
		case M_SHUTDOWN_PROVIDERS:
			_UnloadPlugins();
			if( pcMessage->IsSourceWaiting() )
			{
				pcMessage->SendReply( M_SHUTDOWN_PROVIDERS );
			}
			break;
		default:
			os::Looper::HandleMessage( pcMessage );
			break;
	}
}

status_t ProviderThread::_LoadPlugins()
{
	os::Directory* pcDirectory = NULL;
	try
	{
		pcDirectory = new os::Directory( ASSISTANT_PLUGIN_DIR );
	}
	catch( ... )
	{
		dbprintf( "Error: Cannot open plugin directory %s\n", ASSISTANT_PLUGIN_DIR );
		return ENOENT;
	}

	os::String cName;
	while( pcDirectory->GetNextEntry( &cName ) == 1 )
	{
		if( cName == "." || cName == ".." )
		{
			continue;
		}
		if( cName.size() <= 3 || cName.str().compare( cName.size() - 3, 3, ".so" ) != 0 )
		{
			dbprintf( "path is not a shared library: %s\n", cName.c_str() );
			continue;
		}

		os::Path cPath( ASSISTANT_PLUGIN_DIR );
		cPath.Append( cName );

		int nID = load_library( cPath.GetPath().c_str(), IM_ADDON_SPACE );
		if( nID < 0 )
		{
			dbprintf( "Error: Cannot load plugin %s\n", cPath.GetPath().c_str() );
			continue;
		}

		init_provider_plugin* pInit = NULL;
		if( get_symbol_address( nID, "init_provider_plugin", -1, ( void** )&pInit ) != 0 || pInit == NULL )
		{
			dbprintf( "Error: Plugin %s has no init_provider_plugin\n", cPath.GetPath().c_str() );
			unload_library( nID );
			continue;
		}

		Provider* pcPlugin = pInit( cPath );
		if( pcPlugin == NULL )
		{
			unload_library( nID );
			continue;
		}

		if( _Disabled( pcPlugin->GetName() ) )
		{
			delete pcPlugin;
			unload_library( nID );
			continue;
		}

		/* Attach first, so node monitors created in Initialize() and Watch() reach the plugin */
		AddHandler( pcPlugin );
		pcPlugin->SetApp( this, nID );

		if( pcPlugin->Initialize() != 0 )
		{
			dbprintf( "Error: Plugin %s failed to initialize\n", cPath.GetPath().c_str() );
			delete pcPlugin;
			unload_library( nID );
			continue;
		}

		pcPlugin->Watch();
		pcPlugin->RebuildCache();

		Private::Plugin sPlugin;
		sPlugin.pcProvider = pcPlugin;
		sPlugin.nImage = nID;
		m->m_cPlugins.push_back( sPlugin );
	}
	delete pcDirectory;
	return EOK;
}

void ProviderThread::_UnloadPlugins()
{
	for( std::vector<Private::Plugin>::iterator i = m->m_cPlugins.begin(); i != m->m_cPlugins.end(); ++i )
	{
		i->pcProvider->WriteCache();
		/* ~Handler removes it from this looper; deleting while attached lets it cancel its own timers first */
		delete i->pcProvider;
		unload_library( i->nImage );
	}
	m->m_cPlugins.clear();
}

bool ProviderThread::_Disabled( const os::String& cName )
{
	const PluginSettings* psSettings = NULL;
	size_t nSize = 0;
	if( m->m_cSettings.FindData( cName.c_str(), os::T_RAW, ( const void** )&psSettings, &nSize ) == 0 && nSize >= sizeof( PluginSettings ) )
	{
		return psSettings->disabled;
	}
	return false;
}

Provider* ProviderThread::_FindProvider( const os::String& cProviderName ) const
{
	for( std::vector<Private::Plugin>::const_iterator i = m->m_cPlugins.begin(); i != m->m_cPlugins.end(); ++i )
	{
		if( IsSame( cProviderName )( i->pcProvider ) )
		{
			return i->pcProvider;
		}
	}
	return NULL;
}

void ProviderThread::_Query( os::Message* pcMessage )
{
	os::String cInput;
	int32 nGeneration = 0;
	pcMessage->FindString( "input", &cInput );
	pcMessage->FindInt32( "generation", &nGeneration );

	std::vector<Result>* pcResults = new std::vector<Result>();
	if( !cInput.empty() )
	{
		for( std::vector<Private::Plugin>::const_iterator i = m->m_cPlugins.begin(); i != m->m_cPlugins.end(); ++i )
		{
			std::vector<Result> r = i->pcProvider->Process( cInput );
			pcResults->insert( pcResults->end(), r.begin(), r.end() );
		}
		std::stable_sort( pcResults->begin(), pcResults->end(), ResultScoreGreater );
	}

	os::Message cReply( M_RESULTS );
	cReply.AddInt32( "generation", nGeneration );
	cReply.AddPointer( "results", pcResults );
	if( pcMessage->SendReply( &cReply ) != 0 )
	{
		delete pcResults;
	}
}

void ProviderThread::_Launch( os::Message* pcMessage )
{
	os::String cProvider, cName, cDescription;
	pcMessage->FindString( "provider", &cProvider );
	pcMessage->FindString( "name", &cName );
	pcMessage->FindString( "description", &cDescription );

	Result cResult( cProvider, cName, cDescription );
	os::String cKey, cValue;
	for( int i = 0; pcMessage->FindString( "meta_key", &cKey, i ) == 0 && pcMessage->FindString( "meta_value", &cValue, i ) == 0; i++ )
	{
		cResult.addMetaData( std::pair<os::String, os::String>( cKey, cValue ) );
	}

	Provider* pcProvider = _FindProvider( cProvider );
	if( pcProvider == NULL )
	{
		dbprintf( "Error: No provider named %s\n", cProvider.c_str() );
		return;
	}
	if( pcProvider->Launch( cResult ) != 0 )
	{
		dbprintf( "Error: %s failed to launch %s\n", cProvider.c_str(), cName.c_str() );
	}
}
