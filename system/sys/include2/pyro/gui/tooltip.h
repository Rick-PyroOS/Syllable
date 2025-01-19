#ifndef _F_GUI_TOOLTIP_H__
#define _F_GUI_TOOLTIP_H__

#include <gui/view.h>
#include <gui/window.h>
#include <util/string.h>
#include <gui/guidefines.h>

namespace os
{
class ToolTipView : public os::View
{
	public:
		ToolTipView(const os::Rect& cRect,const os::String& cName, const os::String& cText);
		
		void Paint(const os::Rect& cRect);
		
		void SetToolTipBgColor(const Color32_s& sBgColor);
		void SetToolTipFgColor(const Color32_s& sFgColor);		
		void SetText(const os::String& cText);
		os::String GetText() const;
		os::Point GetPreferredSize(bool b) const;
private:
	os::String m_cText;
	os::Color32_s m_sToolTipBgColor;
	os::Color32_s m_sToolTipFgColor;
};

class ToolTip : public os::Window
{
public:
	
	ToolTip(const os::String& t);
	
	void SetTip(const os::String& t);
	os::String GetTip() const;
	
	void TimerTick(int nID);
	void ShowTip();
private:
	ToolTipView* pcView;
	enum { TIP_TIMER = 1021};
};
}
#endif

