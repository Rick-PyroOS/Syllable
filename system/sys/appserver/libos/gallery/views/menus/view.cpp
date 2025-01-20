#include "view.h"
#include "../../common.h"

MenusLayoutView::MenusLayoutView(const os::Rect& cFrame) : BaseView(cFrame){
}

void MenusLayoutView::layout(){


	os::VLayoutNode* root = new os::VLayoutNode("");
	root->SetVAlignment(os::ALIGN_TOP);
	root->SetVAlignment(os::ALIGN_TOP);

	os::HLayoutNode* node1 = new os::HLayoutNode("layout_1");

	dropdown1 = new os::DropdownMenu( os::Rect(), "dropdown_1" );
	dropdown1->AppendItem("Apples");
	dropdown1->AppendItem("Oranges");
	dropdown1->AppendItem("Peaches");
	dropdown1->AppendItem("Strawberries");
	dropdown1->SetMaxPreferredSize( (int)GetBounds().Width() );
	node1->AddChild(dropdown1,2);


	//Popup Menus
	os::HLayoutNode* node2 = new os::HLayoutNode("layout_2");

	os::Menu* menu = new os::Menu(os::Rect(),"",os::ITEMS_IN_COLUMN);
	menu->AddItem("Apples", new os::Message(), "",NULL);
	menu->AddItem("Oranges", new os::Message(), "",NULL);
	menu->AddItem("Peaches", new os::Message(), "",NULL);
	menu->AddItem("Strawberries", new os::Message(), "",NULL);


	popupMenu1 = new os::PopupMenu(os::Rect(),"","Produce",menu,GetImageFromResource(os::String().Format("toggle_on.png")));
	node2->SetVAlignment(os::ALIGN_LEFT);
	node2->SetHAlignment(os::ALIGN_LEFT);
	node2->AddChild(popupMenu1);
	node2->AddChild(new os::HLayoutSpacer(""));
	root->AddChild(node1);
	root->AddChild(new os::VLayoutSpacer("",2,2));
	root->AddChild(node2);
	root->AddChild(new os::VLayoutSpacer(""));
	insertNode(root);
}

void MenusLayoutView::alignmentChanged(os::alignment) {
}

void MenusLayoutView::stateChanged(uint32 nState){

	switch (nState){
		case os::DISABLED:{
			dropdown1->SetEnable(false);
			popupMenu1->SetEnabled(false,true);
			break;
		}

		case os::NORMAL:
		default:{
			dropdown1->SetEnable(true);
			popupMenu1->SetEnabled(true,true);
			break;
		}

		case os::ACTIVE:{
			printf("Menus: SetState -> ACTIVE not implemented yet\n");
			break;
		}
		case os::HOVERED:{
			printf("Menus: SetState -> HOVERED not implemented yet\n");
			break;
		}
	}
}

void MenusLayoutView::styleChanged(struct Style s){

}

void MenusLayoutView::reset(os::alignment alignment,os::state state){
	alignmentChanged(alignment);
	stateChanged(state);
}
