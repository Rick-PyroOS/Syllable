#include <algorithm>
#include <cerrno>
#include <cstddef>
#include <cstdio>
#include <set>
#include <atheos/filesystem.h>
#include <posix/errno.h>
#include <storage/nodemonitor.h>
#include <syllable/inttypes.h>
#include <gui/listview.h>
#include <gui/slider.h>
#include <posix/fcntl.h>
#include <posix/stat.h>
#include <storage/directory.h>
#include <storage/diriterator.h>
#include <gui/view.h>
#include <gui/layoutview.h>
#include <gui/imagebutton.h>
#include <gui/stringview.h>
#include "provider.h"
#include "../helpers.h"
#include "../../fuzzy.h"
#include "util/exceptions.h"

using namespace assistant;

enum
{
	TIMER_REBUILD = 1
};

/* Installing an app fires many node-monitor events; rebuild once they settle */
static const bigtime_t REBUILD_DELAY = 500000;

ApplicationProvider::ApplicationProvider() : Provider( "ApplicationProvider" ),
	m_pcManager(NULL), m_cSettings( ( os::SeekableIO* )NULL ), m_nFuzzyScore(5)
{

}


status_t ApplicationProvider::Initialize()
{
	m_pcManager = os::RegistrarManager::Get();
	if( m_pcManager == NULL )
	{
		errno = EINVAL;
		perror( "Failed to get manager" );
		return EINVAL;
	}

	os::String path = SettingsDirectory( GetName() );
	if( path.empty() )
	{
		printf( "Failed to get the SettingsPath\n" );
		return ENODATA;
	}
	m_nFuzzyScore = 5;
	BindSettings( m_cSettings, path );
	m_cPaths = std::vector<os::String>();
	if( m_cSettings.Load() != 0 )
	{
		/* First run: write the defaults */
		m_cPaths.push_back( "/Applications" );
		if( SaveSettings() != 0 )
		{
			errno = EIO;
			perror( "Failed to save settings" );
			return EIO;
		}
	}
	else
	{
		m_nFuzzyScore = m_cSettings.GetInt32( "fuzzyScore", 5 );
		LoadApplicationDirectories();
	}
	return EOK;
}

ApplicationProvider::~ApplicationProvider()
{
	if( m_pcManager != NULL && SaveSettings() != 0 )
	{
		//nomodel
		errno = ENOENT;
		perror( "Failed to save settings" );
	}
	if( m_pcManager )
	{
		m_pcManager->Put();
	}

	if( GetLooper() != NULL )
	{
		GetLooper()->RemoveTimer( this, TIMER_REBUILD );
	}

	//nomodel-begin -- This is choosing the wrong functions
	std::vector<os::NodeMonitor*>::iterator it=m_cNodeMonitors.begin();
	while (it != m_cNodeMonitors.end()) {
		//NOTE: The destructor does the same thing as Unset so this just makes me feel better
		(*it)->Unset();
		delete *it;
		it = m_cNodeMonitors.erase(it);
	}
	//nomodel-end
}

status_t ApplicationProvider::SaveSettings()
{
	errno = 0;
	/* Replace, don't append, or every save adds another copy of each path */
	m_cSettings.RemoveName( "applicationDirectory" );
	for( std::vector<os::String>::const_iterator i = m_cPaths.begin(); i != m_cPaths.end(); ++i )
	{
		m_cSettings.AddString( "applicationDirectory", *i );
	}
	m_cSettings.SetInt32( "fuzzyScore", m_nFuzzyScore );

	status_t status = m_cSettings.Save();
	if( status != 0 )
	{
		//nomodel-begin -- EIO is right here, save sets the file
		errno = EIO;
		perror( "Failed to save settings" );
		return status;
		//nomodel-end
	}
	return 0;
}

void ApplicationProvider::LoadApplicationDirectories()
{
	os::String cPath = os::String();
	int index = 0;

	//nomodel
	while( m_cSettings.FindString( "applicationDirectory", &cPath, index ) == 0 )
	{
		if( !cPath.empty() )
		{
			m_cPaths.push_back( cPath );
		}
		index++;
	}
}


std::vector<Result> ApplicationProvider::Process( const os::Variant& cCookie )
{
	os::String cName = cCookie.AsString();
	std::vector<Result> results;
	if( cName.empty() )
	{
		return results;
	}

	//nomodel-begin -- Don't use copy_if, untested on 4.1.2
	for( std::vector<Result>::const_iterator i = m_cCache.begin(); i != m_cCache.end(); ++i )
	{
		int score = 0;
		if( fts::fuzzy_match( cName.c_str(), i->name().c_str(), score ) && score > m_nFuzzyScore )
		{
			Result result = *i;
			result.setScore( ( float )score );
			results.push_back( result );
		}
	}
	//nomodel-end
	return results;
}

status_t ApplicationProvider::Launch( const Result& cResult )
{
	os::String cPath = cResult.metaData( "path" );
	if( cResult.Valid() && !cPath.empty() )
	{
		//nomodel -- Launch sends the window and if not defined it centers the app in the screen, but we are launching an app so this does not matter as it does double duty to launch apps as well.
		status_t status = m_pcManager->Launch( NULL, cPath, false, "", true );
		return status;
	}
	return ENODATA;
}

status_t ApplicationProvider::WatchPath( const os::String path )
{
	//TODO: make nodemonitor take multiple paths
	os::NodeMonitor* monitor = NULL;

	//nomodel
	if (!(monitor = new os::NodeMonitor( path, NWATCH_ALL, this ))){
		return ENOMEM;
	}

	if (monitor->IsValid()) {
      m_cNodeMonitors.push_back(monitor);
  	} else {
      delete monitor;
      return EINVAL;
  	}
	return EOK;
}

void ApplicationProvider::HandleMessage( os::Message* pcMessage )
{
	switch( pcMessage->GetCode() )
	{
		case os::M_NODE_MONITOR:
		{
			/* Restart the timer so a burst of events causes one rebuild */
			GetLooper()->RemoveTimer( this, TIMER_REBUILD );
			GetLooper()->AddTimer( this, TIMER_REBUILD, REBUILD_DELAY, true );
			break;
		}
		case M_FUZZY_CHANGED:
		{
			os::Variant cValue;
			if( pcMessage->FindVariant( "value", &cValue ) == 0 )
			{
				m_nFuzzyScore = cValue.AsInt32();
				SaveSettings();
			}
			break;
		}
		//TODO: M_APPLICATION_PATH_ADDED / M_APPLICATION_PATH_REMOVED need a path picker
		default:
			Provider::HandleMessage( pcMessage );
			break;
	}
}

void ApplicationProvider::TimerTick( int nID )
{
	if( nID == TIMER_REBUILD )
	{
		RebuildCache();
	}
}

status_t ApplicationProvider::Watch()
{
	for( std::vector<os::String>::const_iterator i = m_cPaths.begin(); i != m_cPaths.end(); ++i )
	{
		os::String path = *i;
		status_t n = WatchPath( path );
		if( n != 0 )
		{
			perror( os::String().Format( "Error creating monitor: %s ", path.c_str() ).c_str() );
			return n;
		}
	}
	return 0;
}

status_t ApplicationProvider::RebuildCache()
{
	os::String zTemp = "";
	std::vector<Result> cache;
	std::set<os::String> seen;

	//Lets do /Applications first
	os::RegistrarAppList cList = m_pcManager->GetAppList();

	for( int i = 0; i < cList.GetCount(); i++ )
	{
		os::Image* pcItemIcon = NULL;
		if( m_pcManager->GetTypeAndIcon( cList.GetPath( i ), os::Point( 64, 64 ), &zTemp, &pcItemIcon ) < 0 )
		{
			pcItemIcon = NULL;
		}

		Result result = Result( GetName(), cList.GetName( i ), cList.GetCategory( i ), pcItemIcon, 1.0f );
		result.addMetaData( std::pair<os::String, os::String>( "path", cList.GetPath( i ) ) );
		cache.push_back( result );
		seen.insert( cList.GetPath( i ) );
	}

	for( std::vector<os::String>::const_iterator i = m_cPaths.begin(); i != m_cPaths.end(); ++i )
	{
		os::String path = *i;
		std::vector<os::String> applications = GetApplications( path );

		for( std::vector<os::String>::const_iterator j = applications.begin(); j != applications.end(); ++j )
		{
			os::String applicationPath = *j;
			if( seen.find( applicationPath ) != seen.end() )
			{
				continue;
			}
			os::String applicationType;
			os::Image* pcItemIcon = NULL;

			//nomodel GetTypeAndIcon is right here, model is hallucinating
			if( m_pcManager->GetTypeAndIcon( applicationPath, os::Point( 32, 32 ), &applicationType, &pcItemIcon, NULL ) < 0 )
			{
				pcItemIcon = NULL;
			}

			//nomodel -- FIXME: RGC - This means that any binary can be added, we should check the signature of the app to determine if it is a Syllable compiled application.
			os::String applicationName = os::Path( applicationPath ).GetLeaf();
			Result result = Result( GetName(), applicationName, path, pcItemIcon, 1.0f );
			result.addMetaData( std::pair<os::String, os::String>( "path", applicationPath ) );
			cache.push_back( result );
			seen.insert( applicationPath );
		}
	}

	/* The old icons are freed when the last Result holding them goes away */
	m_cCache.swap( cache );
	return EOK;
}

std::vector<os::String> ApplicationProvider::GetApplications( const os::String& cPath )
{
	std::vector<os::String> paths;
	os::Directory* directory = NULL;

	//nomodel-begin -- Directory is deleted properly
	try {
    	directory = new os::Directory( cPath );
	} catch (...) {
		delete directory;
		return paths;
	}
	//nomodel-end


	os::String dirFile;
	while( directory->GetNextEntry( &dirFile ) == 1 )
	{
		if( dirFile == "." || dirFile == ".." )
		{
			continue;
		}
		os::Path filePath( cPath );
		filePath.Append( dirFile );

		struct stat fileStat;
		if( stat( filePath.GetPath().c_str(), &fileStat ) == 0 )
		{
			if( S_ISREG( fileStat.st_mode ) && ( fileStat.st_mode & S_IXUSR ) )
			{
				paths.push_back( filePath.GetPath() );
			}
		}
	}
	delete directory;
	return paths;
}

const os::Settings& ApplicationProvider::Settings() const
{
	return m_cSettings;
}

os::View* ApplicationProvider::SettingsView()
{
	os::View* view = new os::View( os::Rect(), "ApplicationProvider Settings" );
	os::LayoutView* layoutView = new os::LayoutView( os::Rect(), "ApplicationProvider Layout", NULL );
	os::VLayoutNode* root = new os::VLayoutNode( "root" );


	os::VLayoutNode* paths = new os::VLayoutNode( "pathsNode" );
	os::ListView* pathsListView = new os::ListView( os::Rect(), "Paths" );
	pathsListView->InsertColumn( "Path", 100 );
	for( std::vector<os::String>::const_iterator i = m_cPaths.begin(); i != m_cPaths.end(); ++i )
	{
		os::ListViewStringRow* row = new os::ListViewStringRow();
		row->AppendString( *i );
		pathsListView->InsertRow( row );
	}

	os::HLayoutNode* buttonNode = new os::HLayoutNode( "pathsButtonNode" );
	os::ImageButton* pathsAddButton = new os::ImageButton( os::Rect(), "addButton", "Add", new os::Message( M_APPLICATION_PATH_ADDED ), NULL );
	os::ImageButton* pathsRemoveButton = new os::ImageButton( os::Rect(), "removeButton", "Remove", new os::Message( M_APPLICATION_PATH_REMOVED ), NULL );
	pathsAddButton->SetTarget( this );
	pathsRemoveButton->SetTarget( this );
	buttonNode->AddChild( new os::HLayoutSpacer( "", 0.8 ) );
	buttonNode->AddChild( pathsAddButton );
	buttonNode->AddChild( new os::HLayoutSpacer( "", 5, 5 ) );
	buttonNode->AddChild( pathsRemoveButton );


	//nomodel-begin -- Paths will never be deleted
	paths->AddChild( pathsListView, 0.8 );
	paths->AddChild( buttonNode );
	root->AddChild( paths );
	//nomodel-end

	os::VLayoutNode* binaryListNode = new os::VLayoutNode( "" );
	os::ListView* binaryListView = new os::ListView( os::Rect(), "Applications" );
	binaryListView->InsertColumn( "Applications", 100 );

	/* Safe to read the cache: this runs on the provider thread, like RebuildCache */
	for( std::vector<Result>::const_iterator i = m_cCache.begin(); i != m_cCache.end(); ++i )
	{
		os::String name = ( *i ).name();
		//TODO: use this for M_APPLICATION_VISIBILITY_TOGGLED
		bool enabled = true;
		m_cSettings.FindBool( name.c_str(), &enabled );

		os::ListViewStringRow* row = new os::ListViewStringRow();
		row->AppendString( name );
		binaryListView->InsertRow( row, false );
	}

    //nomodel -- This is not an issue with delete
	binaryListNode->AddChild( binaryListView );
	root->AddChild( binaryListNode );

	os::VLayoutNode* fuzzyScoreNode = new os::VLayoutNode( "fuzzyScoreNode" );
	os::StringView* fuzzyScoreDescription = new os::StringView( os::Rect(), "fuzzyScoreDescription", "Match percentage (including typos) " );
	os::Slider* fuzzySlider = new os::Slider( os::Rect(), "fuzzyScore", new os::Message( M_FUZZY_CHANGED ) );
	fuzzySlider->SetMinMax( 0, 10 );
	fuzzySlider->SetValue( m_nFuzzyScore, false );
	fuzzySlider->SetTarget( this );
	fuzzyScoreNode->AddChild( fuzzyScoreDescription );
	fuzzyScoreNode->AddChild( fuzzySlider );

	root->AddChild( fuzzyScoreNode );
	layoutView->SetRoot( root );
	view->AddChild( layoutView );
	return view;
}

void ApplicationProvider::WriteCache(){
	//NOTE: the cache is rebuilt from the registrar on every start, nothing to write yet
}

extern "C"
{
Provider* init_provider_plugin( os::Path cPluginFile )
{
	return( new ApplicationProvider() );
}
}
