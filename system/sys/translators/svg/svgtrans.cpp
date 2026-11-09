#include <string.h>
#include <vector>

#include <atheos/kdebug.h>
#include <gui/bitmap.h>
#include <util/circularbuffer.h>
#include <translation/translator.h>

extern "C"
{
	#include <librsvg/rsvg.h>
	#include <librsvg/rsvg-cairo.h>
}

using namespace os;

class SVGTrans : public os::Translator
{
public:
		SVGTrans();
		virtual ~SVGTrans();

		virtual void     SetConfig( const Message& cConfig ) {}
		virtual status_t AddData( const void* pData, size_t nLen, bool bFinal );
		virtual ssize_t  AvailableDataSize();
		virtual ssize_t  Read( void* pData, size_t nLen );
		virtual void     Abort() {}
		virtual void     Reset();

private:
	status_t Render();

	std::vector<uint8> m_cInBuffer;		// the whole document: librsvg parses it at once
	CircularBuffer m_cOutBuffer;
	bool m_bDone;
};


SVGTrans::SVGTrans() : os::Translator()
{
	m_bDone = false;
}

SVGTrans::~SVGTrans()
{
}

status_t SVGTrans::AddData(const void* pData,size_t nLen,bool bFinal)
{
	m_cInBuffer.insert( m_cInBuffer.end(), (const uint8*)pData, (const uint8*)pData + nLen );

	if (bFinal)
	{
		return( Render() );
	}
	return( 0 );
}

// The same as Jonas' svgviewer: librsvg renders the document with cairo
// straight into the bitmap's pixels, which then go out as the translator's
// bitmap stream (one CS_RGB32 frame)
status_t SVGTrans::Render()
{
	GError* pError = NULL;
	RsvgHandle* psHandle = rsvg_handle_new_from_data( (guint8*)&m_cInBuffer[0], m_cInBuffer.size(), &pError );

	if (psHandle == NULL)
	{
		dbprintf( "svgtrans: %s\n", pError ? pError->message : "unable to load the SVG" );
		if( pError )
			g_error_free( pError );
		return( ERR_INVALID_DATA );
	}

	// Get dimension of the SVG
	RsvgDimensionData sDimension;
	rsvg_handle_get_dimensions( psHandle, &sDimension );
	if( sDimension.width <= 0 || sDimension.height <= 0 )
	{
		g_object_unref( psHandle );
		return( ERR_INVALID_DATA );
	}
	int nWidth = sDimension.width;
	int nHeight = sDimension.height;
	int nBytesPerRow = nWidth * 4;

	// Pixels to hold the SVG image
	std::vector<uint8> cPixels( nBytesPerRow * nHeight, 0 );

	// Create the surface
	cairo_surface_t* psSurface = cairo_image_surface_create_for_data(
				                       	&cPixels[0],
                        				CAIRO_FORMAT_ARGB32,
                        				nWidth, nHeight,
                        				nBytesPerRow );
	cairo_t* psCairo = cairo_create( psSurface );

	// Here the actual rendering takes place
	rsvg_handle_render_cairo( psHandle, psCairo );

	// Do some clean up
	cairo_destroy( psCairo );
	cairo_surface_destroy( psSurface );
	g_object_unref( psHandle );

	// cairo stores colour premultiplied by alpha; CS_RGB32 expects it
	// straight, or semi-transparent edges come out too dark.  (In memory
	// both are B, G, R, A on x86.)
	for( size_t i = 0; i < cPixels.size(); i += 4 )
	{
		uint32 nAlpha = cPixels[i + 3];
		if( nAlpha != 0 && nAlpha != 255 )
		{
			for( int c = 0; c < 3; c++ )
				cPixels[i + c] = ( cPixels[i + c] * 255 + nAlpha / 2 ) / nAlpha;
		}
	}

	BitmapHeader sHeader;
	sHeader.bh_header_size = sizeof( sHeader );
	sHeader.bh_data_size = 0;
	sHeader.bh_bounds = IRect( 0, 0, nWidth - 1, nHeight - 1 );
	sHeader.bh_frame_count = 1;
	sHeader.bh_bytes_per_row = nBytesPerRow;
	sHeader.bh_color_space = CS_RGB32;
	m_cOutBuffer.Write( &sHeader, sizeof( sHeader ) );

	BitmapFrameHeader sFrame;
	sFrame.bf_header_size = sizeof( sFrame );
	sFrame.bf_time_stamp = 0;
	sFrame.bf_color_space = CS_RGB32;
	sFrame.bf_frame = sHeader.bh_bounds;
	sFrame.bf_bytes_per_row = nBytesPerRow;
	sFrame.bf_data_size = nBytesPerRow * nHeight;
	m_cOutBuffer.Write( &sFrame, sizeof( sFrame ) );

	m_cOutBuffer.Write( &cPixels[0], cPixels.size() );
	m_bDone = true;
	return( 0 );
}

ssize_t SVGTrans::AvailableDataSize()
{
	return( m_cOutBuffer.Size() );
}

ssize_t SVGTrans::Read( void* pData, size_t nLen )
{
	ssize_t nCurLen = m_cOutBuffer.Read( pData, nLen );
	if( nCurLen == 0 && !m_bDone )
		return( ERR_SUSPENDED );
	return( nCurLen );
}

void SVGTrans::Reset()
{
	m_cInBuffer.clear();
	m_cOutBuffer.Clear();
	m_bDone = false;
}

class SVGTransNode : public TranslatorNode
{
public:
	SVGTransNode()
	{
		// Init the librsvg libary
		rsvg_init();
	}

	// Only the start of the file is here, too little for librsvg to parse:
	// an SVG document has "<svg" near its start
	virtual int Identify( const String& cSrcType, const String& cDstType, const void* pData, int nLen )
	{
		if( nLen < 4 )
			return( TranslatorFactory::ERR_NOT_ENOUGH_DATA );

		const char* pzData = (const char*)pData;
		for( int i = 0; i + 4 <= nLen; i++ )
		{
			if( memcmp( pzData + i, "<svg", 4 ) == 0 )
				return( 0 );
		}
		return( TranslatorFactory::ERR_UNKNOWN_FORMAT );
	}

	// A simple svg translator, April 8, 2009, Rick Caudill
	virtual TranslatorInfo GetTranslatorInfo()
	{
		static TranslatorInfo sInfo = { "image/svg+xml", TRANSLATOR_TYPE_IMAGE, 1.0f };
		return( sInfo );
	}

	virtual Translator* CreateTranslator()
	{
		return( new SVGTrans );
	}
};

//////////////////////////////////////////////////////////////////////////////////////////
//
//  C BINDINGS
//
//////////////////////////////////////////////////////////////////////////////////////////

extern "C" int get_translator_count()
{
    return( 1 );
}

extern "C" TranslatorNode* get_translator_node( int nIndex )
{
    static SVGTransNode sNode;
    return( &sNode );
}
