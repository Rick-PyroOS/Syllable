#include "view.h"
#include "../common.h"
#include "../messages.h"
#include "../application.h"
#include <gui/imageview.h>
#include <gui/statusbar.h>
#include <gui/layoutview.h>
#include <util/application.h>
#include <unistd.h>
#include <iostream>

WidgetGalleryView::WidgetGalleryView(const os::Rect& cFrame) : os::View(cFrame, "View",os::CF_FOLLOW_ALL) {
	m_pcLayoutView = new os::LayoutView(GetBounds(),"",NULL);
	m_pcRootNode = new os::VLayoutNode("");
	m_nAlignment = os::ALIGN_LEFT;
	m_nState = os::NORMAL;

	buttonView = new ButtonLayoutView(cFrame);
	controlsLayoutView = new ControlsLayoutView(cFrame);
	datesLayoutView = new DatesLayoutView(cFrame);
	labelView = new LabelsLayoutView(cFrame);
	listsAndTreesViewLayoutView = new ListsAndTreesLayoutView(cFrame);
	menuView = new MenusLayoutView(cFrame);
	textviewView = new TextViewsLayoutView(cFrame);
	viewsView = new ViewsLayoutView(cFrame);

	_SetupMenus();
	_SetupToolBars();


	m_pcTabView = new os::TabView(os::Rect(),"_main_view",os::CF_FOLLOW_ALL);
	m_pcTabView->SetMessage(new os::Message(M_TAB_CHANGED));
	m_pcTabView->AppendTab("Buttons", (View*)buttonView);
	m_pcTabView->AppendTab("Calendar and Time Selectors",(View*)datesLayoutView);
	m_pcTabView->AppendTab("Controls", (View*)controlsLayoutView);
	m_pcTabView->AppendTab("Dropdown and Popup Menus", (View*)menuView);
	m_pcTabView->AppendTab("Labels", (View*)labelView);
	m_pcTabView->AppendTab("Lists & Trees", (View*)listsAndTreesViewLayoutView);
	m_pcTabView->AppendTab("TextView", (View*)textviewView);
	m_pcTabView->AppendTab("Views", (View*)viewsView);

	m_pcRootNode->AddChild(m_pcTabView,1);

	_SetupStatusBars();
}

void WidgetGalleryView::_SetupMenus(){
	m_pcMenu = new os::Menu( os::Rect(0,0,1,1), "main_menu", os::ITEMS_IN_ROW, os::CF_FOLLOW_LEFT | os::CF_FOLLOW_RIGHT, os::WID_FULL_UPDATE_ON_H_RESIZE );
	m_pcMenu->SetName("_menu");
	m_pcMenu->ResizeTo(GetBounds().Width(),25);

	os::Menu* appMenu = new os::Menu(os::Rect(), "Application", os::ITEMS_IN_COLUMN, os::CF_FOLLOW_LEFT | os::CF_FOLLOW_RIGHT | os::CF_FOLLOW_TOP );
	appMenu->AddItem("Splash Screen",new os::Message(M_SPLASH_SCREEN));


	os::Menu* normalMenu = new os::Menu(os::Rect(), "Normal", os::ITEMS_IN_COLUMN, os::CF_FOLLOW_LEFT | os::CF_FOLLOW_RIGHT | os::CF_FOLLOW_TOP );

	os::Menu* disabledMenu = new os::Menu(os::Rect(), "Disabled", os::ITEMS_IN_COLUMN, os::CF_FOLLOW_LEFT | os::CF_FOLLOW_RIGHT | os::CF_FOLLOW_TOP );
	disabledMenu->AddItem(new os::MenuItem("Disabled MenuItem",new os::Message() ));

	os::Menu* disabledSubMenu = new os::Menu(os::Rect(),"Disabled Menu",os::ITEMS_IN_COLUMN);
	disabledSubMenu->AddItem(new os::MenuItem("Disabled SubMenu Item",new os::Message()));
	disabledSubMenu->SetEnable(false);
	disabledMenu->AddItem(disabledSubMenu);
	disabledMenu->GetItemAt(0)->SetEnable(false);

	os::Menu* iconMenu = new os::Menu(os::Rect(), "Icon", os::ITEMS_IN_COLUMN, os::CF_FOLLOW_LEFT | os::CF_FOLLOW_RIGHT | os::CF_FOLLOW_TOP );
	App* app = (App*) os::Application::GetInstance();
	std::map<os::String,uint32> alignments = app->GetAlignments();
	for (std::map<os::String, uint32>::const_iterator it = alignments.begin(); it != alignments.end(); ++it){
		os::String alignment = it->first;
		os::String lower = os::String(alignment).Lower();
		os::Message* alignmentMessage = new os::Message();
		iconMenu->AddItem(alignment.c_str(), alignmentMessage, "",GetImageFromResource(os::String().Format("align_%s.png", lower.c_str()),os::Point(16,16)));
	}

	m_pcMenu->AddItem(appMenu);
	m_pcMenu->AddItem(normalMenu);
	m_pcMenu->AddItem(disabledMenu);
	m_pcMenu->AddItem(iconMenu);
	m_pcRootNode->AddChild(m_pcMenu,0);
}

void WidgetGalleryView::_SetupToolBars() {
	m_pcToolBar = new os::ToolBar( os::Rect(0,0,1,1), "_tool_bar",os::CF_FOLLOW_LEFT | os::CF_FOLLOW_RIGHT );

	App* app = (App*) os::Application::GetInstance();

	alignmentMenu = new os::Menu(os::Rect(), "Alignment", os::ITEMS_IN_COLUMN, os::CF_FOLLOW_LEFT | os::CF_FOLLOW_RIGHT | os::CF_FOLLOW_TOP );
	std::map<os::String,uint32> alignments = app->GetAlignments();
	for (std::map<os::String, uint32>::const_iterator it = alignments.begin(); it != alignments.end(); ++it){
		os::String alignment = it->first;
		os::String lower = os::String(alignment).Lower();
		os::Message* alignmentMessage = new os::Message(M_ALIGNMENT_CHANGED);
		alignmentMessage->AddInt32("selection",it->second);
		alignmentMenu->AddItem(alignment.c_str(), alignmentMessage, "",GetImageFromResource(os::String().Format("align_%s.png", lower.c_str()),os::Point(16,16)));
	}

	statesMenu = new os::RadioMenu(os::Rect(), "State", os::ITEMS_IN_COLUMN, os::CF_FOLLOW_LEFT | os::CF_FOLLOW_RIGHT | os::CF_FOLLOW_TOP );
	std::map<os::String,uint32> states = app->GetStates();
	for (std::map<os::String, uint32>::const_iterator it = states.begin(); it != states.end(); ++it){
		os::String state = it->first;
		os::Message* stateMessage = new os::Message(M_STATE_CHANGED);
		stateMessage->AddInt32("selection",it->second);
		os::RadioMenuItem* item = new os::RadioMenuItem(state.c_str(), stateMessage);
		item->SetRadio(it->second == m_nState);
		statesMenu->AddItem(item);
	}


	m_pcToolBar->AddPopupMenu("_alignment","Alignment", GetImageFromResource(os::String().Format("align_%s.png",os::String("left").c_str())),alignmentMenu);
	m_pcToolBar->AddSeparator("");
	m_pcToolBar->AddPopupMenu("_state","Normal", GetImageFromResource(os::String().Format("toggle_on.png")),statesMenu);

	m_pcToolBar->ResizeTo(GetBounds().Width(),40);
	m_pcRootNode->AddChild(m_pcToolBar,0);
}

void WidgetGalleryView::_SetupStatusBars() {
	m_pcStatusBar = new os::StatusBar( os::Rect(), "_status_bar",os::CF_FOLLOW_LEFT | os::CF_FOLLOW_RIGHT );
	m_pcStatusBar->AddPanel( "message", "Message" );
	os::StatusPanel* panel = new os::StatusPanel("","",1.0,0);
	panel->AddChild(
			new os::ImageView(os::Rect(0,0,24,24),"Image",GetImageFromResource("icon48x48.png")),
			false
	);
	m_pcStatusBar->AddChild(panel);

	m_pcStatusBar->ResizeTo(GetBounds().Width(),25);
	m_pcRootNode->AddChild(m_pcStatusBar,0);
}

void WidgetGalleryView::AllAttached(){
	m_pcRootNode->FindNode("_tool_bar")->AdjustPrefSize(new os::Point(GetBounds().Width(),40),new os::Point(GetBounds().Width(),40));
	m_pcRootNode->FindNode("_status_bar")->AdjustPrefSize(new os::Point(GetBounds().Width(),25),new os::Point(GetBounds().Width(),25));
	m_pcRootNode->FindNode("_menu")->AdjustPrefSize(new os::Point(GetBounds().Width(),25),new os::Point(GetBounds().Width(),25));

	statesMenu->SetTargetForItems(this);
	alignmentMenu->SetTargetForItems(this);
	m_pcMenu->SetTargetForItems(this);

	m_pcLayoutView->SetRoot(m_pcRootNode);
	m_pcRootNode->Layout();
	m_pcTabView->SetTarget(this);
	AddChild(m_pcLayoutView);
	m_pcLayoutView->InvalidateLayout();
}

void WidgetGalleryView::HandleMessage(os::Message *pcMessage) {
	switch (pcMessage->GetCode()) {

		case M_SPLASH_SCREEN:{
			os::Splash* splash = os::Splash::Go((os::BitmapImage*)GetImageFromResource("splash.png",os::Point(400,266)),"");
			splash->SetTextColor(os::Color32_s(255,255,255,0));
			for (int i=0; i<100; i++){
				splash->SetText(os::String().Format("Progress: %.2f %%", (i/100.0f)*100));
				splash->SetProgress(i / 100.0f);
				usleep(50000);
			}
			splash->OkToQuit();
			break;
		}
		case M_TAB_CHANGED: {
			BaseView *view = (BaseView *) m_pcTabView->GetTabView(m_pcTabView->GetSelection());
			view->reset((os::alignment)m_nAlignment,(os::state)m_nState);
			break;
		}

		case M_ALIGNMENT_CHANGED: {
			int32 selection;
			if (pcMessage->FindInt32("selection", &selection) == 0) {
				m_nAlignment = selection;
				App* application =  (App*) os::Application::GetInstance();
				os::PopupMenu* menu = (os::PopupMenu*)m_pcToolBar->FindNode("_alignment")->GetView();
				os::Image* image = GetImageFromResource(
						os::String().Format("align_%s.png",application->GetAlignment(selection).Lower().c_str())
				);
				menu->SetIcon(image);
				for (int i = 0; i < m_pcTabView->GetTabCount(); i++) {
					BaseView *view = GetTab(i);
					view->alignmentChanged((os::alignment)(selection));
				}
			}
			break;
		}
		case M_STATE_CHANGED: {
			int32 selection;
			if (pcMessage->FindInt32("selection", &selection) == 0) {
				m_nState = selection;
				App* application =  (App*) os::Application::GetInstance();
				os::PopupMenu* menu = (os::PopupMenu*)m_pcToolBar->FindNode("_state")->GetView();
				menu->SetLabel(application->GetState(selection));

				for (int i = 0; i < m_pcTabView->GetTabCount(); i++) {
					BaseView *view = GetTab(i);
					view->stateChanged((os::state)(selection));
				}
			}
			break;
		}
		case M_STYLE_CHANGED: {
			int32 selection;
			if (pcMessage->FindInt32("selection", &selection) == 0) {
				for (int i = 0; i < m_pcTabView->GetTabCount(); i++) {
					BaseView *view = GetTab(i);
					struct Style s = {};
					view->styleChanged(s);
				}
			}
			break;
		}
		default:
			View::HandleMessage(pcMessage);
			break;
	}
}

BaseView* WidgetGalleryView::GetTab(uint32 i) {
	if (m_pcTabView) {
		BaseView *view = (BaseView *) m_pcTabView->GetTabView(i);
		return view;
	}
	return NULL;
}

BaseView* WidgetGalleryView::GetActiveTab(){
	if (m_pcTabView){
		return GetTab(m_pcTabView->GetSelection());
	}
	return NULL;
}

