/* Host shim: the one Syllable thread call libsyllable's Locker needs. */
#ifndef __F_ATHEOS_THREADS_H__
#define __F_ATHEOS_THREADS_H__

#include <atheos/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/* NULL name: the calling thread (Linux gettid). */
thread_id get_thread_id( const char* const pzName );

#ifdef __cplusplus
}
#endif

#endif /* __F_ATHEOS_THREADS_H__ */
