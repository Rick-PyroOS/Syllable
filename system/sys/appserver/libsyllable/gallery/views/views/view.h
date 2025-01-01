#ifndef __VIEWS__VIEW__H
#define __VIEWS__VIEW__H

#include "../baseview.h"
#include <gui/splitter.h>
#include <gui/frameview.h>
#include <gui/tableview.h>
#include "../../defines.h"


class ViewsLayoutView : public BaseView{
public:
	ViewsLayoutView(const os::Rect&);
private:
	void layout();
	void alignmentChanged(os::alignment);
	void stateChanged(uint32 nState);
	void styleChanged(struct Style s);
	void reset(os::alignment alignment, os::state state);
private:
	os::TableView* m_pcTableViewV1;
	os::FrameView* m_pcTableViewV2;
	os::Splitter* m_pcSplitter;
};

#endif
