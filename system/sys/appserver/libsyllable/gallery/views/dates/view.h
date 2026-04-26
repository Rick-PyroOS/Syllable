#ifndef __DATES__VIEW__H
#define __DATES__VIEW__H

#include "../baseview.h"
#include "../../defines.h"

#include <gui/stringview.h>
#include <gui/calendarview.h>
#include <gui/dateview.h>
#include <gui/splitter.h>

class DatesLayoutView : public BaseView{
public:
	DatesLayoutView(const os::Rect&);
private:
	os::DateView* dateView;
	os::CalendarView* calendarView;
	os::Splitter* splitter;
private:
	void layout();
	void alignmentChanged(os::alignment);
	void stateChanged(uint32 nState);
	void styleChanged(struct Style s);
	void reset(os::alignment alignment, os::state state);
};

#endif