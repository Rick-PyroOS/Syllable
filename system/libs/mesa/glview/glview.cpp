/*
 *  os::GLView on Mesa's OSMesa interface (see GLView.h).
 *
 *  The back buffer is an os::Bitmap (CS_RGB32, shared with the appserver):
 *  OSMesa renders straight into its raster as BGRA, top row first, and
 *  SwapBuffers() draws the bitmap in the view.  Two locks:
 *
 *    m_cGLLock      LockGL()/UnlockGL(): one renderer at a time
 *    m_cBufferLock  the bitmap pointer: taken to swap in a resized bitmap,
 *                   and to draw it (Paint(), SwapBuffers()).  It is always
 *                   taken after the window's lock, never before.
 */

#include <string.h>
#include <stdio.h>

#include <gui/GLView.h>
#include <gui/window.h>
#include <gui/bitmap.h>
#include <util/locker.h>

#include <GL/osmesa.h>

using namespace os;

GLParams::GLParams()
	: flags( 0 ), colour_format( SYLGL_COLOUR_RGB888 ),
	  depth_bits( 0 ), stencil_bits( 0 ), accum_bits( 0 )
{
}

class GLView::Private
{
public:
	Private( const GLParams& sParams )
		: m_sParams( sParams ), m_hContext( NULL ), m_pcBitmap( NULL ),
		  m_nWidth( 0 ), m_nHeight( 0 ),
		  m_cGLLock( "glview_gl" ), m_cBufferLock( "glview_buffer" )
	{
	}

	GLParams m_sParams;
	OSMesaContext m_hContext;
	Bitmap* m_pcBitmap;
	int m_nWidth;
	int m_nHeight;
	Locker m_cGLLock;
	Locker m_cBufferLock;
};

GLView::GLView( const Rect& cFrame, const String& cTitle, const GLParams& sParams,
		uint32 nResizeMask, uint32 nFlags )
	: View( cFrame, cTitle, nResizeMask, nFlags )
{
	m = new Private( sParams );
	m->m_hContext = OSMesaCreateContextExt( OSMESA_BGRA, sParams.depth_bits,
		sParams.stencil_bits, sParams.accum_bits, NULL );
	if( m->m_hContext == NULL )
		fprintf( stderr, "GLView: OSMesaCreateContextExt() failed\n" );
}

GLView::~GLView()
{
	if( m->m_hContext != NULL )
	{
		if( OSMesaGetCurrentContext() == m->m_hContext )
			OSMesaMakeCurrent( NULL, NULL, GL_UNSIGNED_BYTE, 0, 0 );
		OSMesaDestroyContext( m->m_hContext );
	}
	delete m->m_pcBitmap;
	delete m;
}

const GLParams& GLView::GetParams() const
{
	return m->m_sParams;
}

bool GLView::IsDoubleBuffered() const
{
	return ( m->m_sParams.flags & SYLGL_DOUBLE_BUFFER ) != 0;
}

void GLView::MakeCurrent()
{
	if( m->m_hContext == NULL )
		return;

	Rect cBounds = GetBounds();
	int nWidth = int( cBounds.Width() ) + 1;
	int nHeight = int( cBounds.Height() ) + 1;
	if( nWidth < 1 )
		nWidth = 1;
	if( nHeight < 1 )
		nHeight = 1;

	if( m->m_pcBitmap == NULL || nWidth != m->m_nWidth || nHeight != m->m_nHeight )
	{
		Bitmap* pcNew = new Bitmap( nWidth, nHeight, CS_RGB32,
			Bitmap::SHARE_FRAMEBUFFER | Bitmap::NO_ALPHA_CHANNEL );
		uint8* pRaster = pcNew->LockRaster();
		if( pRaster != NULL )
			memset( pRaster, 0, pcNew->GetBytesPerRow() * nHeight );
		pcNew->UnlockRaster();

		m->m_cBufferLock.Lock();
		Bitmap* pcOld = m->m_pcBitmap;
		m->m_pcBitmap = pcNew;
		m->m_nWidth = nWidth;
		m->m_nHeight = nHeight;
		m->m_cBufferLock.Unlock();
		delete pcOld;
	}

	uint8* pRaster = m->m_pcBitmap->LockRaster();
	if( pRaster == NULL )
	{
		fprintf( stderr, "GLView: the bitmap has no raster\n" );
		return;
	}
	if( !OSMesaMakeCurrent( m->m_hContext, pRaster, GL_UNSIGNED_BYTE, nWidth, nHeight ) )
	{
		fprintf( stderr, "GLView: OSMesaMakeCurrent() failed\n" );
		return;
	}
	// rows are the bitmap's (padded) rows, the top one first
	OSMesaPixelStore( OSMESA_ROW_LENGTH, m->m_pcBitmap->GetBytesPerRow() / 4 );
	OSMesaPixelStore( OSMESA_Y_UP, 0 );
}

void GLView::LockGL()
{
	m->m_cGLLock.Lock();
	MakeCurrent();
}

void GLView::UnlockGL()
{
	m->m_cGLLock.Unlock();
}

void GLView::SwapBuffers()
{
	// Mesa keeps primitives back until a flush: finish them first
	if( m->m_hContext != NULL && OSMesaGetCurrentContext() == m->m_hContext )
		glFinish();

	Window* pcWindow = GetWindow();
	if( pcWindow == NULL )
		return;
	if( pcWindow->Lock() != 0 )
		return;
	m->m_cBufferLock.Lock();
	if( m->m_pcBitmap != NULL )
	{
		Rect cRect( 0, 0, m->m_nWidth - 1, m->m_nHeight - 1 );
		DrawBitmap( m->m_pcBitmap, cRect, cRect );
	}
	m->m_cBufferLock.Unlock();
	// Sync, not Flush: the appserver has drawn the bitmap when it returns,
	// so the next frame can render into it (and a resize can delete it)
	Sync();
	pcWindow->Unlock();
}

void GLView::CopySubBuffer( GLint x, GLint y, GLuint width, GLuint height ) const
{
	if( m->m_hContext != NULL && OSMesaGetCurrentContext() == m->m_hContext )
		glFinish();

	Window* pcWindow = GetWindow();
	if( pcWindow == NULL || pcWindow->Lock() != 0 )
		return;
	m->m_cBufferLock.Lock();
	if( m->m_pcBitmap != NULL )
	{
		// GL's y goes up from the bottom; the bitmap's rows go down
		float vTop = float( m->m_nHeight - y - int( height ) );
		Rect cRect( x, vTop, x + int( width ) - 1, vTop + int( height ) - 1 );
		cRect &= Rect( 0, 0, m->m_nWidth - 1, m->m_nHeight - 1 );
		if( cRect.IsValid() )
			const_cast<GLView*>( this )->DrawBitmap( m->m_pcBitmap, cRect, cRect );
	}
	m->m_cBufferLock.Unlock();
	const_cast<GLView*>( this )->Sync();
	pcWindow->Unlock();
}

void GLView::Paint( const Rect& cUpdateRect )
{
	m->m_cBufferLock.Lock();
	if( m->m_pcBitmap != NULL )
	{
		Rect cRect = cUpdateRect & Rect( 0, 0, m->m_nWidth - 1, m->m_nHeight - 1 );
		if( cRect.IsValid() )
			DrawBitmap( m->m_pcBitmap, cRect, cRect );
	}
	else
	{
		SetFgColor( 0, 0, 0 );
		FillRect( cUpdateRect );
	}
	m->m_cBufferLock.Unlock();
}
