#ifndef __DEBUG_H__
#define __DEBUG_H__

#include <os/kdebug.h>

#ifdef __DEBUG__
#define FN_START()	dbprintf("CALLED %s\n", __PRETTY_FUNCTION__)
#define FN_END()	dbprintf("FINISHED %s\n", __PRETTY_FUNCTION__)
#define FN_PRINTF	dbprintf
#else
#define FN_START()
#define FN_END()
#define FN_PRINTF
#endif /* __DEBUG__ */

#endif /* __DEBUG_H__ */




