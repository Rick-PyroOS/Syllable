#ifndef __LABELS__VIEW__H
#define __LABELS__VIEW__H

#include "../baseview.h"
#include "../../defines.h"

#include <gui/stringview.h>

class LabelsLayoutView : public BaseView{
public:
	LabelsLayoutView(const os::Rect&);
private:
	os::StringView* stringView1;
	os::StringView* stringView2;
private:
	void layout();
	void alignmentChanged(os::alignment);
	void stateChanged(uint32 nState);
	void styleChanged(struct Style s);
	void reset(os::alignment alignment, os::state state);
};

#endif
