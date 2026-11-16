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

#include <util/settingscache.h>
#include <util/application.h>
#include <util/variant.h>
#include <util/locker.h>
#include <storage/file.h>

#include <sqlite3.h>

#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <vector>

using namespace os;

// The file format of os::Settings (settings.cpp), for streams and for
// reading a program's old settings file
#define SETTINGS_MAGIC		0x53626C53
#define SETTINGS_VERSION	0x00000001

// GCC 4.1 gives an anonymous namespace external linkage: hidden keeps
// these helpers out of libsyllable's exported symbols
#pragma GCC visibility push(hidden)
namespace
{
	struct FileHeader
	{
		uint32 nMagic;
		uint32 nHeaderSize;
		uint32 nVersion;
		uint32 nSize;
	};

	// SQLite is built without its own mutexes (SQLITE_THREADSAFE=0, the
	// port's Makefile), so every use of it in libsyllable holds this lock
	Locker g_cSQLiteLock( "settingscache_lock" );

	const char* const DATABASE_NAME = "settings.db";

	const char* const SCHEMA =
		"PRAGMA user_version = 1;"
		"CREATE TABLE IF NOT EXISTS files ("
		"  file  TEXT PRIMARY KEY,"		// SetFile() name, "Settings" by default
		"  code  INTEGER NOT NULL DEFAULT 0,"	// Message::GetCode()
		"  saved INTEGER);"			// time of the last Save(), Unix seconds
		"CREATE TABLE IF NOT EXISTS settings ("
		"  file  TEXT NOT NULL,"
		"  name  TEXT NOT NULL,"
		"  type  INTEGER NOT NULL,"		// os type code (T_INT32, T_STRING...)
		"  idx   INTEGER NOT NULL,"		// index of the value under its name
		"  pos   INTEGER NOT NULL,"		// order of the names in the message
		"  fixed INTEGER NOT NULL,"		// AddData()'s bFixedSize
		"  value,"				// INTEGER, REAL, TEXT or BLOB, by type
		"  PRIMARY KEY ( file, name, idx ) );";

	status_t ReadFlattened( SeekableIO* pcIO, Message* pcMsg )
	{
		FileHeader sHeader;

		pcIO->Seek( 0, SEEK_SET );
		if( pcIO->Read( &sHeader, sizeof( sHeader ) ) != sizeof( sHeader ) )
			return -1;
		if( sHeader.nMagic != SETTINGS_MAGIC || sHeader.nVersion > SETTINGS_VERSION )
			return -1;
		if( sHeader.nSize == 0 || sHeader.nSize >= 0x800000 )
			return -1;
		pcIO->Seek( sHeader.nHeaderSize, SEEK_SET );

		std::vector<uint8> cBuffer( sHeader.nSize );
		if( ( uint32 )pcIO->Read( &cBuffer[0], sHeader.nSize ) != sHeader.nSize )
			return -1;
		pcMsg->Unflatten( &cBuffer[0] );
		return 0;
	}

	status_t WriteFlattened( SeekableIO* pcIO, const Message* pcMsg )
	{
		FileHeader sHeader;

		sHeader.nMagic = SETTINGS_MAGIC;
		sHeader.nVersion = SETTINGS_VERSION;
		sHeader.nHeaderSize = sizeof( sHeader );
		sHeader.nSize = pcMsg->GetFlattenedSize();

		std::vector<uint8> cBuffer( sHeader.nSize );
		pcMsg->Flatten( &cBuffer[0], sHeader.nSize );

		pcIO->Seek( 0, SEEK_SET );
		if( pcIO->Write( &sHeader, sizeof( sHeader ) ) != sizeof( sHeader ) )
			return -1;
		if( ( uint32 )pcIO->Write( &cBuffer[0], sHeader.nSize ) != sHeader.nSize )
			return -1;
		return 0;
	}

	bool IsVariableSize( int nType )
	{
		return nType == T_STRING || nType == T_MESSAGE || nType == T_VARIANT || nType == T_RAW;
	}

	/* An open database, closed when it goes out of scope */
	class Database
	{
	public:
		Database() : m_psDB( NULL ) {}
		~Database() { Close(); }

		bool Open( const String& cPath, bool bWrite )
		{
			int nFlags = bWrite ? ( SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE ) : SQLITE_OPEN_READONLY;
			if( sqlite3_open_v2( cPath.c_str(), &m_psDB, nFlags, NULL ) != SQLITE_OK )
			{
				Close();
				return false;
			}
			// Another program (or thread) may be saving: wait for it
			sqlite3_busy_timeout( m_psDB, 5000 );
			return true;
		}

		void Close()
		{
			if( m_psDB != NULL )
				sqlite3_close( m_psDB );
			m_psDB = NULL;
		}

		bool Exec( const char* pzSQL )
		{
			return sqlite3_exec( m_psDB, pzSQL, NULL, NULL, NULL ) == SQLITE_OK;
		}

		sqlite3* Get() const { return m_psDB; }

	private:
		sqlite3* m_psDB;
	};

	/* A prepared statement, finalized when it goes out of scope */
	class Statement
	{
	public:
		Statement( const Database& cDB, const char* pzSQL ) : m_psStmt( NULL )
		{
			if( sqlite3_prepare_v2( cDB.Get(), pzSQL, -1, &m_psStmt, NULL ) != SQLITE_OK )
				m_psStmt = NULL;
		}
		~Statement()
		{
			if( m_psStmt != NULL )
				sqlite3_finalize( m_psStmt );
		}
		bool IsValid() const { return m_psStmt != NULL; }
		sqlite3_stmt* Get() const { return m_psStmt; }
		void BindText( int nCol, const String& cText )
		{
			sqlite3_bind_text( m_psStmt, nCol, cText.c_str(), cText.size(), SQLITE_TRANSIENT );
		}
		int Step() { return sqlite3_step( m_psStmt ); }
		bool Run()
		{
			int nResult = sqlite3_step( m_psStmt );
			sqlite3_reset( m_psStmt );
			return nResult == SQLITE_DONE;
		}

	private:
		sqlite3_stmt* m_psStmt;
	};

	/* Binds one value of a message as the SQL value its type suits best */
	void BindValue( sqlite3_stmt* psStmt, int nCol, int nType, const void* pData, size_t nSize )
	{
		switch( nType )
		{
			case T_INT8:
				sqlite3_bind_int64( psStmt, nCol, *( const int8* )pData );
				return;
			case T_INT16:
				sqlite3_bind_int64( psStmt, nCol, *( const int16* )pData );
				return;
			case T_INT32:
				sqlite3_bind_int64( psStmt, nCol, *( const int32* )pData );
				return;
			case T_INT64:
				sqlite3_bind_int64( psStmt, nCol, *( const int64* )pData );
				return;
			case T_BOOL:
				sqlite3_bind_int64( psStmt, nCol, *( const bool* )pData ? 1 : 0 );
				return;
			case T_FLOAT:
				sqlite3_bind_double( psStmt, nCol, *( const float* )pData );
				return;
			case T_DOUBLE:
				sqlite3_bind_double( psStmt, nCol, *( const double* )pData );
				return;
			case T_STRING:
			{
				// Stored without its terminating NUL
				size_t nLen = nSize;
				if( nLen > 0 && ( ( const char* )pData )[nLen - 1] == '\0' )
					nLen--;
				sqlite3_bind_text( psStmt, nCol, ( const char* )pData, nLen, SQLITE_TRANSIENT );
				return;
			}
			default:
				sqlite3_bind_blob( psStmt, nCol, pData, nSize, SQLITE_TRANSIENT );
				return;
		}
	}

	/* Adds the value in column nCol of the current row to the message */
	status_t AddValue( Message* pcMsg, const char* pzName, int nType, bool bFixed, sqlite3_stmt* psStmt, int nCol )
	{
		switch( nType )
		{
			case T_INT8:
			{
				int8 v = ( int8 )sqlite3_column_int64( psStmt, nCol );
				return pcMsg->AddData( pzName, nType, &v, sizeof( v ), true );
			}
			case T_INT16:
			{
				int16 v = ( int16 )sqlite3_column_int64( psStmt, nCol );
				return pcMsg->AddData( pzName, nType, &v, sizeof( v ), true );
			}
			case T_INT32:
			{
				int32 v = ( int32 )sqlite3_column_int64( psStmt, nCol );
				return pcMsg->AddData( pzName, nType, &v, sizeof( v ), true );
			}
			case T_INT64:
			{
				int64 v = sqlite3_column_int64( psStmt, nCol );
				return pcMsg->AddData( pzName, nType, &v, sizeof( v ), true );
			}
			case T_BOOL:
			{
				bool v = sqlite3_column_int64( psStmt, nCol ) != 0;
				return pcMsg->AddData( pzName, nType, &v, sizeof( v ), true );
			}
			case T_FLOAT:
			{
				float v = ( float )sqlite3_column_double( psStmt, nCol );
				return pcMsg->AddData( pzName, nType, &v, sizeof( v ), true );
			}
			case T_DOUBLE:
			{
				double v = sqlite3_column_double( psStmt, nCol );
				return pcMsg->AddData( pzName, nType, &v, sizeof( v ), true );
			}
			case T_STRING:
			{
				const unsigned char* pzText = sqlite3_column_text( psStmt, nCol );
				int nLen = sqlite3_column_bytes( psStmt, nCol );
				std::string cValue;
				if( pzText != NULL )
					cValue.assign( ( const char* )pzText, nLen );
				return pcMsg->AddData( pzName, nType, cValue.c_str(), cValue.size() + 1, false );
			}
			default:
			{
				static const uint8 nEmpty = 0;	// AddData() wants a pointer, even for no bytes
				const void* pData = sqlite3_column_blob( psStmt, nCol );
				int nSize = sqlite3_column_bytes( psStmt, nCol );
				if( pData == NULL )
					pData = &nEmpty;
				return pcMsg->AddData( pzName, nType, pData, nSize, bFixed );
			}
		}
	}
}
#pragma GCC visibility pop

/** \internal */
class SettingsCache::Private
{
public:
	Private()
	{
		m_pcIO = NULL;
		m_bUserStream = false;
	}

	~Private()
	{
		if( m_pcIO )
			delete m_pcIO;
	}

	// As Settings: the application's name, from its signature
	String GetAppName()
	{
		Application *pApp = Application::GetInstance();

		if( pApp )
		{
			const char *pzAppSig = pApp->GetName().c_str();
			const char *pzAppName = strrchr( pzAppSig, '-' );

			if( pzAppName != NULL )
				pzAppName++;
			else if( strrchr( pzAppSig, '/' ) != NULL )
				pzAppName = strrchr( pzAppSig, '/' ) + 1;
			else
				pzAppName = pzAppSig;

			if( pzAppName && strlen( pzAppName ) > 1 )
				return String( pzAppName );
		}

		throw "Error! :(";
	}

	void CreatePath( const String & cPath )
	{
		std::string cDirPath;
		const char *pzPath = cPath.c_str();
		std::vector<char> cBuffer( pzPath, pzPath + strlen( pzPath ) + 1 );

		if( cBuffer[0] == '/' )
			cDirPath = "/";

		char *pzPart = strtok( &cBuffer[0], "/" );
		while( pzPart )
		{
			struct stat sStat;

			cDirPath += pzPart;
			if( stat( cDirPath.c_str(), &sStat ) < 0 )
				mkdir( cDirPath.c_str(), 0700 );
			pzPart = strtok( NULL, "/" );
			cDirPath += "/";
		}
	}

	void SetPath( const String & cFile = "", const String & cPath = "" )
	{
		if( cFile == "" )
			m_cFile = "Settings";
		else
			m_cFile = cFile;
		if( cPath == "" )
		{
			const char* pzHome = getenv( "HOME" );
			m_cPath = pzHome ? pzHome : "";
			m_cPath += String( "/Settings/" ) + GetAppName() + String( "/" );
		}
		else
		{
			m_cPath = cPath;
		}
	}

	void SetIO( SeekableIO * pcIO )
	{
		if( m_pcIO && m_pcIO != pcIO )
			delete m_pcIO;

		m_pcIO = pcIO;
		m_bUserStream = m_pcIO != NULL;
	}

	String GetDatabasePath() const
	{
		return m_cPath + DATABASE_NAME;
	}

	status_t LoadDatabase( Message* pcMsg );
	status_t SaveDatabase( const Message* pcMsg );
	status_t LoadOldFile( Message* pcMsg );

	SeekableIO *m_pcIO;
	bool m_bUserStream;
	String m_cFile;
	String m_cPath;
};

/* Reads this file's rows from the database.  1: the database has none */
status_t SettingsCache::Private::LoadDatabase( Message* pcMsg )
{
	struct stat sStat;
	if( stat( GetDatabasePath().c_str(), &sStat ) < 0 )
		return 1;

	Database cDB;
	if( !cDB.Open( GetDatabasePath(), false ) )
		return -1;

	Statement cFile( cDB, "SELECT code FROM files WHERE file = ?" );
	if( !cFile.IsValid() )
		return 1;	// no tables yet
	cFile.BindText( 1, m_cFile );
	int nResult = cFile.Step();
	if( nResult == SQLITE_DONE )
		return 1;
	if( nResult != SQLITE_ROW )
		return -1;
	int nCode = sqlite3_column_int( cFile.Get(), 0 );

	Statement cRows( cDB, "SELECT name, type, fixed, value FROM settings WHERE file = ? ORDER BY pos, idx" );
	if( !cRows.IsValid() )
		return -1;
	cRows.BindText( 1, m_cFile );

	Message cLoaded( nCode );
	while( ( nResult = cRows.Step() ) == SQLITE_ROW )
	{
		const char* pzName = ( const char* )sqlite3_column_text( cRows.Get(), 0 );
		int nType = sqlite3_column_int( cRows.Get(), 1 );
		bool bFixed = sqlite3_column_int( cRows.Get(), 2 ) != 0;
		if( pzName != NULL )
			AddValue( &cLoaded, pzName, nType, bFixed, cRows.Get(), 3 );
	}
	if( nResult != SQLITE_DONE )
		return -1;

	*pcMsg = cLoaded;
	return 0;
}

/* Replaces this file's rows in the database, in one transaction */
status_t SettingsCache::Private::SaveDatabase( const Message* pcMsg )
{
	CreatePath( m_cPath );

	Database cDB;
	if( !cDB.Open( GetDatabasePath(), true ) )
		return -1;
	if( !cDB.Exec( SCHEMA ) )
		return -1;
	// IMMEDIATE: take the write lock now, so a busy database waits here
	// (busy timeout) rather than failing halfway through
	if( !cDB.Exec( "BEGIN IMMEDIATE" ) )
		return -1;

	bool bOk = true;
	{
		Statement cDelete( cDB, "DELETE FROM settings WHERE file = ?" );
		Statement cFile( cDB, "INSERT OR REPLACE INTO files ( file, code, saved ) VALUES ( ?, ?, strftime( '%s', 'now' ) )" );
		Statement cInsert( cDB, "INSERT OR REPLACE INTO settings ( file, name, type, idx, pos, fixed, value ) VALUES ( ?, ?, ?, ?, ?, ?, ? )" );
		bOk = cDelete.IsValid() && cFile.IsValid() && cInsert.IsValid();

		if( bOk )
		{
			cDelete.BindText( 1, m_cFile );
			bOk = cDelete.Run();
		}
		if( bOk )
		{
			cFile.BindText( 1, m_cFile );
			sqlite3_bind_int( cFile.Get(), 2, pcMsg->GetCode() );
			bOk = cFile.Run();
		}

		int nNames = pcMsg->CountNames();
		for( int i = 0; bOk && i < nNames; i++ )
		{
			String cName = pcMsg->GetName( i );
			int nType, nCount;
			if( pcMsg->GetNameInfo( cName.c_str(), &nType, &nCount ) < 0 )
				continue;
			if( nType == T_POINTER )
				continue;	// a pointer means nothing in another process

			// Fixed size (AddData()'s bFixedSize) unless the type's values
			// vary, or this name's do
			bool bFixed = !IsVariableSize( nType );
			size_t nFirstSize = 0;
			for( int j = 0; bFixed && j < nCount; j++ )
			{
				const void* pData;
				size_t nSize;
				if( pcMsg->FindData( cName.c_str(), nType, &pData, &nSize, j ) < 0 )
					break;
				if( j == 0 )
					nFirstSize = nSize;
				else if( nSize != nFirstSize )
					bFixed = false;
			}

			for( int j = 0; bOk && j < nCount; j++ )
			{
				const void* pData;
				size_t nSize;
				if( pcMsg->FindData( cName.c_str(), nType, &pData, &nSize, j ) < 0 )
					break;
				cInsert.BindText( 1, m_cFile );
				cInsert.BindText( 2, cName );
				sqlite3_bind_int( cInsert.Get(), 3, nType );
				sqlite3_bind_int( cInsert.Get(), 4, j );
				sqlite3_bind_int( cInsert.Get(), 5, i );
				sqlite3_bind_int( cInsert.Get(), 6, bFixed ? 1 : 0 );
				BindValue( cInsert.Get(), 7, nType, pData, nSize );
				bOk = cInsert.Run();
			}
		}
	}	// the statements are finalized before COMMIT

	if( bOk && cDB.Exec( "COMMIT" ) )
		return 0;
	cDB.Exec( "ROLLBACK" );
	return -1;
}

/* Reads the os::Settings file of the same name, if there is one */
status_t SettingsCache::Private::LoadOldFile( Message* pcMsg )
{
	try
	{
		File cFile( m_cPath + m_cFile, O_RDONLY );
		return ReadFlattened( &cFile, pcMsg );
	}
	catch( ... )
	{
	}
	return -1;
}

/** Default constructor.
 * \par Description:
 *	As Settings(): the settings are those of the file "Settings" in
 *	~/Settings/<application>/, the database there being settings.db.
 *	Like Settings(), it throws an exception when there is no
 *	os::Application to take the name from.
 * \sa SetPath(), SetFile()
 *****************************************************************************/
SettingsCache::SettingsCache()
{
	m = new Private;
	try
	{
		m->SetPath();
	}
	catch( ... )
	{
		delete m;
		throw;
	}
}

/** Constructor with a stream.
 * \par Description:
 *	The settings are read from and written to pcFile, in the Settings
 *	file format (no database).  The object takes over pcFile and deletes
 *	it, as Settings does.
 *****************************************************************************/
SettingsCache::SettingsCache( SeekableIO * pcFile )
{
	m = new Private;
	m->SetIO( pcFile );
}

/** Copy constructor.
 * \par Description:
 *	A copy with the same values and the same database and file name.  A
 *	stream is not shared: the copy has none, and a default path.
 *****************************************************************************/
SettingsCache::SettingsCache( const SettingsCache & cSource ) : Message( cSource )
{
	m = new Private;
	m->m_cPath = cSource.m->m_cPath;
	m->m_cFile = cSource.m->m_cFile;
}

SettingsCache::~SettingsCache()
{
	delete m;
}

/** Load the settings.
 * \par Description:
 *	Replaces the values in the object with the saved ones: the rows of
 *	this file name in the database, or, when there are none yet, the
 *	os::Settings file of that name.  With a stream, reads the stream.
 * \return 0 when settings were loaded, -1 when there were none or they
 *	could not be read.
 *****************************************************************************/
status_t SettingsCache::Load( void )
{
	if( m->m_bUserStream )
		return m->m_pcIO ? ReadFlattened( m->m_pcIO, this ) : -1;

	AutoLocker cLock( &g_cSQLiteLock );
	status_t nResult = m->LoadDatabase( this );
	if( nResult == 1 )
		nResult = m->LoadOldFile( this );
	return nResult == 0 ? 0 : -1;
}

/** Save the settings.
 * \par Description:
 *	Writes every value of the object to the database (creating the
 *	directory and the database when needed), replacing what was saved
 *	under this file name before, in one transaction.  Pointers are not
 *	saved.  With a stream, writes the stream in the Settings format.
 * \return 0 on success, -1 on failure (the saved settings are unchanged).
 *****************************************************************************/
status_t SettingsCache::Save( void ) const
{
	if( m->m_bUserStream )
		return m->m_pcIO ? WriteFlattened( m->m_pcIO, this ) : -1;

	AutoLocker cLock( &g_cSQLiteLock );
	return m->SaveDatabase( this );
}

/** The directory the database is in (with a trailing /). */
Path SettingsCache::GetPath() const
{
	Path cPath( m->m_cPath );
	return cPath;
}

/** The settings file name (as Settings::GetFile()). */
File SettingsCache::GetFile() const
{
	File cFile( m->m_cFile );
	return cFile;
}

/** Use the database in another directory. */
void SettingsCache::SetPath( Path* pcPath )
{
	if( pcPath )
		m->SetPath( m->m_cFile, pcPath->GetPath() + String( "/" ) );
}

/** Read and write a stream, in the Settings file format.
 * \par Description:
 *	The object takes over pcFile and deletes it.
 *****************************************************************************/
void SettingsCache::SetFile( SeekableIO* pcFile )
{
	if( pcFile )
		m->SetIO( pcFile );
}

/** Use another settings file name within the same database. */
void SettingsCache::SetFile( String cFilename )
{
	m->SetPath( cFilename, m->m_cPath );
}

SettingsCache & SettingsCache::operator=( const SettingsCache & cSource )
{
	if( &cSource == this )
		return *this;
	Message::operator=( cSource );
	// As Settings: the stream (not copied) or the path and file name
	if( cSource.m->m_bUserStream )
	{
		m->m_bUserStream = false;	// the source still owns its stream
		m->m_pcIO = NULL;
		m->m_cPath = cSource.m->m_cPath;
		m->m_cFile = cSource.m->m_cFile;
	}
	else
	{
		m->m_cPath = cSource.m->m_cPath;
		m->m_cFile = cSource.m->m_cFile;
	}
	return *this;
}

SettingsCache & SettingsCache::operator=( const Message & cSource )
{
	Message::operator=( cSource );
	return *this;
}

String SettingsCache::GetString( const char *pzName, const char *pzDefault, int nIndex ) const
{
	String v;
	if( FindString( pzName, &v, nIndex ) < 0 )
		v = pzDefault;
	return v;
}

int8 SettingsCache::GetInt8( const char *pzName, int8 nDefault, int nIndex ) const
{
	int8 v;
	if( FindInt8( pzName, &v, nIndex ) < 0 )
		v = nDefault;
	return v;
}

int16 SettingsCache::GetInt16( const char *pzName, int16 nDefault, int nIndex ) const
{
	int16 v;
	if( FindInt16( pzName, &v, nIndex ) < 0 )
		v = nDefault;
	return v;
}

int32 SettingsCache::GetInt32( const char *pzName, int32 nDefault, int nIndex ) const
{
	int32 v;
	if( FindInt32( pzName, &v, nIndex ) < 0 )
		v = nDefault;
	return v;
}

int64 SettingsCache::GetInt64( const char *pzName, int64 nDefault, int nIndex ) const
{
	int64 v;
	if( FindInt64( pzName, &v, nIndex ) < 0 )
		v = nDefault;
	return v;
}

bool SettingsCache::GetBool( const char *pzName, bool bDefault, int nIndex ) const
{
	bool v;
	if( FindBool( pzName, &v, nIndex ) < 0 )
		v = bDefault;
	return v;
}

float SettingsCache::GetFloat( const char *pzName, float vDefault, int nIndex ) const
{
	float v;
	if( FindFloat( pzName, &v, nIndex ) < 0 )
		v = vDefault;
	return v;
}

double SettingsCache::GetDouble( const char *pzName, double vDefault, int nIndex ) const
{
	double v;
	if( FindDouble( pzName, &v, nIndex ) < 0 )
		v = vDefault;
	return v;
}

Rect SettingsCache::GetRect( const char *pzName, const Rect & cDefault, int nIndex ) const
{
	Rect v;
	if( FindRect( pzName, &v, nIndex ) < 0 )
		v = cDefault;
	return v;
}

IRect SettingsCache::GetIRect( const char *pzName, const IRect & cDefault, int nIndex ) const
{
	IRect v;
	if( FindIRect( pzName, &v, nIndex ) < 0 )
		v = cDefault;
	return v;
}

Point SettingsCache::GetPoint( const char *pzName, const Point & cDefault, int nIndex ) const
{
	Point v;
	if( FindPoint( pzName, &v, nIndex ) < 0 )
		v = cDefault;
	return v;
}

IPoint SettingsCache::GetIPoint( const char *pzName, const IPoint & cDefault, int nIndex ) const
{
	IPoint v;
	if( FindIPoint( pzName, &v, nIndex ) < 0 )
		v = cDefault;
	return v;
}

Color32_s SettingsCache::GetColor32( const char *pzName, const Color32_s & cDefault, int nIndex ) const
{
	Color32_s v;
	if( FindColor32( pzName, &v, nIndex ) < 0 )
		v = cDefault;
	return v;
}

Variant SettingsCache::GetVariant( const char *pzName, const Variant & cDefault, int nIndex ) const
{
	Variant v;
	if( FindVariant( pzName, &v, nIndex ) < 0 )
		v = cDefault;
	return v;
}

/** Set a value, replacing the one at nIndex.
 * \par Description:
 *	As Settings::SetData(): when pzName has a value at nIndex, it is
 *	replaced; otherwise (no such name, or nIndex past the last value, or
 *	nIndex < 0) the value is added after the others.  Unlike Settings, a
 *	value can be replaced by one of another size.
 *****************************************************************************/
status_t SettingsCache::SetData( const char *pzName, int nType, const void *pData, uint32 nSize, int nIndex, bool bFixedSize, int nMaxCountHint )
{
	int nOldType, nCount;

	if( nIndex < 0 || GetNameInfo( pzName, &nOldType, &nCount ) < 0 || nOldType != nType || nIndex >= nCount )
		return AddData( pzName, nType, pData, nSize, bFixedSize, nMaxCountHint );

	// Message has no call to replace a value: take the name's values out and
	// add them back, with the new one in place of the old
	std::vector<std::string> cValues;
	for( int i = 0; i < nCount; i++ )
	{
		const void* pOld;
		size_t nOldSize;
		if( FindData( pzName, nType, &pOld, &nOldSize, i ) < 0 )
			return -1;
		if( i == nIndex )
			cValues.push_back( std::string( ( const char* )pData, nSize ) );
		else
			cValues.push_back( std::string( ( const char* )pOld, nOldSize ) );
	}
	// a fixed-size array holds values of one size only
	bool bFixed = bFixedSize;
	for( int i = 1; i < nCount; i++ )
	{
		if( cValues[i].size() != cValues[0].size() )
			bFixed = false;
	}

	std::string cName( pzName );	// pzName may point into the message
	RemoveName( cName.c_str() );
	status_t nResult = 0;
	for( int i = 0; i < nCount; i++ )
	{
		static const char nEmpty = 0;
		const char* pBytes = cValues[i].empty() ? &nEmpty : cValues[i].data();
		if( AddData( cName.c_str(), nType, pBytes, cValues[i].size(), bFixed, nCount > nMaxCountHint ? nCount : nMaxCountHint ) < 0 )
			nResult = -1;
	}
	return nResult;
}

status_t SettingsCache::SetString( const char *pzName, const String & cValue, int nIndex )
{
	return SetData( pzName, T_STRING, cValue.c_str(), cValue.size() + 1, nIndex, false );
}

status_t SettingsCache::SetInt8( const char *pzName, int8 nValue, int nIndex )
{
	return SetData( pzName, T_INT8, &nValue, sizeof( nValue ), nIndex, true );
}

status_t SettingsCache::SetInt16( const char *pzName, int16 nValue, int nIndex )
{
	return SetData( pzName, T_INT16, &nValue, sizeof( nValue ), nIndex, true );
}

status_t SettingsCache::SetInt32( const char *pzName, int32 nValue, int nIndex )
{
	return SetData( pzName, T_INT32, &nValue, sizeof( nValue ), nIndex, true );
}

status_t SettingsCache::SetInt64( const char *pzName, int64 nValue, int nIndex )
{
	return SetData( pzName, T_INT64, &nValue, sizeof( nValue ), nIndex, true );
}

status_t SettingsCache::SetBool( const char *pzName, bool bValue, int nIndex )
{
	return SetData( pzName, T_BOOL, &bValue, sizeof( bValue ), nIndex, true );
}

status_t SettingsCache::SetFloat( const char *pzName, float vValue, int nIndex )
{
	return SetData( pzName, T_FLOAT, &vValue, sizeof( vValue ), nIndex, true );
}

status_t SettingsCache::SetDouble( const char *pzName, double vValue, int nIndex )
{
	return SetData( pzName, T_DOUBLE, &vValue, sizeof( vValue ), nIndex, true );
}

status_t SettingsCache::SetRect( const char *pzName, const Rect & cValue, int nIndex )
{
	return SetData( pzName, T_RECT, &cValue, sizeof( cValue ), nIndex, true );
}

status_t SettingsCache::SetIRect( const char *pzName, const IRect & cValue, int nIndex )
{
	return SetData( pzName, T_IRECT, &cValue, sizeof( cValue ), nIndex, true );
}

status_t SettingsCache::SetPoint( const char *pzName, const Point & cValue, int nIndex )
{
	return SetData( pzName, T_POINT, &cValue, sizeof( cValue ), nIndex, true );
}

status_t SettingsCache::SetIPoint( const char *pzName, const IPoint & cValue, int nIndex )
{
	return SetData( pzName, T_IPOINT, &cValue, sizeof( cValue ), nIndex, true );
}

status_t SettingsCache::SetColor32( const char *pzName, const Color32_s & cValue, int nIndex )
{
	return SetData( pzName, T_COLOR32, &cValue, sizeof( cValue ), nIndex, true );
}

status_t SettingsCache::SetMessage( const char *pzName, const Message & cValue, int nIndex )
{
	std::vector<uint8> cBuffer( cValue.GetFlattenedSize() );
	cValue.Flatten( &cBuffer[0], cBuffer.size() );
	return SetData( pzName, T_MESSAGE, &cBuffer[0], cBuffer.size(), nIndex, false );
}

status_t SettingsCache::SetVariant( const char *pzName, const Variant & cValue, int nIndex )
{
	std::vector<uint8> cBuffer( cValue.GetFlattenedSize() );
	if( cBuffer.empty() )
		cBuffer.push_back( 0 );
	cValue.Flatten( &cBuffer[0], cValue.GetFlattenedSize() );
	return SetData( pzName, T_VARIANT, &cBuffer[0], cValue.GetFlattenedSize(), nIndex, false );
}
