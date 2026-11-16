/* Host shim: Syllable semaphores, implemented on pthreads in shim.cpp. */
#ifndef __F_ATHEOS_SEMAPHORE_H__
#define __F_ATHEOS_SEMAPHORE_H__

#include <atheos/types.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SEM_RECURSIVE       0x0001
#define SEM_WARN_DBL_LOCK   0x0002
#define SEM_WARN_DBL_UNLOCK 0x0004
#define SEM_GLOBAL          0x0008

sem_id   create_semaphore( const char* pzName, int nCount, uint32 nFlags );
status_t delete_semaphore( sem_id hSema );
status_t lock_semaphore( sem_id hSema );
status_t lock_semaphore_x( sem_id hSema, int nCount, uint32 nFlags, bigtime_t nTimeOut );
status_t unlock_semaphore( sem_id hSema );
status_t unlock_semaphore_x( sem_id hSema, int nCount, uint32 nFlags );

#ifdef __cplusplus
}
#endif

#endif /* __F_ATHEOS_SEMAPHORE_H__ */
