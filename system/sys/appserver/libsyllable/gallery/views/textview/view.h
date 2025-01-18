#ifndef __TEXTVIEW__VIEW__H
#define __TEXTVIEW__VIEW__H

#include <gui/textview.h>
#include "../baseview.h"
#include "../../defines.h"

class TextViewsLayoutView : public BaseView{
public:
	TextViewsLayoutView(const os::Rect&);
private:
	os::TextView* textView1;
	os::TextView* textView2;
private:
	void layout();
	void alignmentChanged(os::alignment);
	void stateChanged(uint32 nState);
	void styleChanged(struct Style s);
	void reset(os::alignment alignment, os::state state);
};

#endif
