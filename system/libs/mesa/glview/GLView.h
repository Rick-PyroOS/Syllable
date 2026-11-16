/*
 *  os::GLView - an OpenGL view for Syllable (Pyro)
 *
 *  The view renders with Mesa's software renderer through its OSMesa
 *  interface, into an os::Bitmap that the view draws on screen.  The API is
 *  the one the Mesa 6.0.1 Syllable driver had (Builder's mesa-6.0.1 port),
 *  so programs written for it build unchanged.
 *
 *  Usage:
 *
 *      GLParams cParams;
 *      cParams.flags |= SYLGL_DOUBLE_BUFFER;
 *      cParams.depth_bits = 16;
 *      GLView* pcView = new GLView( cFrame, "gl", cParams );
 *      ...
 *      pcView->LockGL();       // makes the view's context current
 *      glClear( ... ); ...     // render
 *      pcView->UnlockGL();
 *      pcView->SwapBuffers();  // shows what was rendered
 *
 *  Rendering can be done from any thread, but only one view is current at a
 *  time in a process (Mesa is built without thread-local contexts): hold
 *  LockGL() while rendering.  The view's buffer follows the view's size;
 *  LockGL() resizes it.  Paint() draws the last rendered frame.
 */

#ifndef __F_GUI_GLVIEW_H__
#define __F_GUI_GLVIEW_H__

#include <gui/view.h>
#include <GL/gl.h>

namespace os {

enum gl_colour_format
{
	SYLGL_COLOUR_RGB888,
	SYLGL_COLOUR_MONO8	// accepted, rendered as RGB
};

const uint32 SYLGL_DOUBLE_BUFFER = 0x0001;

struct GLParams
{
	uint32 flags;

	gl_colour_format colour_format;
	uint32 depth_bits;
	uint32 stencil_bits;
	uint32 accum_bits;

	GLParams();
};

class GLView : public View
{
public:
	GLView( const Rect& cFrame, const String& cTitle, const GLParams& sParams,
		uint32 nResizeMask = CF_FOLLOW_LEFT | CF_FOLLOW_TOP,
		uint32 nFlags = WID_WILL_DRAW | WID_CLEAR_BACKGROUND );
	virtual ~GLView();

	void LockGL();		// lock, and make this view's context current
	void UnlockGL();
	void SwapBuffers();	// draw the rendered frame in the view
	void CopySubBuffer( GLint x, GLint y, GLuint width, GLuint height ) const;

	// Make the context current without taking the lock (GLUT: one thread
	// renders).  Resizes the buffer to the view's size.
	void MakeCurrent();
	bool IsDoubleBuffered() const;
	const GLParams& GetParams() const;

	virtual void Paint( const Rect& cUpdateRect );

private:
	GLView( const GLView& );
	GLView& operator=( const GLView& );

	class Private;
	Private* m;
};

} // namespace os

#endif // __F_GUI_GLVIEW_H__
