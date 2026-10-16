/* Host shim: Syllable's basic types on top of Linux's.  The real header pulls
 * in posix/types.h and syllable/pthreadtypes.h, which clash with glibc. */
#ifndef __F_ATHEOS_TYPES_H__
#define __F_ATHEOS_TYPES_H__

#include <stdint.h>
#include <stddef.h>
#include <sys/types.h>
#ifndef __cplusplus
# include <stdbool.h>
#endif
#include <syllable/inttypes.h>

#endif /* __F_ATHEOS_TYPES_H__ */
