#ifndef __DROPDOWN__VIEW__H
#define __DROPDOWN__VIEW__H

#include <gui/popupmenu.h>
#include <gui/dropdownmenu.h>
#include <gui/menu.h>

#include "../../defines.h"
#include "../baseview.h"


class MenusLayoutView : public BaseView{
public:
	MenusLayoutView(const os::Rect&);
private:
	os::DropdownMenu* dropdown1;
	os::PopupMenu* popupMenu1;
private:
	void layout();
	void alignmentChanged(os::alignment);
	void stateChanged(uint32 nState);
	void styleChanged(struct Style s);
	void reset(os::alignment alignment, os::state state);
};

#endif
