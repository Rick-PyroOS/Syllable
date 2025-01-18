#include "view.h"
#include "../../common.h"



#include <util/message.h>
#include <gui/separator.h>


ButtonLayoutView::ButtonLayoutView(const os::Rect& cFrame) : BaseView(cFrame){
}

void ButtonLayoutView::layout() {
	os::VLayoutNode* root = new os::VLayoutNode("");
	root->SetVAlignment(os::ALIGN_TOP);
	root->SetVAlignment(os::ALIGN_TOP);

	os::HLayoutNode* node1 = new os::HLayoutNode("layout_1");
	button_1 = new os::Button(os::Rect(),"button_1","Button",new os::Message(),os::CF_FOLLOW_ALL);
	node1->AddChild(button_1,0.5);


	os::HLayoutNode* node3 = new os::HLayoutNode("layout_3");
	button_2 = new os::ImageButton(os::Rect(),"image_button_1","Image Button",new os::Message(),GetImageFromResource("icon.png"),os::ImageButton::IB_TEXT_LEFT,true,true);
	node3->AddChild(button_2);

	root->AddChild(node1);
	root->AddChild( new os::Separator( os::Rect(), "separator", os::HORIZONTAL ), 0.0f );
	root->AddChild(node3);
	insertNode(root);
}

void ButtonLayoutView::alignmentChanged(os::alignment alignment) {

	//FIXME: Fix alignment in os::Button, images will be part of the button too so no need for two classes
	//button_1->SetTextAlignment();

	printf("alignmentChanged: %d x %d\n", alignment,os::ImageButton::IB_TEXT_TOP);

	//FIXME: This is sad, they should be identical without the need to use a specific enum
	switch(alignment){
		case os::ALIGN_LEFT: {
			button_2->SetTextPosition(os::ImageButton::IB_TEXT_LEFT);
			break;
		}
		case os::ALIGN_RIGHT: {
			button_2->SetTextPosition(os::ImageButton::IB_TEXT_RIGHT);
			break;
		}
		case os::ALIGN_TOP: {
			button_2->SetTextPosition(os::ImageButton::IB_TEXT_TOP);
			break;
		}
		case os::ALIGN_BOTTOM: {
			button_2->SetTextPosition(os::ImageButton::IB_TEXT_BOTTOM);
			break;
		}

		case os::ALIGN_CENTER:
		default:
			if (alignment & os::ALIGN_CENTER){
				printf("Button: alignmentChanged -> Center not implemented\n");
			}
			button_2->SetTextPosition(os::ImageButton::IB_TEXT_LEFT);
	}
}

void ButtonLayoutView::stateChanged(uint32 nState){
	switch (nState){
		case os::DISABLED:{
			button_2->SetEnable(false);
			button_1->SetEnable(false);
			break;
		}
		case os::ACTIVE:{
			printf("Button: SetState -> ACTIVE not implemented\n");
			break;
		}
		case os::HOVERED:{
			printf("Button: SetState -> HOVERED not implemented\n");
			break;
		}

		case os::NORMAL:
		default:{
			button_2->SetEnable(true);
			button_1->SetEnable(true);
		}
	}
}


void ButtonLayoutView::styleChanged(struct Style s){

}

void ButtonLayoutView::reset(os::alignment alignment,os::state state){
	alignmentChanged(alignment);
	stateChanged(state);
}


