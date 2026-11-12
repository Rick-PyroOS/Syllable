/*  SettingsCache - Settings kept in an SQLite database.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of version 2 of the GNU Library
 *  General Public License as published by the Free Software
 *  Foundation.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public
 *  License along with this library; if not, write to the Free
 *  Software Foundation, Inc., 59 Temple Place - Suite 330, Boston,
 *  MA 02111-1307, USA
 */

#ifndef	__F_UTIL_SETTINGSCACHE_H__
#define	__F_UTIL_SETTINGSCACHE_H__

#include <util/message.h>
#include <storage/file.h>
#include <storage/path.h>
#include <storage/seekableio.h>

namespace os
{

/** Settings kept in an SQLite database.
 * \ingroup util
 * \par Description:
 *	SettingsCache has the same API as os::Settings and is used the same
 *	way: Load() fills the object (an os::Message) from storage, the Get
 *	and Set methods work on the copy in memory, and Save() writes it
 *	back.  Switching a program from Settings to SettingsCache is a change
 *	of the class name.
 * \par
 *	Where Settings writes a flattened os::Message to a file, SettingsCache
 *	keeps one row per value in an SQLite database, \c settings.db in the
 *	settings directory (by default ~/Settings/<application>/).  Every
 *	settings file name of that directory (SetFile(), by default
 *	"Settings") is a set of rows in the same database.  Save() replaces
 *	the rows in one transaction, so a crash or a full disk leaves the old
 *	settings rather than a truncated file.  Numbers and strings are stored
 *	as SQL values, so the database can be read with the sqlite3 shell:
 * \code
 *	sqlite3 ~/Settings/Webster/settings.db "SELECT * FROM settings"
 * \endcode
 * \par
 *	The first Load() of a file that has no rows yet reads the Settings file
 *	of the same name, if there is one, so the program keeps its settings.
 *	The next Save() writes them to the database; the old file is left
 *	alone.
 * \par
 *	Given a stream (the SeekableIO constructor or SetFile( SeekableIO* )),
 *	SettingsCache reads and writes the Settings file format on it, as
 *	Settings does: an SQLite database needs a file of its own.
 * \par
 *	The object itself is not locked; use it from one thread at a time, as
 *	Settings.  Load() and Save() may run in several threads at once.
 *
 * \sa os::Settings, os::Message
 *****************************************************************************/

class SettingsCache : public Message
{
public:
    SettingsCache();
    SettingsCache( SeekableIO *pcFile );
    SettingsCache( const SettingsCache& cSource );
    ~SettingsCache();

    status_t	Load( void );
    status_t	Save( void ) const;

    SettingsCache&	operator=( const SettingsCache& cSource );
    SettingsCache&	operator=( const Message& cSource );

	Path	GetPath() const;
	File	GetFile() const;
	void	SetPath( Path* pcPath );
	void	SetFile( SeekableIO* pcFile );
	void	SetFile( String cFilename );

    String	GetString( const char* pzName, const char* pzDefault = "", int nIndex = 0 ) const;
    int8	GetInt8( const char* pzName, int8 nDefault, int nIndex = 0 ) const;
    int16	GetInt16( const char* pzName, int16 nDefault, int nIndex = 0 ) const;
    int32	GetInt32( const char* pzName, int32 nDefault, int nIndex = 0 ) const;
    int64	GetInt64( const char* pzName, int64 nDefault, int nIndex = 0 ) const;
    bool	GetBool( const char* pzName, bool bDefault, int nIndex = 0 ) const;
    float	GetFloat( const char* pzName, float vDefault, int nIndex = 0 ) const;
    double	GetDouble( const char* pzName, double vDefault, int nIndex = 0 ) const;
    Rect	GetRect( const char* pzName, const Rect& cDefault, int nIndex = 0 ) const;
    IRect	GetIRect( const char* pzName, const IRect& cDefault, int nIndex = 0 ) const;
    Point	GetPoint( const char* pzName, const Point& cDefault, int nIndex = 0 ) const;
    IPoint	GetIPoint( const char* pzName, const IPoint& cDefault, int nIndex = 0 ) const;
    Color32_s	GetColor32( const char* pzName, const Color32_s& cDefault, int nIndex = 0 ) const;
    Variant	GetVariant( const char* pzName, const Variant& cDefault, int nIndex = 0 ) const;
	// As in Settings, GetMessage() and GetPointer() are left out: use
	// Message::FindMessage().

    status_t	SetData( const char* pzName, int nType, const void* pData, uint32 nSize, int nIndex = 0, bool bFixedSize = true, int nMaxCountHint = 1 );
    status_t	SetString( const char* pzName, const String& cValue, int nIndex = 0 );
    status_t	SetInt8( const char* pzName, int8 cValue, int nIndex = 0 );
    status_t	SetInt16( const char* pzName, int16 cValue, int nIndex = 0 );
    status_t	SetInt32( const char* pzName, int32 cValue, int nIndex = 0 );
    status_t	SetInt64( const char* pzName, int64 cValue, int nIndex = 0 );
    status_t	SetBool( const char* pzName, bool cValue, int nIndex = 0 );
    status_t	SetFloat( const char* pzName, float cValue, int nIndex = 0 );
    status_t	SetDouble( const char* pzName, double cValue, int nIndex = 0 );
    status_t	SetRect( const char* pzName, const Rect& cValue, int nIndex = 0 );
    status_t	SetIRect( const char* pzName, const IRect& cValue, int nIndex = 0 );
    status_t	SetPoint( const char* pzName, const Point& cValue, int nIndex = 0 );
    status_t	SetIPoint( const char* pzName, const IPoint& cValue, int nIndex = 0 );
    status_t	SetColor32( const char* pzName, const Color32_s& cValue, int nIndex = 0 );
    status_t	SetMessage( const char* pzName, const Message& cValue, int nIndex = 0 );
    status_t	SetVariant( const char* pzName, const Variant& cValue, int nIndex = 0 );

private:
    class Private;
    Private* m;
};


} // end of namespace

#endif // __F_UTIL_SETTINGSCACHE_H__
