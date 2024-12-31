#ifndef __CONTROLS__VIEW__H
#define __CONTROLS__VIEW__H

#include "../baseview.h"
#include <gui/checkbox.h>
#include <gui/progressbar.h>
#include <gui/radiobutton.h>
#include <gui/slider.h>
#include <gui/spinner.h>
#include <gui/guidefines.h>
#include "../../defines.h"

class ControlsLayoutView : public BaseView{
public:
	ControlsLayoutView(const os::Rect&);
private:
	os::RadioButton* radioButton1;
	os::RadioButton* radioButton2;
	os::RadioButton* radioButton3;
	os::ProgressBar* progressBar1;
	os::CheckBox* checkbox1;
	os::Slider* slider1;
	os::Spinner* spinner1;
private:
	void layout();
	void alignmentChanged(os::alignment);
	void stateChanged(uint32 nState);
	void styleChanged(struct Style s);
	void reset(os::alignment alignment, os::state state);
};

#endif
