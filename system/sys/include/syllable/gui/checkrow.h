#ifndef _CHECK_ROW_H
#define _CHECK_ROW_H


#include <gui/listview.h>
#include <gui/view.h>
#include <gui/rect.h>
#include <util/messenger.h>
#include <gui/bitmap.h>
#include <gui/guidefines.h>

namespace os
{
class CheckRow : public os::ListViewStringRow
{
public:
	CheckRow(os::Messenger* target=NULL);
	
	void Paint( const Rect & cFrame, View * pcView, uint nColumn, bool bSelected, bool bHighlighted, bool bHasFocus );

	bool HitTest( View * pcView, const Rect & cFrame, int nColumn, Point cPos );
	void Toggle(os::View* pcView);
	bool IsChecked();
private:
	bool m_bIsChecked;
	os::Messenger* m_pcTarget;
	static os::Bitmap* s_pcCheckBitmap;
};
}

#endif



