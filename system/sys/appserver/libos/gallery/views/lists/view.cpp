#include "view.h"




ListsAndTreesLayoutView::ListsAndTreesLayoutView(const os::Rect& cFrame) : BaseView(cFrame){

}

void ListsAndTreesLayoutView::layout(){
	os::VLayoutNode* root = new os::VLayoutNode("");
	root->SetVAlignment(os::ALIGN_TOP);
	root->SetVAlignment(os::ALIGN_TOP);

	os::HLayoutNode* node1 = new os::HLayoutNode("layout_1");

	listView1 = new os::ListView(os::Rect(),"listview_1",os::ListView::F_RENDER_BORDER | os::ListView::F_NO_AUTO_SORT);
	listView1->InsertColumn("Column 1",(int)GetBounds().Width()/3);
	listView1->InsertColumn("Column 2",(int)GetBounds().Width()/3);
	listView1->InsertColumn("Column 3",(int)GetBounds().Width()/3);

	for (int i=0; i<3; i++) {
		os::ListViewStringRow *pcRow = new os::ListViewStringRow();
		for (int j = 0; j < 3; j++) {
			pcRow->AppendString(os::String().Format("%d x %d",i+1,j+1));
		}
		listView1->InsertRow(pcRow);
	}

	os::CheckRow* row = new os::CheckRow();
	for (int i=0; i<3; i++) {
		row->AppendString(os::String().Format("4x%d",i));
	}
	listView1->InsertRow(row);


	node1->AddChild(listView1);
	root->AddChild(node1);
	root->AddChild( new os::Separator( os::Rect(), "separator", os::HORIZONTAL ), 0.0f );


	os::HLayoutNode* node2 = new os::HLayoutNode("");
	treeView1 = new os::TreeView(os::Rect(), "_treeview", ( os::ListView::F_NO_HEADER | os::ListView::F_RENDER_BORDER ), os::CF_FOLLOW_ALL );
	treeView1->InsertColumn( "Path", 1 );


	os::TreeViewStringNode *rootNode = new os::TreeViewStringNode();
	rootNode->AppendString( "/" );
	rootNode->SetIndent( 1 );
	treeView1->InsertNode( rootNode, false );
	for (int i=1; i<4; i++){
		for (int j=1; j<4; j++){
			os::TreeViewStringNode *node = new os::TreeViewStringNode();
			node->AppendString(os::String().Format("%dx%d",i,j));
			node->SetIndent(i+1);
			treeView1->InsertNode(node,false);
		}
	}

	os::TreeViewCheckNode* checkNode = new os::TreeViewCheckNode();
	for (int i=0; i<3; i++) {
		checkNode->AppendString(os::String().Format("4x%d",i));
	}
	checkNode->SetIndent(0);
	treeView1->InsertNode(checkNode,true);

	node2->AddChild( treeView1 );
	root->AddChild(node2);
	insertNode(root);

}

void ListsAndTreesLayoutView::alignmentChanged(os::alignment) {
	printf("lists: state changed\n");
}

void ListsAndTreesLayoutView::stateChanged(uint32 nState){
	printf("lists\n");

	switch (nState){
		case os::DISABLED:{
			treeView1->SetEnable(false);
			listView1->SetEnable(false);
			break;
		}

		case os::NORMAL:
		default:{
			treeView1->SetEnable(true);
			listView1->SetEnable(true);
			break;
		}

		case os::ACTIVE:{
			printf("Lists: SetState -> ACTIVE not implemented\n");
			break;
		}
		case os::HOVERED:{
			printf("Lists: SetState -> HOVERED not implemented\n");
			break;
		}
	}
}

void ListsAndTreesLayoutView::styleChanged(struct Style s){

}

void ListsAndTreesLayoutView::reset(os::alignment alignment,os::state state){
	alignmentChanged(alignment);
	stateChanged(state);
}

