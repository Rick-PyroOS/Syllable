#include "view.h"
#include <gui/button.h>
#include <gui/view.h>
ViewsLayoutView::ViewsLayoutView(const os::Rect& cFrame) : BaseView(cFrame){

	//TableView
	m_pcTableViewV1 = new os::TableView( os::Rect(), "", "",(int)GetBounds().Width(),  (int)GetBounds().Height() );
	m_pcTableViewV1->SetCellBorders( 2, 2, 2, 2 );


	m_pcTableViewV1->SetChild(new os::Button(os::Rect(),"button","Button 1",new os::Message()),1,1);
	m_pcTableViewV1->SetChild(new os::Button(os::Rect(),"button","Button 2",new os::Message()),2,1);

	m_pcTableViewV2 = new os::TableViewV2( GetBounds(), "", 4, 6, false, os::CF_FOLLOW_ALL );
	m_pcTableViewV2->SetRowSpacings( 3 );
	m_pcTableViewV2->SetColumnSpacings( 3 );

	// Create textview
	os::TextView* m_Result = new os::TextView( os::Rect(), "", "" );
	m_Result->SetReadOnly();
	m_pcTableViewV2->Attach( m_Result, 0, 4, 0, 1,  ( TABLE_EXPAND | TABLE_FILL ),  0, 4, 4 );

	// Create buttons
	os::Button* m_pcButton_clear = new os::Button( os::Rect(), "", "C", NULL );
	m_pcTableViewV2->Attach( m_pcButton_clear, 0, 1, 1, 2 );

	os::Button* m_pcButton_div = new os::Button( os::Rect(), "", "/", NULL );
	m_pcTableViewV2->Attach( m_pcButton_div, 1, 2, 1, 2 );

	os::Button* m_pcButton_multi = new os::Button( os::Rect(), "", "*", NULL );
	m_pcTableViewV2->Attach( m_pcButton_multi, 2, 3, 1, 2 );

	os::Button* m_pcButton_minus = new os::Button( os::Rect(), "", "-", NULL );
	m_pcTableViewV2->Attach( m_pcButton_minus, 3, 4, 1, 2 );

	os::Button* m_pcButton_plus = new os::Button( os::Rect(), "", "+", NULL );
	m_pcTableViewV2->Attach( m_pcButton_plus, 3, 4, 2, 4 );

	os::Button* m_pcButton_equal = new os::Button( os::Rect(), "", "=", NULL );
	m_pcTableViewV2->Attach( m_pcButton_equal, 3, 4, 4, 6 );

	os::Button* m_pcButton_decimal = new os::Button( os::Rect(), "", ".", NULL );
	m_pcTableViewV2->Attach( m_pcButton_decimal, 2, 3, 5, 6 );

	os::Button* m_pcButton_0 = new os::Button( os::Rect(), "", "0", NULL );
	m_pcTableViewV2->Attach( m_pcButton_0, 0, 2, 5, 6 );

	os::Button* m_pcButton_1 = new os::Button( os::Rect(), "", "1", NULL );
	m_pcTableViewV2->Attach( m_pcButton_1, 0, 1, 4, 5 );

	os::Button* m_pcButton_2 = new os::Button( os::Rect(), "", "2", NULL );
	m_pcTableViewV2->Attach(m_pcButton_2, 1, 2, 4, 5 );

	os::Button* m_pcButton_3 = new os::Button( os::Rect(), "", "3", NULL );
	m_pcTableViewV2->Attach( m_pcButton_3, 2, 3, 4, 5 );

	os::Button* m_pcButton_4 = new os::Button( os::Rect(), "", "4", NULL );
	m_pcTableViewV2->Attach( m_pcButton_4, 0, 1, 3, 4 );

	os::Button* m_pcButton_5 = new os::Button( os::Rect(), "", "5", NULL );
	m_pcTableViewV2->Attach( m_pcButton_5, 1, 2, 3, 4 );

	os::Button* m_pcButton_6 = new os::Button( os::Rect(), "", "6", NULL );
	m_pcTableViewV2->Attach( m_pcButton_6, 2, 3, 3, 4 );

	os::Button* m_pcButton_7 = new os::Button( os::Rect(), "", "7", NULL );
	m_pcTableViewV2->Attach( m_pcButton_7, 0, 1, 2, 3 );

	os::Button* m_pcButton_8 = new os::Button( os::Rect(), "", "8", NULL );
	m_pcTableViewV2->Attach( m_pcButton_8, 1, 2, 2, 3 );

	os::Button* m_pcButton_9 = new os::Button( os::Rect(), "", "9", NULL );
	m_pcTableViewV2->Attach( m_pcButton_9, 2, 3, 2, 3 );

	//Splitter
	m_pcSplitter = new os::Splitter( cFrame, "splitter", m_pcTableViewV1, m_pcTableViewV2,os::VERTICAL );
	m_pcSplitter->SetSplitRatio( 0.5 );

	// We need to manually call the Layout function after we have added children to the table
	m_pcTableViewV2->Layout();

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
