#include "view.h"

ControlsLayoutView::ControlsLayoutView(const os::Rect& cFrame) : BaseView(cFrame){
}



void ControlsLayoutView::layout(){

	os::VLayoutNode* root = new os::VLayoutNode("");
	root->SetVAlignment(os::ALIGN_TOP);
	root->SetVAlignment(os::ALIGN_TOP);

	os::HLayoutNode* node1 = new os::HLayoutNode("layout_1");
	checkbox1 = new os::CheckBox(os::Rect(),"checkbox_1","Checkbox", new os::Message() );
	node1->AddChild(checkbox1,0.5);
	node1->AddChild(new os::HLayoutSpacer(""));


	os::HLayoutNode* node3 = new os::HLayoutNode("");
	progressBar1 = new os::ProgressBar(os::Rect(),"progress");
	progressBar1->SetProgress(0.5);
	node3->AddChild(progressBar1);


	os::HLayoutNode* node4 = new os::HLayoutNode("");


	radioButton1 = new os::RadioButton( os::Rect(), "", "Radio 1", new os::Message() );
	radioButton1->SetValue( true );
	node4->AddChild(radioButton1);

	radioButton2 = new os::RadioButton( os::Rect(), "", "Radio 2", new os::Message() );
	radioButton2->SetValue( false);
	node4->AddChild(radioButton2);

	radioButton3 = new os::RadioButton( os::Rect(), "", "Radio 3", new os::Message() );
	radioButton3->SetValue( false );
	node4->AddChild(radioButton3);



	os::HLayoutNode* node5 = new os::HLayoutNode("");
	slider1 = new os::Slider(os::Rect(), "", new os::Message(), os::Slider::TICKS_BELOW, 10,os::Slider::KNOB_SQUARE, os::HORIZONTAL );
	slider1->SetMinMax( 0, 100 );
	slider1->SetProgStrFormat( "Slider (%.1f%%)" );
	node5->AddChild(slider1);


	os::HLayoutNode* node6 = new os::HLayoutNode("");
	spinner1 = new os::Spinner( os::Rect(), "spinner_!", 0.0f, new os::Message() );
	spinner1->SetMinMax( 0.0f, 100.0f );
	spinner1->SetStep( 0.01f );
	spinner1->SetScale( 1.0f );
	spinner1->SetFormat( "%.2f" );
	node6->AddChild(spinner1);

	root->AddChild(node1);
	root->AddChild(node3);
	root->AddChild(node4);
	root->AddChild(node5);
	root->AddChild(node6);

	root->SameHeight("node_1","node_2",NULL);
	root->SameWidth("checkbox_2","checkbox_4",NULL);

	insertNode(root);
}

void ControlsLayoutView::alignmentChanged(os::alignment) {
	printf("controls: alignment changed not implemented yet\n");
}

void ControlsLayoutView::stateChanged(uint32 nState){
	switch (nState){
		case os::DISABLED:{
			radioButton1->SetEnable(false);
			radioButton2->SetEnable(false);
			radioButton3->SetEnable(false);
			slider1->SetEnable(false);
			spinner1->SetEnable(false);
			checkbox1->SetEnable(false);
			//progressBar1->SetEnable(false);
			break;
		}

		case os::NORMAL:
		default:{
			radioButton1->SetEnable(true);
			radioButton2->SetEnable(true);
			radioButton3->SetEnable(true);
			slider1->SetEnable(true);
			spinner1->SetEnable(true);
			checkbox1->SetEnable(true);
			//progressBar1->SetEnable(true);
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
	}
}


void ControlsLayoutView::styleChanged(struct Style s){

}

void ControlsLayoutView::reset(os::alignment alignment,os::state state){
	alignmentChanged(alignment);
	stateChanged(state);
}
