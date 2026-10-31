#ifndef __COMMON_H__
#define __COMMON_H__

#include <util/string.h>

/* ~/Settings/assistant[/<provider>]/Settings */
os::String GetSettingsPath( const os::String & provider = "" );
/* ~/Settings/assistant[/<provider>]/Cache */
os::String GetCachePath(const os::String& provider = "");
#endif
