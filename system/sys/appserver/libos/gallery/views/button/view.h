#ifndef __BUTTON__VIEW_H
#define __BUTTON__VIEW_H

#include "../baseview.h"
#include "../../defines.h"

#include <gui/button.h>
#include <gui/imagebutton.h>

class ButtonLayoutView : public BaseView{
public:
	ButtonLayoutView(const os::Rect&);
protected:
private:
	void layout();
	void alignmentChanged(os::alignment);
	void stateChanged(uint32 nState);
	void styleChanged(struct Style s);
	void reset(os::alignment alignment, os::state state);
private:
	os::Button* button_1;
	os::ImageButton* button_2;
};

#endif

