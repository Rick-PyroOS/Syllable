#ifndef __LISTS_AND_TREES__VIEW__H
#define __LISTS_AND_TREES__VIEW__H

#include "../baseview.h"

#include <gui/listview.h>
#include <gui/treeview.h>
#include <gui/separator.h>
#include <gui/checkrow.h>
#include <gui/checknode.h>

#include "../../defines.h"


class ListsAndTreesLayoutView : public BaseView{
public:
	ListsAndTreesLayoutView(const os::Rect&);
private:
	os::TreeView* treeView1;
	os::ListView* listView1;
private:
	void layout();
	void alignmentChanged(os::alignment);
	void stateChanged(uint32 nState);
	void styleChanged(struct Style s);
	void reset(os::alignment alignment, os::state state);
};

#endif
