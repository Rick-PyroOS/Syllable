#ifndef __TOP_VIEW_H__
#define __TOP_VIEW_H__

#include <gui/window.h>
#include <gui/layoutview.h>
#include <gui/listview.h>
#include "syll_gui/textview.h"
#include "providers/provider.h"
#include <gui/window.h>


class TopView : public os::View{
public:
	TopView(os::Window* parent,const os::Rect& cFrame);
	void MakeFocus(bool bFocus);
private:
	void _Layout();
	virtual void AllAttached();
	virtual void AttachedToWindow();
	virtual void KeyDown(const char*, const char*, uint32);
	virtual void HandleMessage(os::Message* pcMessage);
	virtual void TimerTick(int nID);
	void _Query();
	void _ShowResults(os::Message* pcMessage);
	void _LaunchSelected();
	os::Point GetPreferredSize(bool);
	os::String GetContent() const;
	void Paint(const os::Rect&);
	void FontChanged(os::Font* font);
private:
	os::TextView* m_pcView;
	os::ListView* m_pcListView;
	os::LayoutView* m_pcLayoutView;
	os::VLayoutNode* m_pcRoot;
	os::Window* m_pcParent;
	/* Only touched with the window locked (window thread, or MakeFocus), so no lock of its own */
	int32 m_nGeneration;
};

#endif














