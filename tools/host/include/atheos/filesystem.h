/* Host shim: the Syllable filesystem calls libsyllable's storage classes use,
 * on top of Linux.  The real header pulls in Syllable's posix/ headers. */
#ifndef __F_ATHEOS_FILESYSTEM_H__
#define __F_ATHEOS_FILESYSTEM_H__

#include <atheos/types.h>
#include <atheos/fs_attribs.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>

/* Syllable opens the symlink itself.  Linux can't do that for reading or
 * writing, so this follows the link (attributes land on the target). */
#define O_NOTRAVERSE 0

/* fcntl( nFD, F_COPYFD, -1 ): duplicate to any free descriptor. */
#define F_COPYFD     0x5359

#ifdef __cplusplus
extern "C" {
#endif

enum { IMGFILE_BIN_DIR = -2 };
/* No executable images on the host: fails with ENOSYS. */
int open_image_file( int nImageID );

status_t get_directory_path( int nFD, char* pzBuffer, int nBufLen );
int based_open( int nRootFD, const char *pzPath, int nFlags, ... );
ssize_t read_pos( int nFile, off_t nPos, void* pBuffer, size_t nLength );
ssize_t write_pos( int nFile, off_t nPos, const void* pBuffer, size_t nLength );

int syl_fcntl( int nFD, int nCmd, ... );

#ifdef __cplusplus
}
#endif

#define fcntl syl_fcntl

#endif /* __F_ATHEOS_FILESYSTEM_H__ */
