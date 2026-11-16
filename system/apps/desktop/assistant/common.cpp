
#include "common.h"
#include "providers/helpers.h"

using namespace assistant;

/*
 * The path logic lives in providers/helpers.h, inline, so plugins can use it
 * too without linking against the executable.
 */

os::String GetSettingsPath( const os::String &provider )
{
	os::String cDirectory = SettingsDirectory( provider );
	if( cDirectory.empty() )
	{
		return os::String();
	}
	return cDirectory + "/Settings";
}

os::String GetCachePath( const os::String &provider )
{
	os::String cDirectory = SettingsDirectory( provider );
	if( cDirectory.empty() )
	{
		return os::String();
	}
	return cDirectory + "/Cache";
}
