#ifndef __PROVIDERS__HELPERS__H__
#define __PROVIDERS__HELPERS__H__

#include <stdlib.h>
#include <string.h>
#include <posix/stat.h>
#include <util/string.h>
#include "./provider.h"

/*
 * These are inline so a plugin .so can use them without linking against
 * anything in the Assistant executable.
 */

#define ASSISTANT_NAME "assistant"
#define ASSISTANT_PLUGIN_DIR "/system/extensions/assistant"

namespace assistant{
struct IsSame{
	os::String targetName;

	IsSame(const os::String& name): targetName(name){}

	bool operator()(const Provider* p) const {
		return p->GetName() == targetName;
	}
};

/* ~/Settings/assistant, or ~/Settings/assistant/<provider> */
inline os::String SettingsDirectory( const os::String& cProvider = "" )
{
	const char* pzHome = getenv( "HOME" );
	if( pzHome == NULL || pzHome[0] == '\0' )
	{
		return os::String();
	}

	os::String cPath = os::String( pzHome ) + "/Settings/" + ASSISTANT_NAME;
	if( !cPath.empty() && !cProvider.empty() )
	{
		cPath += os::String( "/" ) + cProvider;
	}
	return cPath;
}

/*
 * Point a Settings object at <cDirectory>/Settings. Construct the object
 * with os::Settings( (os::SeekableIO*)NULL ) first: the default constructor
 * works out the path itself, and binding it to an os::File throws when the
 * file does not exist yet. Save() creates the directories and the file.
 */
inline void BindSettings( os::Settings& cSettings, const os::String& cDirectory )
{
	os::Path cPath( cDirectory );
	cSettings.SetPath( &cPath );
	cSettings.SetFile( "Settings" );
}

/* mkdir -p */
inline status_t MakeDirectories( const os::String& cPath )
{
	if( cPath.empty() )
	{
		return EINVAL;
	}

	os::String cPart;
	const char* pzPath = cPath.c_str();
	for( const char* p = pzPath; ; ++p )
	{
		if( *p == '/' || *p == '\0' )
		{
			cPart = os::String( pzPath, p - pzPath );
			struct stat sStat;
			if( !cPart.empty() && stat( cPart.c_str(), &sStat ) < 0 && mkdir( cPart.c_str(), 0700 ) < 0 )
			{
				return errno;
			}
		}
		if( *p == '\0' )
		{
			break;
		}
	}
	return EOK;
}
}

#endif
