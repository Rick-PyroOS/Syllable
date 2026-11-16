#ifndef __ROW__H__
#define __ROW__H__

#include <gui/listview.h>
#include <gui/view.h>
#include "../providers/result.h"

using namespace assistant;

class ListViewDisplayRow : public os::ListViewRow
{
public:
	ListViewDisplayRow( const Result& result );
	~ListViewDisplayRow();

	void AttachToView( os::View* pcView, int nColumn );
	void SetRect( const os::Rect& cRect, int nColumn );
	float GetWidth( os::View* pcView, int nColumn );
	float GetHeight( os::View* pcView );
	void Paint( const os::Rect& cFrame, os::View* pcView, uint nColumn, bool bSelected, bool bHighlighted, bool bHasFocus );
	bool HitTest( os::View* pcView, const os::Rect& cFrame, int nColumn, os::Point cPos );
	bool IsLessThan( const os::ListViewRow* pcOther, uint nColumn ) const;
	Result& GetResult();

private:
	os::String m_cTitle;
	os::String m_cDescription;
	/* Shares ownership with the Result, so the icon outlives a cache rebuild */
	IconPtr m_pcImage;
	Result m_cResult;
};

#endif
