/*******************************************************************************
/
/	File:		GLView.h
/
/	Copyright 1993-98, Be Incorporated, All Rights Reserved.
/
*******************************************************************************/

#ifndef GLVIEW_H
#define GLVIEW_H

#include <GL/gl.h>

#define GL_RGB			0
#define GL_INDEX		1 
#define GL_SINGLE		0
#define GL_DOUBLE		2
#define GL_DIRECT		0
#define GL_INDIRECT		4
#define GL_ACCUM		8
#define GL_ALPHA		16
#define GL_DEPTH		32
#define GL_OVERLAY		64
#define GL_UNDERLAY		128
#define GL_STENCIL		512

#ifdef __cplusplus


#include <gui/window.h>
#include <gui/view.h>
#include <gui/bitmap.h>
#include <gui/guidefines.h>

class GLView : public View {
public:

					GLView( Rect cRect, char *zName, uint32 nResizingMode, uint32 nMode, uint32 nOptions );
virtual 			~GLView();

		void		MakeCurrent();
		void		ReleaseCurrent();
		void		YieldCurrent();
		void		SwapBuffers();
		void		SwapBuffers( bool bVSync );
virtual	void        ErrorCallback( uint32 nErrorCode ); 	// Mesa's GLenum is uint where Be's ones was ulong!
		
virtual void		Paint( const Rect& cUpdateRect );

virtual void		AttachedToWindow();
virtual void        AllAttached();
virtual void        DetachedFromWindow();
virtual void        AllDetached();
 
virtual void		FrameSized( const Point& cDelta );

	/* The public methods below, for the moment,
	   are just pass-throughs to View */

virtual void        Show()		{ View::Show();	};
virtual void        Hide()		{ View::Hide();	};


/* New public functions */
		void		*getGC()	{ return m_pGc; }
		
private:

virtual void        _ReservedGLView1();
virtual void        _ReservedGLView2(); 
virtual void        _ReservedGLView3(); 
virtual void        _ReservedGLView4(); 
virtual void        _ReservedGLView5(); 
virtual void        _ReservedGLView6(); 
virtual void        _ReservedGLView7(); 
virtual void        _ReservedGLView8(); 

					GLView(const GLView &);
					GLView     &operator=(const GLView &);

		void		*m_pGc;
		color_space	m_nColorSpace;
		uint32		m_nOptions;
};


#endif	// __cplusplus

#endif	// GLVIEW_H

