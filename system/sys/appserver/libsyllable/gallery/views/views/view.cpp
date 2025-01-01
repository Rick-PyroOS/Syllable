#include "view.h"
#include <gui/button.h>
#include <gui/view.h>
ViewsLayoutView::ViewsLayoutView(const os::Rect& cFrame) : BaseView(cFrame){

	//TableView
	m_pcTableViewV1 = new os::TableView( os::Rect(), "", "",(int)GetBounds().Width(),  (int)GetBounds().Height() );
	m_pcTableViewV1->SetCellBorders( 2, 2, 2, 2 );


	m_pcTableViewV1->SetChild(new os::Button(os::Rect(),"button","Button 1",new os::Message()),1,1);
	m_pcTableViewV1->SetChild(new os::Button(os::Rect(),"button","Button 2",new os::Message()),2,1);

	//Splitter
	m_pcSplitter = new os::Splitter( os::Rect(), "splitter", m_pcTableViewV1, new os::View(os::Rect(),""),os::VERTICAL );
	m_pcSplitter->SetSplitRatio( 0.5 );

}

void ViewsLayoutView::layout(){
	os::VLayoutNode* root = new os::VLayoutNode("");
	root->SetVAlignment(os::ALIGN_TOP);
	root->SetVAlignment(os::ALIGN_TOP);
	root->AddChild(m_pcSplitter);
	insertNode(root);
}

void ViewsLayoutView::alignmentChanged(os::alignment) {
	printf("views: state changed\n");
}

void ViewsLayoutView::stateChanged(uint32 nState){
	switch (nState){
		case os::DISABLED:{
//			m_pcTableViewV1->SetEnable(false);
//			m_pcTableViewV2->SetEnable(false);
			break;
		}

		case os::NORMAL:
		default:{
//			m_pcTableViewV1->SetEnable(true);
//			m_pcTableViewV2->SetEnable(true);
			break;
		}

		case os::ACTIVE:{
			printf("TextViews: SetState -> ACTIVE not implemented\n");
			break;
		}
		case os::HOVERED:{
			printf("TextViews: SetState -> HOVERED not implemented\n");
			break;
		}
	}
}

void ViewsLayoutView::styleChanged(struct Style s){

}

void ViewsLayoutView::reset(os::alignment alignment,os::state state){
	alignmentChanged(alignment);
	stateChanged(state);
}
