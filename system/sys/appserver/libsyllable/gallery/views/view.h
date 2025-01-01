#ifndef  __VIEWS_H__
#define  __VIEWS_H__

#include <gui/window.h>
#include <gui/tabview.h>
#include <gui/view.h>
#include <gui/menu.h>
#include <gui/toolbar.h>
#include <gui/statusbar.h>
#include <gui/layoutview.h>
#include <gui/layoutview.h>
#include "../defines.h"

#include "button/view.h"
#include "controls/view.h"
#include "dates/view.h"
#include "label/view.h"
#include "lists/view.h"
#include "menus/view.h"
#include "textview/view.h"
#include "views/view.h"


class WidgetGalleryView : public os::View{
public:
	WidgetGalleryView(const os::Rect& cFrame);
	void AllAttached();
	virtual void HandleMessage(os::Message*);
public:
	BaseView* GetTab(uint32);
	BaseView* GetActiveTab();
private:
	void _SetupMenus();
	void _SetupToolBars();
	void _SetupStatusBars();

private:
	int32 m_nState;
	int32 m_nAlignment;
	os::Menu* m_pcMenu;
	os::Menu* alignmentMenu;
	os::Menu* statesMenu;
	os::ToolBar* m_pcToolBar;
	os::StatusBar* m_pcStatusBar;
	os::TabView* m_pcTabView;
	ButtonLayoutView* buttonView;

	ControlsLayoutView* controlsLayoutView;
	DatesLayoutView* datesLayoutView;
	LabelsLayoutView* labelView;
	ListsAndTreesLayoutView* listsAndTreesViewLayoutView;
	MenusLayoutView* menuView;
	ViewsLayoutView* viewsView;
	TextViewsLayoutView* textviewView;
	os::LayoutView* m_pcLayoutView;
	os::VLayoutNode* m_pcRootNode;
};

#endif
