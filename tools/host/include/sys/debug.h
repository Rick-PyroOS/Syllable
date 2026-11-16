/* Host shim: Syllable's kernel debug log.  Printed only with SYL_DEBUG set. */
#ifndef __F_SYS_DEBUG_H__
#define __F_SYS_DEBUG_H__

#ifdef __cplusplus
extern "C" {
#endif

int dbprintf( const char* pzFmt, ... ) __attribute__((format(printf, 1, 2)));

#ifdef __cplusplus
}
#endif

#endif /* __F_SYS_DEBUG_H__ */
