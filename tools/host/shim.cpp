/*
 * The Syllable system calls the host tools' libsyllable sources use,
 * implemented on Linux.
 *
 *   semaphores        pthread mutex + condition variable counting semaphores
 *   attributes        Linux user.* extended attributes: attribute NAME is
 *                     stored as user.NAME, and its Syllable type (when not
 *                     ATTR_TYPE_NONE) as a 4-byte int in user.syllable.type.NAME,
 *                     which attribute listings hide.
 *   images            Syllable executables find their resources through their
 *                     image ID; there are none here, so those calls fail with
 *                     ENOSYS.
 */

#include <atheos/types.h>
#include <atheos/semaphore.h>
#include <atheos/threads.h>
#include <atheos/fs_attribs.h>
#include <atheos/filesystem.h>
#include <atheos/image.h>
#include <sys/debug.h>
#include <util/application.h>

#undef fcntl

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <limits.h>
#include <pthread.h>
#include <sys/syscall.h>
#include <sys/xattr.h>

#include <string>
#include <vector>

/* ---- debug -------------------------------------------------------------- */

extern "C" int dbprintf( const char* pzFmt, ... )
{
	if( getenv( "SYL_DEBUG" ) == NULL )
		return( 0 );
	va_list ap;
	va_start( ap, pzFmt );
	int n = vfprintf( stderr, pzFmt, ap );
	va_end( ap );
	return( n );
}

/* ---- threads and semaphores --------------------------------------------- */

extern "C" thread_id get_thread_id( const char* const pzName )
{
	if( pzName != NULL ) {
		errno = ENOSYS;
		return( -1 );
	}
	return( (thread_id)syscall( SYS_gettid ) );
}

namespace {

struct Semaphore
{
	bool            bUsed;
	int             nCount;
	pthread_cond_t  sCond;
};

enum { MAX_SEMAPHORES = 4096 };

pthread_mutex_t g_sSemLock = PTHREAD_MUTEX_INITIALIZER;
Semaphore       g_asSems[MAX_SEMAPHORES];

Semaphore* find_sem( sem_id hSema )
{
	if( hSema < 0 || hSema >= MAX_SEMAPHORES || !g_asSems[hSema].bUsed ) {
		errno = EINVAL;
		return( NULL );
	}
	return( &g_asSems[hSema] );
}

}

extern "C" sem_id create_semaphore( const char* pzName, int nCount, uint32 nFlags )
{
	pthread_mutex_lock( &g_sSemLock );
	for( int i = 0; i < MAX_SEMAPHORES; i++ ) {
		if( !g_asSems[i].bUsed ) {
			g_asSems[i].bUsed = true;
			g_asSems[i].nCount = nCount;
			pthread_cond_init( &g_asSems[i].sCond, NULL );
			pthread_mutex_unlock( &g_sSemLock );
			return( i );
		}
	}
	pthread_mutex_unlock( &g_sSemLock );
	errno = ENOMEM;
	return( -1 );
}

extern "C" status_t delete_semaphore( sem_id hSema )
{
	pthread_mutex_lock( &g_sSemLock );
	Semaphore* psSem = find_sem( hSema );
	if( psSem != NULL ) {
		psSem->bUsed = false;
		pthread_cond_broadcast( &psSem->sCond );
		pthread_cond_destroy( &psSem->sCond );
	}
	pthread_mutex_unlock( &g_sSemLock );
	return( psSem != NULL ? 0 : -1 );
}

extern "C" status_t lock_semaphore_x( sem_id hSema, int nCount, uint32 nFlags, bigtime_t nTimeOut )
{
	struct timespec sDeadline;
	if( nTimeOut != INFINITE_TIMEOUT ) {
		clock_gettime( CLOCK_REALTIME, &sDeadline );
		sDeadline.tv_sec += nTimeOut / 1000000;
		sDeadline.tv_nsec += ( nTimeOut % 1000000 ) * 1000;
		if( sDeadline.tv_nsec >= 1000000000 ) {
			sDeadline.tv_sec++;
			sDeadline.tv_nsec -= 1000000000;
		}
	}
	pthread_mutex_lock( &g_sSemLock );
	Semaphore* psSem = find_sem( hSema );
	while( psSem != NULL && psSem->nCount < nCount ) {
		int nError = ( nTimeOut == INFINITE_TIMEOUT )
			? pthread_cond_wait( &psSem->sCond, &g_sSemLock )
			: pthread_cond_timedwait( &psSem->sCond, &g_sSemLock, &sDeadline );
		if( nError == ETIMEDOUT ) {
			pthread_mutex_unlock( &g_sSemLock );
			errno = ETIME;
			return( -1 );
		}
		psSem = find_sem( hSema );
	}
	if( psSem != NULL )
		psSem->nCount -= nCount;
	pthread_mutex_unlock( &g_sSemLock );
	return( psSem != NULL ? 0 : -1 );
}

extern "C" status_t lock_semaphore( sem_id hSema )
{
	return( lock_semaphore_x( hSema, 1, 0, INFINITE_TIMEOUT ) );
}

extern "C" status_t unlock_semaphore_x( sem_id hSema, int nCount, uint32 nFlags )
{
	pthread_mutex_lock( &g_sSemLock );
	Semaphore* psSem = find_sem( hSema );
	if( psSem != NULL ) {
		psSem->nCount += nCount;
		pthread_cond_broadcast( &psSem->sCond );
	}
	pthread_mutex_unlock( &g_sSemLock );
	return( psSem != NULL ? 0 : -1 );
}

extern "C" status_t unlock_semaphore( sem_id hSema )
{
	return( unlock_semaphore_x( hSema, 1, 0 ) );
}

/* ---- files -------------------------------------------------------------- */

extern "C" int open_image_file( int nImageID )
{
	errno = ENOSYS;
	return( -1 );
}

extern "C" int get_image_id( void )
{
	errno = ENOSYS;
	return( -1 );
}

extern "C" int get_image_info( int nImage, int nSubImage, image_info* psInfo )
{
	errno = ENOSYS;
	return( -1 );
}

extern "C" status_t get_directory_path( int nFD, char* pzBuffer, int nBufLen )
{
	char zLink[64];
	snprintf( zLink, sizeof( zLink ), "/proc/self/fd/%d", nFD );
	ssize_t nLen = readlink( zLink, pzBuffer, nBufLen - 1 );
	if( nLen < 0 )
		return( -1 );
	pzBuffer[nLen] = '\0';
	return( 0 );
}

extern "C" int based_open( int nRootFD, const char* pzPath, int nFlags, ... )
{
	mode_t nMode = 0;
	if( nFlags & O_CREAT ) {
		va_list ap;
		va_start( ap, nFlags );
		nMode = va_arg( ap, int );
		va_end( ap );
	}
	return( openat( nRootFD, pzPath, nFlags, nMode ) );
}

extern "C" ssize_t read_pos( int nFile, off_t nPos, void* pBuffer, size_t nLength )
{
	return( pread( nFile, pBuffer, nLength, nPos ) );
}

extern "C" ssize_t write_pos( int nFile, off_t nPos, const void* pBuffer, size_t nLength )
{
	return( pwrite( nFile, pBuffer, nLength, nPos ) );
}

extern "C" int syl_fcntl( int nFD, int nCmd, ... )
{
	va_list ap;
	va_start( ap, nCmd );
	long nArg = va_arg( ap, long );
	va_end( ap );
	if( nCmd == F_COPYFD )
		return( fcntl( nFD, F_DUPFD, 0 ) );
	return( fcntl( nFD, nCmd, nArg ) );
}

/* ---- attributes --------------------------------------------------------- */

namespace {

const char* const TYPE_PREFIX = "user.syllable.type.";

std::string data_name( const char* pzName )
{
	return( std::string( "user." ) + pzName );
}

std::string type_name( const char* pzName )
{
	return( std::string( TYPE_PREFIX ) + pzName );
}

/* The whole value of an attribute, or false with errno set. */
bool get_value( int nFile, const std::string& cName, std::string* pcValue )
{
	for( ;; ) {
		ssize_t nSize = fgetxattr( nFile, cName.c_str(), NULL, 0 );
		if( nSize < 0 )
			return( false );
		pcValue->resize( nSize );
		nSize = fgetxattr( nFile, cName.c_str(), &(*pcValue)[0], nSize );
		if( nSize >= 0 ) {
			pcValue->resize( nSize );
			return( true );
		}
		if( errno != ERANGE )	/* grew in between: try again */
			return( false );
	}
}

int get_type( int nFile, const char* pzName )
{
	int nType;
	if( fgetxattr( nFile, type_name( pzName ).c_str(), &nType, sizeof( nType ) ) != sizeof( nType ) )
		return( ATTR_TYPE_NONE );
	return( nType );
}

struct AttrDir
{
	std::vector<std::string> cNames;
	size_t                   nPos;
	struct dirent            sEntry;
};

}

extern "C" ssize_t write_attr( int nFile, const char* pzAttrName, int nFlags, int nType,
			       const void* pBuffer, off_t nPos, size_t nLen )
{
	if( nPos < 0 ) {
		errno = EINVAL;
		return( -1 );
	}
	std::string cName = data_name( pzAttrName );
	std::string cValue;
	if( !( nFlags & O_TRUNC ) && !get_value( nFile, cName, &cValue ) && errno != ENODATA )
		return( -1 );
	if( cValue.size() < size_t( nPos ) + nLen )
		cValue.resize( nPos + nLen );
	memcpy( &cValue[nPos], pBuffer, nLen );
	if( fsetxattr( nFile, cName.c_str(), cValue.data(), cValue.size(), 0 ) < 0 )
		return( -1 );
	if( nType != ATTR_TYPE_NONE ) {
		if( fsetxattr( nFile, type_name( pzAttrName ).c_str(), &nType, sizeof( nType ), 0 ) < 0 )
			return( -1 );
	} else if( fremovexattr( nFile, type_name( pzAttrName ).c_str() ) < 0 && errno != ENODATA ) {
		return( -1 );
	}
	return( nLen );
}

extern "C" ssize_t read_attr( int nFile, const char* pzAttrName, int nType,
			      void* pBuffer, off_t nPos, size_t nLen )
{
	std::string cValue;
	if( !get_value( nFile, data_name( pzAttrName ), &cValue ) ) {
		if( errno == ENODATA )
			errno = ENOENT;
		return( -1 );
	}
	if( nPos < 0 ) {
		errno = EINVAL;
		return( -1 );
	}
	if( size_t( nPos ) >= cValue.size() )
		return( 0 );
	if( nLen > cValue.size() - nPos )
		nLen = cValue.size() - nPos;
	memcpy( pBuffer, cValue.data() + nPos, nLen );
	return( nLen );
}

extern "C" int remove_attr( int nFile, const char* pzName )
{
	if( fremovexattr( nFile, data_name( pzName ).c_str() ) < 0 ) {
		if( errno == ENODATA )
			errno = ENOENT;
		return( -1 );
	}
	fremovexattr( nFile, type_name( pzName ).c_str() );
	return( 0 );
}

extern "C" int stat_attr( int nFile, const char* pzName, struct attr_info* psBuffer )
{
	ssize_t nSize = fgetxattr( nFile, data_name( pzName ).c_str(), NULL, 0 );
	if( nSize < 0 ) {
		if( errno == ENODATA )
			errno = ENOENT;
		return( -1 );
	}
	psBuffer->ai_size = nSize;
	psBuffer->ai_type = get_type( nFile, pzName );
	return( 0 );
}

extern "C" DIR* open_attrdir( int nFile )
{
	std::string cList;
	for( ;; ) {
		ssize_t nSize = flistxattr( nFile, NULL, 0 );
		if( nSize < 0 )
			return( NULL );
		cList.resize( nSize );
		nSize = flistxattr( nFile, &cList[0], nSize );
		if( nSize >= 0 ) {
			cList.resize( nSize );
			break;
		}
		if( errno != ERANGE )
			return( NULL );
	}
	AttrDir* psDir = new AttrDir;
	psDir->nPos = 0;
	for( size_t i = 0; i < cList.size(); i += strlen( &cList[i] ) + 1 ) {
		const char* pzName = &cList[i];
		if( strncmp( pzName, "user.", 5 ) == 0 &&
		    strncmp( pzName, TYPE_PREFIX, strlen( TYPE_PREFIX ) ) != 0 )
			psDir->cNames.push_back( pzName + 5 );
	}
	return( reinterpret_cast<DIR*>( psDir ) );
}

extern "C" struct dirent* read_attrdir( DIR* pDir )
{
	AttrDir* psDir = reinterpret_cast<AttrDir*>( pDir );
	if( psDir->nPos >= psDir->cNames.size() )
		return( NULL );
	const std::string& cName = psDir->cNames[psDir->nPos++];
	memset( &psDir->sEntry, 0, sizeof( psDir->sEntry ) );
	strncpy( psDir->sEntry.d_name, cName.c_str(), sizeof( psDir->sEntry.d_name ) - 1 );
	return( &psDir->sEntry );
}

extern "C" void rewind_attrdir( DIR* pDir )
{
	reinterpret_cast<AttrDir*>( pDir )->nPos = 0;
}

extern "C" int close_attrdir( DIR* pDir )
{
	delete reinterpret_cast<AttrDir*>( pDir );
	return( 0 );
}

/* ---- libsyllable classes the tools link against but never use ----------- */

namespace os {

Application* Application::GetInstance()
{
	static Application s_cApp;
	return( &s_cApp );
}

const Catalog* Application::GetCatalog() const
{
	return( NULL );
}

}
