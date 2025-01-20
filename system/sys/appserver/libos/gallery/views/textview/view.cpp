#include "view.h"

TextViewsLayoutView::TextViewsLayoutView(const os::Rect& cFrame) : BaseView(cFrame){

}

void TextViewsLayoutView::layout(){
	os::VLayoutNode* root = new os::VLayoutNode("");
	root->SetVAlignment(os::ALIGN_TOP);
	root->SetVAlignment(os::ALIGN_TOP);


	textView1 = new os::TextView( os::Rect(), "", "",os::CF_FOLLOW_LEFT | os::CF_FOLLOW_RIGHT);
	root->AddChild(textView1);


	textView2 = new os::TextView( os::Rect(), "", "",os::CF_FOLLOW_LEFT | os::CF_FOLLOW_RIGHT);
	textView2->SetMultiLine(true);
	textView2->Set("Lorem Ipsum is simply dummy text of the printing and typesetting industry. Lorem Ipsum has been the industry's standard dummy text ever since the 1500s, when an unknown printer took a galley of type and scrambled it to make a type specimen book. It has survived not only five centuries, but also the leap into electronic typesetting, remaining essentially unchanged. It was popularised in the 1960s with the release of Letraset sheets containing Lorem Ipsum passages, and more recently with desktop publishing software like Aldus PageMaker including versions of Lorem Ipsum");
	root->AddChild(textView2);

	insertNode(root);
}

void TextViewsLayoutView::alignmentChanged(os::alignment) {
	printf("textviews: alignment changed - not implemented yet\n");
}

void TextViewsLayoutView::stateChanged(uint32 nState){
	switch (nState){
		case os::DISABLED:{
			textView1->SetEnable(false);
			textView2->SetEnable(false);
			break;
		}

		case os::NORMAL:
		default:{
			textView1->SetEnable(true);
			textView2->SetEnable(true);
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

void TextViewsLayoutView::styleChanged(struct Style s){
}

void TextViewsLayoutView::reset(os::alignment alignment,os::state state){
	alignmentChanged(alignment);
	stateChanged(state);
}
