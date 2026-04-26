#include "view.h"

#include <gui/stringview.h>

LabelsLayoutView::LabelsLayoutView(const os::Rect& cFrame) : BaseView(cFrame){
}

void LabelsLayoutView::layout(){
	os::VLayoutNode* root = new os::VLayoutNode("");
	root->SetVAlignment(os::ALIGN_TOP);
	root->SetVAlignment(os::ALIGN_TOP);

	os::HLayoutNode* node1 = new os::HLayoutNode("layout_1");
	stringView1 = new os::StringView( os::Rect(), "stringview_1", "Label 1" );

	node1->AddChild(stringView1);
	node1->AddChild(new os::HLayoutSpacer(""));

	os::VLayoutNode* node2 = new os::VLayoutNode("layout_2");
	stringView2 = new os::StringView( os::Rect(), "stringview_2", "",os::DTF_WRAP_SOFT );
	stringView2->SetString("Lorem Ipsum is simply dummy text of the printing and typesetting industry. Lorem Ipsum has been the industry's standard dummy text ever since the 1500s, when an unknown printer took a galley of type and scrambled it to make a type specimen book. It has survived not only five centuries, but also the leap into electronic typesetting, remaining essentially unchanged. It was popularised in the 1960s with the release of Letraset sheets containing Lorem Ipsum passages, and more recently with desktop publishing software like Aldus PageMaker including versions of Lorem Ipsum");
	node2->AddChild(stringView2);

	root->AddChild(node1);
	root->AddChild(node2);
	root->AddChild(new os::VLayoutSpacer(""));

	insertNode(root);
}


void LabelsLayoutView::alignmentChanged(os::alignment) {
	printf("labels: alignment changed not implemented yet\n");
}

void LabelsLayoutView::stateChanged(uint32 nState){
	switch (nState){
		case os::DISABLED:{
//			stringView1->SetEnable(false);
//			stringView2->SeEnable(false);
			break;
		}

		case os::NORMAL:
		default:{
//			stringView1->SetEnable(true);
//			stringView2->SeEnable(true);
			break;
		}

		case os::ACTIVE:{
			printf("Label: SetState -> ACTIVE not implemented\n");
			break;
		}
		case os::HOVERED:{
			printf("Label: SetState -> HOVERED not implemented\n");
			break;
		}
	}
}

void LabelsLayoutView::styleChanged(struct Style s){

}

void LabelsLayoutView::reset(os::alignment alignment,os::state state){
	alignmentChanged(alignment);
	stateChanged(state);
}
