#include "view.h"

#include <gui/stringview.h>

DatesLayoutView::DatesLayoutView(const os::Rect& cFrame) : BaseView(cFrame){
}

void DatesLayoutView::layout(){
	os::VLayoutNode* root = new os::VLayoutNode("");
	root->SetVAlignment(os::ALIGN_TOP);
	root->SetVAlignment(os::ALIGN_TOP);

	dateView = new os::DateView(os::DateTime::Now(),new os::Messenger(this));
	calendarView = new os::CalendarView(os::Rect(),"",new os::Messenger(this));

	splitter = new os::Splitter( os::Rect(), "splitter", dateView,calendarView,os::VERTICAL );
	splitter->SetSplitRatio( 0.5 );

	root->AddChild(splitter);
	insertNode(root);
}


void DatesLayoutView::alignmentChanged(os::alignment) {
	printf("dates: alignment changed not implemented yet\n");
}

void DatesLayoutView::stateChanged(uint32 nState){
	switch (nState){
		case os::DISABLED:{
			break;
		}

		case os::NORMAL:
		default:{
			break;
		}

		case os::ACTIVE:{
			break;
		}
		case os::HOVERED:{
			break;
		}
	}
}

void DatesLayoutView::styleChanged(struct Style s){

}

void DatesLayoutView::reset(os::alignment alignment,os::state state){
	alignmentChanged(alignment);
	stateChanged(state);
}
