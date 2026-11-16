/*
 *  The GLUT internals that GLUT 3.7's portable files (fonts, shapes,
 *  teapot) use.  It replaces the X11 glutint.h of the MesaGLUT sources,
 *  which the build copies over it (see ../CMakeLists.txt).
 */
#ifndef __glutint_h__
#define __glutint_h__

#define GLUT_NO_LIB_PRAGMA
#include <GL/glut.h>

#ifdef __cplusplus
extern "C" {
#endif

extern void __glutFatalError( char* format, ... );
extern void __glutWarning( char* format, ... );
extern void* __glutFont( void* font );

#ifdef __cplusplus
}
#endif

#endif
