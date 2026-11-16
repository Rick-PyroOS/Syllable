#include "statusbar.h"

using namespace os;

class Progress : public os::View
{
public:
	Progress( os::Rect cFrame, os::String cName ) : os::View( cFrame, cName )
	{
		m_nProgress = 0;
		m_bProgress = false;
		SetEraseColor( os::get_default_color( os::COL_SHINE ) );
	}
	void Paint( const os::Rect& cUpdate )
	{
		if( !m_bProgress )
			EraseRect( cUpdate );
		else
		{
			float vX = m_nProgress - 6;
			
			SetFgColor( os::get_default_color( os::COL_SEL_WND_BORDER ) );
			
			os::Color32_s sDefaultColor = os::get_default_color( os::COL_SEL_WND_BORDER );
			while( vX < GetBounds().Width() + 13 )
			{
				for( int i = -6; i < 7; i++ )
				{
					os::Color32_s sColor;
					sColor.red = sDefaultColor.red + ( 255 - sDefaultColor.red ) * abs( i ) / 6;
					sColor.blue = sDefaultColor.blue + ( 255 - sDefaultColor.blue ) * abs( i ) / 6;
					sColor.green = sDefaultColor.green + ( 255 - sDefaultColor.green ) * abs( i ) / 6;
				
					SetFgColor( sColor );
					FillRect( os::Rect( vX + i, 0, vX + i, GetBounds().Height() ) );
				}
				//EraseRect( os::Rect( vX + 5, 0, vX + 9, GetBounds().Height() ) );
				vX += 13;
			}
		}
	}
	void SetProgress( bool bProgress )
	{
		if( bProgress == false && m_bProgress == false )
			return;
		m_bProgress = bProgress;
		m_nProgress += 1;
		m_nProgress %= 13;
		Invalidate();
		Flush();
	}
	
private:
	int m_nProgress;
	bool m_bProgress;
};

StatusBar::StatusBar( const os::Rect& cFrame, const std::string& cTitle ) : View( cFrame, cTitle )
{
    GetFontHeight( &m_sFontHeight );
    os::Rect cPFrame( GetBounds().Width() - 25, 2, GetBounds().Width() - 2, GetBounds().Height() - 1 );
    m_pcProgress = new Progress( os::Rect(), "progress" );
    AddChild( m_pcProgress );
}


Point StatusBar::GetPreferredSize( bool bLargest ) const
{
    if ( bLargest ) {
	return( Point( COORD_MAX, 12.0f + m_sFontHeight.ascender + m_sFontHeight.descender + m_sFontHeight.line_gap ) );
    } else {
	return( Point( 12.0f, 12.0f + m_sFontHeight.ascender + m_sFontHeight.descender + m_sFontHeight.line_gap ) );
    }
}

void StatusBar::FrameSized( const Point& cDelta )
{
	os::Rect cPFrame( GetBounds().Width() - 50, 7, GetBounds().Width() - 6, GetBounds().Height() - 6 );
	m_pcProgress->SetFrame( cPFrame );
    if ( cDelta.x < 0.0f ) {
	Rect cBounds = GetBounds();
	cBounds.left = cBounds.right - 5.0f;
	Invalidate( cBounds );
    } else if ( cDelta.x > 0.0f ) {
	Rect cBounds = GetBounds();
	cBounds.left = cBounds.right - 5.0f - cDelta.x;
	Invalidate( cBounds );
    }
    if ( cDelta.y < 0.0f ) {
	Rect cBounds = GetBounds();
	cBounds.top = cBounds.bottom - 5.0f;
	Invalidate( cBounds );
    } else if ( cDelta.y > 0.0f ) {
	Rect cBounds = GetBounds();
	cBounds.top = cBounds.bottom - 5.0f - cDelta.y;
	Invalidate( cBounds );
    }
    Flush();
}

void StatusBar::Paint( const Rect& /*cUpdateRect*/ )
{
    Rect cBounds = GetBounds();
    DrawFrame( cBounds, FRAME_RAISED );
    cBounds.Resize( 4.0f, 4.0f, -4.0f, -4.0f );
    DrawFrame( cBounds, FRAME_RECESSED );
    SetFgColor( 0, 0, 0 );
    DrawString( Point( 8.0f, 6.0f + ceil( m_sFontHeight.ascender + m_sFontHeight.line_gap * 0.5f ) ), m_cStatusStr );
}

void StatusBar::SetStatus( const std::string& cText )
{
	if( cText == m_cStatusStr )
		return;
    m_cStatusStr = cText;
    Rect cBounds = GetBounds();
    cBounds.Resize( 6.0f, 6.0f, -6.0f, -6.0f );
    Invalidate( cBounds );
    Flush();
}

void StatusBar::SetProgress( bool bProgress )
{
	m_pcProgress->SetProgress( bProgress );
}






