/* Host shim: Syllable finds an application's catalogs in its own resources
 * (Resources(image id)), which a Linux binary does not have.  This Locale
 * finds nothing, so callers take their no-catalog path. */
#ifndef __F_UTIL_LOCALE_H__
#define __F_UTIL_LOCALE_H__

#include <errno.h>
#include <util/string.h>
#include <storage/streamableio.h>

namespace os {

class Locale
{
public:
    Locale() {}
    StreamableIO* GetResourceStream( const String& cName ) { errno = ENOSYS; return( NULL ); }
    StreamableIO* GetLocalizedResourceStream( const String& cName ) { errno = ENOSYS; return( NULL ); }
};

} // end of namespace

#endif /* __F_UTIL_LOCALE_H__ */
