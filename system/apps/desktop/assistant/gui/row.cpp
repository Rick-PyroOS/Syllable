#include "row.h"


ListViewDisplayRow::ListViewDisplayRow( const Result& result ) : ListViewRow(), m_cTitle( result.name() ), m_cDescription( result.description() )
{
	m_pcImage = result.iconPtr();
	m_cResult = result;
	SetIsSelectable( true );
}

ListViewDisplayRow::~ListViewDisplayRow()
{
}


void ListViewDisplayRow::AttachToView( os::View* pcView, int nColumn )
{
	//    	os::ListViewRow::AttachToView(pcView,nColumn);
}

void ListViewDisplayRow::SetRect( const os::Rect& cRect, int nColumn )
{
	//  	os::ListViewRow::SetRect(cRect,nColumn);
}

float ListViewDisplayRow::GetWidth( os::View* pcView, int nColumn )
{
	return 200;
}

float ListViewDisplayRow::GetHeight( os::View* pcView )
{
	return 64;
}


void ListViewDisplayRow::Paint( const os::Rect& cFrame, os::View* pcView, uint nColumn, bool bSelected, bool bHighlighted, bool bHasFocus )
{
	pcView->SetFgColor( os::Color32_s( 255, 255, 255, 0 ) );
	pcView->SetDrawingMode( os::DM_COPY );
	pcView->FillRect( cFrame );


	if( bSelected || bHighlighted )
	{
		pcView->SetDrawingMode( os::DM_BLEND );
		pcView->SetFgColor( os::Color32_s( 255, 120, 120, 128 ) );

		os::Rect frame = cFrame;
		frame.Resize( 1, 1, -1, -1 );
		pcView->FillRect( frame );
	}
	else
	{
		pcView->SetDrawingMode( os::DM_BLEND );
		pcView->SetFgColor( os::Color32_s( 120, 120, 120, 128 ) );

		os::Rect frame = cFrame;
		frame.Resize( 1, 1, -1, -1 );
		pcView->FillRect( frame );
	}


	if( m_pcImage )
	{
		pcView->SetDrawingMode( os::DM_BLEND );
		m_pcImage->Draw( os::Point( cFrame.left + 3, cFrame.top ), pcView );
	}

	pcView->SetDrawingMode( os::DM_OVER );
	pcView->SetFgColor( os::Color32_s( 255, 255, 255, 0 ) );
	pcView->DrawText( os::Rect( cFrame.left + 70, cFrame.top, 200, cFrame.bottom - 20 ), m_cTitle );

	pcView->DrawText( os::Rect( cFrame.left + 70, cFrame.bottom - 20, 200, cFrame.bottom ), m_cDescription );
}


bool ListViewDisplayRow::HitTest( os::View* pcView, const os::Rect& cFrame, int nColumn, os::Point cPos )
{
	return os::ListViewRow::HitTest( pcView, cFrame, nColumn, cPos );
}

bool ListViewDisplayRow::IsLessThan( const os::ListViewRow* pcOther, uint nColumn ) const
{
	return false;
}

Result& ListViewDisplayRow::GetResult()
{
	return m_cResult;
}
