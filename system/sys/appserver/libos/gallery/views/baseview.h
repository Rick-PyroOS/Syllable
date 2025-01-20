#ifndef __BASE_VIEW_H__
#define __BASE_VIEW_H__

#include <gui/layoutview.h>
#include <gui/view.h>
#include <util/message.h>
#include <gui/dropdownmenu.h>
#include <gui/stringview.h>
#include <gui/separator.h>

#include "../application.h"
#include "../messages.h"

struct Style{

};

class BaseView : public os::View{
public:

	BaseView(const os::Rect& cFrame) : os::View(cFrame, "",os::CF_FOLLOW_ALL){
		m_pcView = new os::LayoutView(cFrame, "",NULL,os::CF_FOLLOW_ALL);
		m_pcRoot = new os::VLayoutNode("__root__");
		m_pcRoot->SetBorders(os::Rect(5,5,5,5));

		m_pcRoot->AddChild(new os::Separator( os::Rect(), "separator", os::HORIZONTAL ),0);

		os::VLayoutSpacer* spacer = new os::VLayoutSpacer("");
		spacer->SetWeight(0);
		m_pcRoot->AddChild(spacer);

		m_pcView->SetRoot(m_pcRoot);
		AddChild(m_pcView);
	}

	void HandleMessage(os::Message* message){
		switch (message->GetCode()){
			default:
				View::HandleMessage(message);
				break;
		}
	}

public:
	os::LayoutView* getParent() const{
		return m_pcView;
	}
	void insertNode(os::LayoutNode* node){
		m_pcRoot->AddChild(node);

		os::VLayoutSpacer* spacer = new os::VLayoutSpacer("");
		spacer->SetWeight(0);
		m_pcRoot->AddChild(spacer);

		m_pcView->InvalidateLayout();
	}
public:
	virtual void layout() = 0;
	virtual void alignmentChanged(os::alignment) = 0;
	virtual void stateChanged(uint32 nState) = 0;
	virtual void styleChanged(struct Style s) = 0;
	virtual void reset(os::alignment,os::state) = 0;

public:
	void AllAttached(){
	}

private:
	void AttachedToWindow(){
		layout();
	}

private:
	os::LayoutView* m_pcView;
	os::VLayoutNode* m_pcRoot;
	os::DropdownMenu* stateMenu;
	os::DropdownMenu* alignmentMenu;
};

#endif
