// EFileBrowser (C)opyright 2008 Jonas Jarvoll
//
// This is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
// See the GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

#include <gui/menu.h>
#include <gui/checkmenu.h>
#include <gui/popupmenu.h>
#include <gui/statusbar.h>

#include "layouter.h"
#include "application.h"
#include "common.h"
#include "preferences.h"
#include "messages.h"
#include "labels.h"
#include "statusbar.h"

#include "settings/settingsdialog.h"
#include "my_gui/sidebar_base.h"
#include "my_gui/address_field.h"
#include "syll_gui/tabview.h"
#include "syll_gui/splitter.h"
#include "syll_gui/toolbar.h"
#include "syll_gui/radiomenu.h"
#include "syll_gui/iconview.h"
#include "tabs/tab_browser.h"
#include "resources/lang.h"
#include "../include/plugin.h"

#include <vector>
#include <unistd.h>


	

/* 
   The Layouter is a Singleton and is responsible for handling the layout
   of the different widgets such as the iconbar and the image. It calculates
   the size and position for every widget depending on the current state (e.g. 
   if it is in fullscreen mode or not, if scrollbars is visible or not etc.)
*/

///////////////////////////////////////////////////////////////////////////////
//
// S I N G L E T O N
//
///////////////////////////////////////////////////////////////////////////////

Layouter* Layouter::m_theInstance = NULL;

Layouter* Layouter :: GetInstance()
{
	if( m_theInstance == NULL )
	{
		m_theInstance = new Layouter();
	}
	return m_theInstance;
}

///////////////////////////////////////////////////////////////////////////////
//
// P R I V A T E
//
///////////////////////////////////////////////////////////////////////////////


class Layouter :: _Private
{
public:
	_Private()
	{		
		_Parent = NULL;
		_ShowMenu = _ShowToolbar = _ShowAddressField = _ShowStatusbar = _ShowSidebar = false;
		_ShowHiddenFiles = Preferences::GetInstance()->GetShowHiddenFiles();
		_SettingsDialog = NULL;
		
	}

	void _SetViewSelection(IconView::view_type mode)
	{
		TabBase* tab = _GetCurrentTab();
		tab->SetViewMode(mode);
	}

	void _CreateMenu()
	{
		tShortcuts shortcuts = Shortcuts::GetInstance()->GetShortcuts();
		_MenuBar = new os::Menu( os::Rect(), "", os::ITEMS_IN_ROW );				
	
		// Application menu
		_ApplicationMenu = new os::Menu( os::Rect(), ID_MSG_MENU_APPLICATION_MENU, os::ITEMS_IN_COLUMN );
		_ApplicationMenu->AddItem( ID_MSG_MENU_APPLICATION_MENU_QUIT, new os::Message( MSG_MENU_APP_QUIT ), shortcuts["quit"] );
		_ApplicationMenu->AddItem( new os::MenuSeparator() );
		_ApplicationMenu->AddItem( ID_MSG_MENU_APPLICATION_MENU_ABOUT, new os::Message( MSG_MENU_APP_ABOUT ) );
		_MenuBar->AddItem( _ApplicationMenu );

		_LabelSelectMenu = Labels::GetInstance()->GetMenu(MSG_MENU_SELECT_BY_LABEL);
		_LabelSelectMenu->SetLabel("Select Items By Label");
		
		os::Menu* _EditMenu = new os::Menu(os::Rect(),"Edit",os::ITEMS_IN_COLUMN);
		_EditMenu->AddItem("Cut",new os::Message(MSG_CUT),shortcuts["cut"]);
		_EditMenu->AddItem("Copy",new os::Message(MSG_COPY),shortcuts["copy"]);
		_EditMenu->AddItem("Paste",new os::Message(MSG_PASTE),shortcuts["paste"]);
		_EditMenu->AddItem(new os::MenuSeparator());

		
	
		_EditMenu->AddItem("Select All",new os::Message(MSG_SELECT_ALL),shortcuts["select_all"]);
		_EditMenu->AddItem("Invert Selection",new os::Message(MSG_INVERT_SELECTION),shortcuts["invert_selection"]);
		_EditMenu->AddItem(_LabelSelectMenu);	
		_EditMenu->AddItem("Select Items Matching...",new os::Message());
		_EditMenu->AddItem(new os::MenuSeparator());
		_EditMenu->AddItem("Move To...",new os::Message());
		_EditMenu->AddItem("Copy To...",new os::Message());
		_EditMenu->AddItem(new os::MenuSeparator());
		_EditMenu->AddItem("Preferences...",new os::Message(MSG_SETTINGS_DIALOG));
		
		_MenuBar->AddItem(_EditMenu);

		// Toolbar submenu
		os::Menu* _ToolbarSubMenu = new os::Menu(os::Rect(), "Toolbar", os::ITEMS_IN_COLUMN);

		_ToolbarMenu_TextAndIcon = new os::RadioMenuItem( "Text and Icons", new os::Message( MSG_MENU_VIEW_TOOLBAR_BOTH ) );
		_ToolbarMenu_TextAndIcon->SetRadio(Preferences::GetInstance()->GetToolBarType() == os::ImageButton::IB_TEXT_BOTTOM);
		_ToolbarSubMenu->AddItem( _ToolbarMenu_TextAndIcon );
		
		_ToolbarMenu_IconsOnly = new os::RadioMenuItem( "Icons only", new os::Message( MSG_MENU_VIEW_TOOLBAR_ICON ) );;
		_ToolbarMenu_IconsOnly->SetRadio(Preferences::GetInstance()->GetToolBarType() == os::ImageButton::IB_TEXT_NONE);
		_ToolbarSubMenu->AddItem( _ToolbarMenu_IconsOnly );
		
		_ToolbarMenu_TextOnly = new os::RadioMenuItem( "Text only", new os::Message( MSG_MENU_VIEW_TOOLBAR_TEXT ) );
		_ToolbarMenu_TextOnly->SetRadio(Preferences::GetInstance()->GetToolBarType() == os::ImageButton::IB_TEXT_ONLY);
		_ToolbarSubMenu->AddItem( _ToolbarMenu_TextOnly );
	
		_ToolbarSubMenu->AddItem( new os::MenuSeparator() );
	
		_ToolbarMenu_Hide = new os::CheckMenu( "Show Toolbar", new os::Message( MSG_MENU_VIEW_TOOLBAR_HIDE ) );
		_ToolbarMenu_Hide->SetChecked( Preferences::GetInstance()->GetShowToolBar() );
		_ToolbarSubMenu->AddItem( _ToolbarMenu_Hide );
		
		
		

		// Tabs submenu		
		os::Menu* _TabSubMenu = new os::Menu(os::Rect(), "Tabs", os::ITEMS_IN_COLUMN);
		_ViewMenu_TabShow = new os::CheckMenu( "Show", new os::Message( MSG_MENU_VIEW_TAB_SHOW ) );
		_ViewMenu_TabShow->SetChecked( false );
		_TabSubMenu->AddItem( _ViewMenu_TabShow );
		_ViewMenu_TabHide = new os::CheckMenu( "Hide", new os::Message( MSG_MENU_VIEW_TAB_HIDE ) );
		_ViewMenu_TabHide->SetChecked( false );
		_TabSubMenu->AddItem( _ViewMenu_TabHide );
		_TabSubMenu->AddItem( new os::MenuSeparator() );
		_ViewMenu_TabAutomatic = new os::CheckMenu( "Automatic", new os::Message( MSG_MENU_VIEW_TAB_AUTOMATIC ) );
		_ViewMenu_TabAutomatic->SetChecked(true );
		_TabSubMenu->AddItem( _ViewMenu_TabAutomatic );

		// Address submenu		
		os::Menu* _AdrSubMenu = new os::Menu(os::Rect(), "Addressfield", os::ITEMS_IN_COLUMN);
		_AdrMenu_Graphical = new os::CheckMenu( "Graphical", new os::Message( MSG_MENU_VIEW_ADDRESSBAR_GRAPHICAL ) );
		_AdrMenu_Graphical->SetChecked( false );
		_AdrSubMenu->AddItem( _AdrMenu_Graphical );
		_AdrMenu_Textual = new os::CheckMenu( "Textual", new os::Message( MSG_MENU_VIEW_ADDRESSBAR_TEXTUAL ) );
		_AdrMenu_Textual->SetChecked( false );
		_AdrSubMenu->AddItem( _AdrMenu_Textual );
		_AdrSubMenu->AddItem( new os::MenuSeparator() );
		_AdrMenu_Hide = new os::CheckMenu( "Hide", new os::Message( MSG_MENU_VIEW_ADDRESSBAR_HIDE ) );
		_AdrMenu_Hide->SetChecked( true );
		_AdrSubMenu->AddItem( _AdrMenu_Hide );



		// View
		_ViewMenu = new os::Menu( os::Rect(), "View", os::ITEMS_IN_COLUMN );
		_ViewMenu_Fullscreen = new os::MenuItem( "Fullscreen mode",  new os::Message( MSG_MENU_VIEW_FULLSCREEN ) );
		_ViewMenu->AddItem( _ViewMenu_Fullscreen );
		_ViewMenu_Menu = new os::CheckMenu( "Menu", new os::Message( MSG_MENU_VIEW_MENU ), "Ctrl + M" );
		_ViewMenu->AddItem( _ViewMenu_Menu );
		_ViewMenu->AddItem( _ToolbarSubMenu );
		_ViewMenu->AddItem( _AdrSubMenu );
		_ViewMenu->AddItem( _TabSubMenu );
		
		_ViewMenu->AddItem(_ViewSideBarMenuItem = new os::CheckMenu("Sidebar",new os::Message(MSG_SHOW_SIDEBAR),Preferences::GetInstance()->GetShowSideBar()));
		_ViewMenu->AddItem(_ViewStatusBarMenuItem = new os::CheckMenu("Statusbar", new os::Message(MSG_SHOW_STATUSBAR),Preferences::GetInstance()->GetShowStatusBar()));
		_ViewMenu->AddItem(_ViewPaneMenuItem = new os::CheckMenu("Pane",new os::Message(MSG_SHOW_PANE),Preferences::GetInstance()->GetShowPane()));
		_ViewMenu->AddItem(_ShowHiddenMenuItem = new os::CheckMenu("Show Hidden Files",new os::Message(MSG_SHOW_HIDDEN),_ShowHiddenFiles));
		
	
		_MenuBar->AddItem( _ViewMenu );
	
		_LabelMenu = Labels::GetInstance()->GetMenu(MSG_MENU_LABEL);
		_LabelMenu->AddItem(new os::MenuSeparator());
		_LabelMenu->AddItem("Configure Labels...",new os::Message(MSG_MENU_CONFIGURE_LABEL));
		_LabelMenu->AddItem("Clear Label",new os::Message(MSG_MENU_CLEAR_LABEL));
		_MenuBar->AddItem(_LabelMenu);
	}

	void _SetSideBar( bool show )
	{
		if (show)
		{
			if( !_Splitter->IsView1Visible() )
				_Splitter->ShowView1();			
		}
		else
		{	
			if ( _Splitter->IsView1Visible() )
				_Splitter->HideView1();
							
		}
		
		_Splitter->AdjustLayout();
	}
	
	void _SetPane(bool show)
	{
		if (show)
		{
			if( !_PaneSplitter->IsView2Visible() )
				_PaneSplitter->ShowView2();			
		}
		else
		{	
			if ( _PaneSplitter->IsView2Visible() )
				_PaneSplitter->HideView2();
							
		}
		
		_PaneSplitter->AdjustLayout();
	}

	void _Layout( os::Rect frame )
	{
		if (_ShowStatusbar)
		{
			os::Rect r = frame;
			r.bottom = frame.Height();
			r.top = frame.Height()-20;
			_Statusbar->SetFrame( r );
			frame.bottom = r.top + 1.0f;			
		}

		else
			_Statusbar->SetFrame(os::Rect(-1,-1,-1,-1));
					
		if( _ShowMenu )
		{
			os::Rect r = frame;
			r.bottom = frame.top;
			r.bottom += _MenuBar->GetPreferredSize( false ).y;
			_MenuBar->SetFrame( r );
			frame.top = r.bottom + 1.0f;
		}
		else
			_MenuBar->SetFrame( os::Rect( -1, -1, -1, -1  ) );

		if( _ShowToolbar )
		{
			os::Rect r = frame;
			r.bottom = frame.top;
			r.bottom += _ToolBar->GetPreferredSize( false ).y;
			_ToolBar->SetFrame( r );
			frame.top = r.bottom + 1.0f;
		}
		else
			_ToolBar->SetFrame( os::Rect( -1, -1, -1, -1  ) );

		if( _ShowAddressField )
		{
			os::Rect r = frame;
			r.bottom = frame.top;
			r.bottom += _AddressField->GetPreferredSize( false ).y;
			_AddressField->SetFrame( r );
			frame.top = r.bottom + 1.0f;
		}
		else
			_AddressField->SetFrame( os::Rect( -1, -1, -1, -1  ) );
	
		_Splitter->SetFrame( frame );
		_SetSideBar(Preferences::GetInstance()->GetShowSideBar());
		_SetPane(Preferences::GetInstance()->GetShowPane());
		
		_Splitter->SetSplitLimits(85,500);			
		_PaneSplitter->SetSplitLimits(_TabView->GetBounds().Height(),20);
	}

	// Make sure that the GUI parts are up to date with
	// the curent tab and path settings
	void _UpdateGUI()
	{
		TabBase* tab = _GetCurrentTab();

		if( tab != NULL )
		{
			_UpdateWindowTitle( tab );

			_AddressField->SetPath( tab->GetPath() );

			// Set tab name
			for( int i = 0 ; i < _TabView->GetTabCount(); i++ )
			{
				if( _TabView->GetTabView( i ) == tab )
				{
					_TabView->SetTabTitle( i, tab->GetName() );
					break;
				}
			}		
		}
		
		//int view = tab->GetViewMode();

	}

	TabBase* _UpdatePath( os::String path, bool new_tab, bool force_reread, bool update_history )
	{
		TabBase* tab = _GetCurrentTab();

		// If no tab was found or we always want to create a new tab
		if( tab == NULL || new_tab )
			tab = _NewBrowserTab( path );
		else if( update_history )
			tab->MakeHistory();

		// Dont notify about the change, we are already aware of it
		tab->SetPath( path, false );
	
		if( force_reread )
			tab->ReRead();

		//_UpdateGUI();

		return tab;
	}

	void _SetAddressbarMenu( int message )
	{
		_ShowAddressField = ( message != MSG_MENU_VIEW_ADDRESSBAR_HIDE );
		_AdrMenu_Hide->SetChecked( message == MSG_MENU_VIEW_ADDRESSBAR_HIDE );
		_AdrMenu_Textual->SetChecked( message == MSG_MENU_VIEW_ADDRESSBAR_TEXTUAL );
		_AdrMenu_Graphical->SetChecked( message == MSG_MENU_VIEW_ADDRESSBAR_GRAPHICAL );
	}

	void _SetToolbarMenu( int message )
	{	
		switch( message )
		{
			case MSG_MENU_VIEW_TOOLBAR_BOTH:
				_ToolBar->SetTextPosition( os::ImageButton::IB_TEXT_BOTTOM );
				Preferences::GetInstance()->SetToolBarType( os::ImageButton::IB_TEXT_BOTTOM);
				
				_ToolbarMenu_TextAndIcon->SetRadio(true);
				_ToolbarMenu_IconsOnly->SetRadio(false);
				_ToolbarMenu_TextOnly->SetRadio(false);
				break;
			case MSG_MENU_VIEW_TOOLBAR_ICON:
				_ToolBar->SetTextPosition( os::ImageButton::IB_TEXT_NONE );
				Preferences::GetInstance()->SetToolBarType(os::ImageButton::IB_TEXT_NONE );
				
				_ToolbarMenu_TextAndIcon->SetRadio(false);
				_ToolbarMenu_IconsOnly->SetRadio(true);
				_ToolbarMenu_TextOnly->SetRadio(false);
				
				break;
			case MSG_MENU_VIEW_TOOLBAR_TEXT:
				_ToolBar->SetTextPosition( os::ImageButton::IB_TEXT_ONLY );
				Preferences::GetInstance()->SetToolBarType(os::ImageButton::IB_TEXT_ONLY);
				
				_ToolbarMenu_TextAndIcon->SetRadio(false);
				_ToolbarMenu_IconsOnly->SetRadio(false);
				_ToolbarMenu_TextOnly->SetRadio(true);				
				break;
		}
	}

	void _SetTabsMenu( int message )
	{
		_ViewMenu_TabAutomatic->SetChecked( message == MSG_MENU_VIEW_TAB_AUTOMATIC );
		_ViewMenu_TabShow->SetChecked( message == MSG_MENU_VIEW_TAB_SHOW );
		_ViewMenu_TabHide->SetChecked( message == MSG_MENU_VIEW_TAB_HIDE );

		switch( message )
		{
			case MSG_MENU_VIEW_TAB_SHOW:
				_TabView->SetTabType( os::TabView::SHOW_TABS );
				break;	
			case MSG_MENU_VIEW_TAB_HIDE:
				_TabView->SetTabType( os::TabView::HIDE_TABS );
				break;
			case MSG_MENU_VIEW_TAB_AUTOMATIC:
				_TabView->SetTabType( os::TabView::AUTOMATIC_TABS );
				break;
		}
	}



	bool _SetMenuHide()
	{
		_ShowMenu = _ViewMenu_Menu->IsChecked();
		return !_ShowMenu;
	}

	TabBase* _GetCurrentTab()
	{
		return _GetTab( _TabView->GetSelection() );
	}

	TabBase* _GetTab( int no )
	{
		View* tabview = _TabView->GetTabView( no );

		if( tabview != NULL )
		{
			// Convert it to a TabBase
			TabBase* base = dynamic_cast< TabBase* >( tabview );

			return base;
		}

		return NULL;
	}
	
	std::vector<TabBase*> _GetAllTabs()
	{
		std::vector<TabBase*> tabs;
		for (int i=0; i<_TabView->GetTabCount();i++)
		{
			View* tabview = _TabView->GetTabView( i );

			if( tabview != NULL )
			{
				// Convert it to a TabBase
				TabBase* base = dynamic_cast< TabBase* >( tabview );
				tabs.push_back(base);
			}
		}
		return tabs;
	}

	void _UpdateWindowTitle( TabBase* tab )
	{
		os::String title = "EFileBrowser";

		if( tab != NULL )
		{
			title = tab->GetTitle() + os::String(" - ") + title;
		}

		AppWindow* win = EFileBrowser::GetAppWindow();

		if( win != NULL )
			win->SetTitle( title );
	}

	void _CloseTab( os::Message* msg )
	{
		int tab_no;
                                                                                                                                                                                                  
        if( msg->FindInt32( "tab", &tab_no ) != EOK )
			return;

		TabBase* tab = _GetTab( tab_no );

		if( tab != NULL && tab->Close() )
		{
			delete _TabView->DeleteTab( tab_no );
			_UpdateGUI();
		}
	}

	TabBase* _NewBrowserTabs( os::Message* msg )
	{
		if( msg == NULL )
			return NULL;

		TabBase* first_tab = NULL;
		os::Message filemsg;
		if( msg->FindMessage( "msg", &filemsg ) == EOK )
			return _NewBrowserTabs( &filemsg );

		os::String path;
        int i = 0;
                                                                                                                                                                                                        
		while( msg->FindString( "file/path", &path, i ) == EOK )
		{
			os::FSNode fn( path );
                                                                                                                                                                                                        
			// only open if it is a directory
			if( fn.IsDir() )
			{
				TabBase* tab = _UpdatePath( path, true, true, false );
				tab->SetZoom(Preferences::GetInstance()->GetZoom());
				if( first_tab == NULL )
					first_tab = tab;
			}
            i++;
		}

		_UpdateGUI();

		return first_tab;
	}


	TabBase* _NewBrowserTab( os::String p )
	{
		TabBase* tab = new TabBrowser();
		tab->SetDirChangeMsg(new os::Message( MSG_DIR_CHANGE ));
		tab->SetFileChangeMsg(new os::Message(MSG_FILE_CHANGE));
		tab->SetShowHiddenFiles(_ShowHiddenFiles);
		tab->SetZoom(Preferences::GetInstance()->GetZoom());
		tab->SetTarget( _Parent );
		_TabView->SetSelection( _TabView->AppendTab( tab->GetName(), tab ) );	

		return tab;
	}

	View* _Parent;

	// Widgets
	os::Menu* _MenuBar;
	os::Menu* _ApplicationMenu;
	os::Menu* _ViewMenu;
	os::Menu* _LabelMenu;
	os::Menu* _LabelSelectMenu;
	
	os::MenuItem* _ViewMenu_Fullscreen;
	
	
	os::CheckMenu* _ViewMenu_Menu;
	os::CheckMenu* _ViewMenu_TabAutomatic;
	os::CheckMenu* _ViewMenu_TabShow;
	os::CheckMenu* _ViewMenu_TabHide;
	os::CheckMenu* _AdrMenu_Textual;
	os::CheckMenu* _AdrMenu_Graphical;
	os::CheckMenu* _AdrMenu_Hide;
	
	
	os::CheckMenu* _ShowHiddenMenuItem;
	os::CheckMenu* _ViewPaneMenuItem;
	os::CheckMenu* _ViewStatusBarMenuItem;
	os::CheckMenu* _ViewSideBarMenuItem;
		
	
	
	
	os::RadioMenuItem* _ToolbarMenu_TextAndIcon;
	os::RadioMenuItem* _ToolbarMenu_IconsOnly;
	os::RadioMenuItem* _ToolbarMenu_TextOnly;
	
	
	os::CheckMenu* _ToolbarMenu_Hide;


	os::ToolBar* _ToolBar;
	os::Menu* _ViewPopupMenuMenu;
	os::PopupMenu* _ViewPopupMenu; 	
	os::TabView *_TabView, *_PaneView;
	os::Splitter *_Splitter,*_PaneSplitter;
	AddressField* _AddressField;
	Status* _Statusbar;
	
	SideBar* _SideBar;
	bool _ShowMenu;
	bool _ShowToolbar;
	bool _ShowAddressField;
	bool _ShowStatusbar;
	bool _ShowSidebar;
	bool _ShowHiddenFiles;
	
	SettingsDialog* _SettingsDialog;

	std::vector<Plugin*> m_cPlugins;
	os::LayoutNode* _ZoomRestoreNode;

	enum eZoomType
	{
		ZOOM_LOWEST = 33,
		ZOOM_LOWER = 50,
		ZOOM_LOW = 66,
		ZOOM_NORMAL = 100,
		ZOOM_HIGH = 150,
		ZOOM_HIGHER = 175,
		ZOOM_HIGHEST = 200,
		ZOOM_MEGA = 400,
	};

};

///////////////////////////////////////////////////////////////////////////////
//
// T H E   C L A S S
//
///////////////////////////////////////////////////////////////////////////////
Layouter :: Layouter() : View( os::Rect(0,0,1,1), "efilebrowser_layouter_root", os::CF_FOLLOW_ALL, os::WID_WILL_DRAW )
{
	// Create the private class
	my = new _Private();
	my->_Parent = this;

	// Menu
	my->_CreateMenu();

	// Toolbar	
	my->_ToolBar = new os::ToolBar( os::Rect(), "toolbar" );
	my->_ToolBar->AddButton( "home", "Home", LoadImage( "home.png" ), new os::Message( MSG_TOOLBAR_HOME ) );
	my->_ToolBar->AddButton( "desktop", "Desktop", LoadImage( "desktop.png" ), new os::Message( MSG_TOOLBAR_DESKTOP ) );
	my->_ToolBar->AddSeparator( "" );
	my->_ToolBar->AddButton( "back", "Back", LoadImage( "back.png" ), new os::Message( MSG_TOOLBAR_BACK ) );
	my->_ToolBar->AddButton( "up", "Up", LoadImage( "up.png" ), new os::Message( MSG_TOOLBAR_UP ) );
	my->_ToolBar->AddSeparator( "" );
	my->_ToolBar->AddButton("zoom-minus","Zoom Out",LoadImage("zoom-out.png"),new os::Message(MSG_ZOOM_MINUS));
	my->_ZoomRestoreNode = my->_ToolBar->AddButton("zoom-restore",os::String().Format("%d %%",(int)(Preferences::GetInstance()->GetZoom()*100.0f)),LoadImage("zoom-original.png"),new os::Message(MSG_ZOOM_RESTORE));
	my->_ToolBar->AddButton("zoom-plus","Zoom In",LoadImage("zoom-in.png"),new os::Message(MSG_ZOOM_PLUS));
	my->_ToolBar->AddSeparator( "" );
	my->_ToolBar->SetTextPosition(Preferences::GetInstance()->GetToolBarType());
	
	/* Create popupmenu */
	my->_ViewPopupMenuMenu = new os::Menu( os::Rect(), "", os::ITEMS_IN_COLUMN );
	my->_ViewPopupMenuMenu->AddItem( new os::RadioMenuItem(ID_MSG_MENU_VIEW_ICONS, new os::Message(  MSG_VIEW_ICONS )));
	my->_ViewPopupMenuMenu->AddItem( new os::RadioMenuItem(ID_MSG_MENU_VIEW_LIST, new os::Message(  MSG_VIEW_LIST )));
	my->_ViewPopupMenuMenu->AddItem( new os::RadioMenuItem(ID_MSG_MENU_VIEW_DETAILS, new os::Message( MSG_VIEW_DETAILS )));
	
	
	os::RadioMenuItem* item = (os::RadioMenuItem*)my->_ViewPopupMenuMenu->GetItemAt(Preferences::GetInstance()->GetDefaultView());
	item->SetRadio(true);
	
	my->_ViewPopupMenuMenu->AddItem( new os::MenuSeparator() );
	my->_ViewPopupMenuMenu->AddItem( ID_MSG_MENU_VIEW_SAVE, new os::Message(  MSG_SAVE_WINDOW ) );
	my->_ViewPopupMenuMenu->AddItem( ID_MSG_MENU_VIEW_SAVE_DEFAULT, new os::Message(  MSG_SAVE_DEFAULT ) );
	



	my->_ViewPopupMenu = new os::PopupMenu(os::Rect(), "",  ID_MSG_MENU_VIEW, my->_ViewPopupMenuMenu, LoadImage("view.png"),os::CF_FOLLOW_RIGHT | os::CF_FOLLOW_RIGHT );

	my->_ToolBar->AddChild( my->_ViewPopupMenu,os::ToolBar::TB_FIXED_WIDTH );

	// Address field
	my->_AddressField = new AddressField( Preferences::GetInstance()->GetStartUpPath(), new os::Message(MSG_ADDRESS_FIELD ) );
	

	//Pane View
	my->_PaneView = new os::TabView(os::Rect(),"pane_view");
	my->_PaneView->SetTabType(os::TabView::SHOW_TABS);
	
	
	// Tab view
	my->_TabView = new os::TabView( os::Rect(), "tab_view" );
	my->_TabView->SetMessage( new os::Message( MSG_TAB_CHANGE ) );
	my->_TabView->SetDragDropMsg( new os::Message( MSG_TAB_DRAGDROP ) );
	my->_TabView->SetCloseTabMsg( new os::Message( MSG_TAB_CLOSE ) );
	my->_TabView->SetTabType( os::TabView::SHOW_TABS );
	my->_UpdatePath( getenv( "HOME" ), true, true, false );

	
	my->_PaneSplitter = new os::Splitter(os::Rect(),"pane_split",my->_TabView,my->_PaneView,VERTICAL);
	my->_PaneSplitter->SetSplitRatio(0.99);
	my->_PaneSplitter->SetMoveableByMouse(false);
	
	my->_SideBar = new SideBar(this);

	// Splitter
	my->_Splitter = new os::Splitter( os::Rect(), "split",  my->_SideBar, my->_PaneSplitter); //my->_TabView);
	my->_Splitter->SetSplitRatio(Preferences::GetInstance()->GetSplitRatio() );
	my->_Splitter->SetChangeMsg(new os::Message(MSG_SPLITTER_CHANGED));
	
	my->_Statusbar = new Status();
	
	
	
	// Attached created widgets to the window
	AddChild( my->_MenuBar );
	AddChild( my->_ToolBar );
	AddChild( my->_AddressField );
	AddChild( my->_Splitter );	
	AddChild( my->_Statusbar);

	my->_ShowMenu = true;
	my->_ShowToolbar = Preferences::GetInstance()->GetShowToolBar();
	my->_ShowAddressField = true;
	my->_ShowStatusbar = Preferences::GetInstance()->GetShowStatusBar();
	my->_ShowSidebar = Preferences::GetInstance()->GetShowSideBar();
	my->_Layout( GetBounds() );
}

Layouter :: ~Layouter()
{
	delete my;
}

void Layouter::AllAttached()
{
	View::AllAttached();
	my->_AddressField->SetTarget( this );
	my->_TabView->SetTarget( this );
	my->_ToolBar->SetTarget( this );
	my->_MenuBar->SetTargetForItems( this );
	my->_Splitter->SetTarget(this);
	my->_ViewPopupMenuMenu->SetTargetForItems( this );
	my->_LabelMenu->SetTargetForItems(this);

	
	// Update all current tabs
	for( int i = 0 ; i < my->_TabView->GetTabCount(); i++ )
	{
		TabBase* tab = dynamic_cast<TabBase*>( my->_TabView->GetTabView( i ) );
	
		if( tab != NULL )
		{
			tab->SetTarget( this );
			tab->ReRead();
		}
	}
	
	my->_UpdateGUI();
}

void Layouter::AttachedToWindow()
{	
}

void Layouter :: SetFrame( const os::Rect& cFrame, bool bNotifyServer )
{
	View::SetFrame( cFrame, bNotifyServer );
	my->_Layout( GetBounds() );
}

void Layouter :: HandleMessage( os::Message* pcMessage )
{
	switch( pcMessage->GetCode() )	//Get the message code from the message
	{
	// MENU 
		case MSG_MENU_VIEW_MENU:
			if( my->_SetMenuHide() )
			{
				// If the menu was hidden, lets show a message
			}

			my->_Layout( GetBounds() );
			break;
		case MSG_MENU_VIEW_FULLSCREEN:
		{
			bool full = EFileBrowser::GetAppWindow()->Fullscreen();
			my->_ViewMenu_Fullscreen->SetLabel( full ? "Window mode" : "Fullscreen mode" );
			break;
		}
		// ADDRESSFIELD AND MENU
		case MSG_MENU_VIEW_ADDRESSBAR_HIDE:
		case MSG_MENU_VIEW_ADDRESSBAR_TEXTUAL:
		case MSG_MENU_VIEW_ADDRESSBAR_GRAPHICAL:
			my->_SetAddressbarMenu( pcMessage->GetCode() );			
			my->_Layout( GetBounds() );
			break;
		case MSG_ADDRESS_FIELD:
		{			
			os::String p;
			if( pcMessage->FindString( "file/path", &p.str() ) == EOK )
				my->_UpdatePath( p, false, true, true );

			break;
		}

	// TABS AND TAB MENU
		case MSG_TAB_CHANGE:		
			my->_UpdateGUI();
			break;
		case MSG_TAB_DRAGDROP:
			my->_NewBrowserTabs( pcMessage );		
			break;
		case MSG_TAB_CLOSE:
			my->_CloseTab( pcMessage );
 			break;

		case MSG_MENU_VIEW_TAB_SHOW:
		case MSG_MENU_VIEW_TAB_HIDE:
		case MSG_MENU_VIEW_TAB_AUTOMATIC:
			my->_SetTabsMenu( pcMessage->GetCode() );
			break;
			
	// TOOLBAR AND TOOLBAR MENU
		case MSG_TOOLBAR_HOME:
		{
			//uint32 qual = GetQualifiers();
			my->_UpdatePath( getenv( "HOME" ), true,true,false); //qual & os::QUAL_SHIFT, true, !( qual & os::QUAL_SHIFT ) );
			break;
		}
		case MSG_TOOLBAR_DESKTOP:
		{
			//uint32 qual = GetQualifiers();
			my->_UpdatePath( os::String( getenv( "HOME" ) ) + os::String( "/Desktop" ), true,true,false); //qual & os::QUAL_SHIFT, true, !( qual & os::QUAL_SHIFT ) );		
			break;
		}
		case MSG_TOOLBAR_BACK:
		{
			TabBase* tab = my->_GetCurrentTab();
			if( tab != NULL )
			{
				os::String back = tab->Back();

				if( back != "" )
					my->_UpdatePath( back, false, true, false );
			}
			break;
		}
		case MSG_TOOLBAR_UP:
		{
			TabBase* tab = my->_GetCurrentTab();
			if( tab != NULL )
			{
				os::String up = tab->Up();

				if( up != "" )
					my->_UpdatePath( up, false, true, true );
			}
			break;
		}

		case MSG_MENU_VIEW_TOOLBAR_BOTH:
		case MSG_MENU_VIEW_TOOLBAR_ICON:
		case MSG_MENU_VIEW_TOOLBAR_TEXT:
			my->_SetToolbarMenu( pcMessage->GetCode() );			
			my->_Layout( GetBounds() );
			break;

		case MSG_MENU_VIEW_TOOLBAR_HIDE:
		{
			void* p;
			pcMessage->FindPointer("source",&p);
			os::CheckMenu* item = static_cast<os::CheckMenu*> (p);			
			my->_ShowToolbar = item->IsChecked();
			Preferences::GetInstance()->SetShowToolBar(my->_ShowToolbar);
			my->_Layout( GetBounds() );
			break;
		}
		
		case MSG_SHOW_SIDEBAR:
		{
			void* p;
			pcMessage->FindPointer("source",&p);
			os::CheckMenu* item = static_cast<os::CheckMenu*> (p);			
			my->_ShowSidebar = item->IsChecked();
			Preferences::GetInstance()->SetShowSideBar(my->_ShowSidebar);
			my->_SetSideBar(my->_ShowSidebar);
			break;
		}
		
		case MSG_CLOSE_SIDEBAR:
		{
			Preferences::GetInstance()->SetShowSideBar(false);
			my->_SetSideBar(false);
			my->_ViewSideBarMenuItem->SetChecked(false);
			break;
		}
				

	// TAB BROWSER
		case MSG_DIR_CHANGE:		
		{
			int32 button = 1;
			os::String path;

			if( pcMessage->FindString( "file/path", &path.str() ) != EOK )
				break;

			pcMessage->FindInt32( "button", &button );

			my->_UpdatePath( path, button == 3, true, button == 1 );
			
			break;
		}
		
		case MSG_FILE_CHANGE:
		{
			TabBase* tab = my->_GetCurrentTab();
			printf("icon selected count: %d\n",tab->GetSelectedCount());
			break;
		}
		
		case MSG_MENU_APP_QUIT:
		{
			EFileBrowser::GetAppWindow()->OkToQuit();
			break;
		}
		
		case MSG_SIDEBAR_SET_FILE:
		{
			void* p;
			
			pcMessage->FindPointer("browsertab",&p);
			TabBrowser* tab = static_cast<TabBrowser*> (p);
			os::String cPath = tab->GetFile();
			
			if (cPath != "")
			{
				//SideBarBase::Current()->SelectFile(cPath);
			}
			break;
		}
		
		case MSG_SPLITTER_CHANGED:
		{
			float width;
			pcMessage->FindFloat("width",&width);
			Preferences::GetInstance()->SetSplitRatio(width);
			break;
		}
		case MSG_VIEW_ICONS:
		{
			os::RadioMenuItem* item = (os::RadioMenuItem*)my->_ViewPopupMenuMenu->GetItemAt(0);
			if (!item->GetRadio())
			{
				item->SetRadio(true);
				
				for (int i=1; i<=2; i++)
				{
					os::RadioMenuItem* n = (os::RadioMenuItem*)my->_ViewPopupMenuMenu->GetItemAt(i);
					n->SetRadio(false);
				}
				my->_SetViewSelection(IconView::VIEW_ICONS);
			}
			break;
		}
		case MSG_VIEW_LIST:
		{
			os::RadioMenuItem* item = (os::RadioMenuItem*)my->_ViewPopupMenuMenu->GetItemAt(1);
			if (!item->GetRadio())
			{
				item->SetRadio(true);
				
				os::RadioMenuItem* n1 = (os::RadioMenuItem*)my->_ViewPopupMenuMenu->GetItemAt(0);
				n1->SetRadio(false);
				
				os::RadioMenuItem* n2 = (os::RadioMenuItem*)my->_ViewPopupMenuMenu->GetItemAt(2);
				n2->SetRadio(false);
				
				my->_SetViewSelection(IconView::VIEW_LIST);
			}
			break;
		}
		case MSG_VIEW_DETAILS:
		{
			os::RadioMenuItem* item = (os::RadioMenuItem*)my->_ViewPopupMenuMenu->GetItemAt(2);
			if (!item->GetRadio())
			{
				item->SetRadio(true);

				for (int i=0; i<2; i++)
				{
					os::RadioMenuItem* n = (os::RadioMenuItem*)my->_ViewPopupMenuMenu->GetItemAt(i);
					n->SetRadio(false);
				}
				my->_SetViewSelection(IconView::VIEW_DETAILS);
			}
			break;
		}
		case MSG_SAVE_WINDOW:
		{
			TabBase* tab = my->_GetCurrentTab();
			os::String path = tab->GetPath();
			
			try
			{
				os::FSNode cNode( path );
				int32 nStyle = tab->GetViewMode();
				cNode.WriteAttr( "os::WindowStyle", O_TRUNC, ATTR_TYPE_INT32, &nStyle, 0, sizeof( int32 ) );
			} 
			catch( ... )
			{
			}
			
			
			break;
		}
		case MSG_SAVE_DEFAULT:
		{
			TabBase* tab = my->_GetCurrentTab();
			IconView::view_type type = tab->GetViewMode();
			Preferences::GetInstance()->SetDefaultView(type);
			break;
		}

		case MSG_SHOW_STATUSBAR:
		{
			void* p;
			pcMessage->FindPointer("source",&p);
			os::CheckMenu* item = static_cast<os::CheckMenu*> (p);			
			my->_ShowStatusbar = item->IsChecked();
			Preferences::GetInstance()->SetShowStatusBar(my->_ShowStatusbar);
			my->_Layout( GetBounds() );
			break;
		}
		
		case MSG_SHOW_PANE:
		{
			void* p;
			pcMessage->FindPointer("source",&p);
			os::CheckMenu* item = static_cast<os::CheckMenu*> (p);			
			Preferences::GetInstance()->SetShowPane(item->IsChecked());
			my->_Layout( GetBounds() );
			break;
		}
		
		case MSG_SELECT_ALL:
		{
			TabBase* tab = my->_GetCurrentTab();
			tab->SelectAll();
			break;
		}
		
		case MSG_INVERT_SELECTION:
		{
			TabBase* tab = my->_GetCurrentTab();
			tab->InvertSelection();
			break;			
		}
		
		case MSG_SHOW_HIDDEN:
		{
			void* p;
			pcMessage->FindPointer("source",&p);
			os::CheckMenu* item = static_cast<os::CheckMenu*> (p);			
			my->_ShowHiddenFiles = item->IsChecked();
			Preferences::GetInstance()->SetShowHiddenFiles(my->_ShowHiddenFiles);
			
			std::vector<TabBase*> tabs = my->_GetAllTabs();
			
			for (uint i=0;i<tabs.size();i++)
			{
				TabBase* tab = tabs[i];
				tab->SetShowHiddenFiles(my->_ShowHiddenFiles);
			}
			break;
		}
		case MSG_MENU_LABEL:
		{
			void* p;
			pcMessage->FindPointer("source",&p);
			LabelMenuItem* item = static_cast<LabelMenuItem*> (p);
			TabBase* tab = my->_GetCurrentTab();
			tab->AddLabel(item->GetColor());
			break;
		}
		
		case MSG_MENU_CLEAR_LABEL:
		{
			TabBase* tab = my->_GetCurrentTab();
			tab->ClearLabel();
			break;
		}
		
		case MSG_MENU_CONFIGURE_LABEL:
		{
			break;
		}
		
		case MSG_SETTINGS_DIALOG:
		{
			if (my->_SettingsDialog)
			{
				if (!my->_SettingsDialog->IsVisible())
				{
					my->_SettingsDialog->Show(true);
				}
				my->_SettingsDialog->CenterInWindow(this->GetWindow());
				my->_SettingsDialog->MakeFocus();
			}
			else
			{
				my->_SettingsDialog = new SettingsDialog(this->GetWindow());
				my->_SettingsDialog->CenterInWindow(this->GetWindow());
				my->_SettingsDialog->MakeFocus();
				my->_SettingsDialog->Show();
			}
			break;
		}
		
		case MSG_CREATE_DOCUMENT_EMPTY_FILE:
		{
			printf("empty file\n");
			break;
		}
		
		case MSG_ZOOM_MINUS:
		{
			int percent = (int)(Preferences::GetInstance()->GetZoom()*100.0f);
			float vNewPercent = 0.0f;
			
			os::ImageButton* but = (os::ImageButton*)my->_ZoomRestoreNode->GetView();
			
			switch (percent)
			{
				case _Private::ZOOM_LOWEST:
				{
					//SEND A MESSAGE TO THE STATUSBAR
					 my->_Statusbar->SendMessage(Status::STATUS_ERROR,"This is the lowest you can go");
					
					break;
				}
				
				case  _Private::ZOOM_LOWER:
				{
					//You can go lower
					vNewPercent = 0.33f;
					break;
				}
				
				case  _Private::ZOOM_LOW:
				{
					vNewPercent = 0.50;
					break;
				}
				
				case  _Private::ZOOM_NORMAL:
				{
					vNewPercent = 0.66;
					break;
				}
				
				case  _Private::ZOOM_HIGH:
				{
					vNewPercent = 1.0;
					break;
				}
				
				case  _Private::ZOOM_HIGHER:
				{
					vNewPercent = 1.5;
					break;
				}
				
				case  _Private::ZOOM_HIGHEST:
				{
					vNewPercent = 1.75;
					break;
				}
				
				case  _Private::ZOOM_MEGA:
				{
					vNewPercent = 2.0;
					break;
				}
			}
			if (vNewPercent > 0.0f)
			{
				Preferences::GetInstance()->SetZoom(vNewPercent);
				int new_percent = (int)(vNewPercent*100.0f);
				os::String _UpdateString = os::String().Format("%d %%",new_percent);
				but->SetLabel(_UpdateString);
			
			
				std::vector<TabBase*> tabs = my->_GetAllTabs();
				for (int32_t i=0;i<tabs.size();i++)
					tabs[i]->SetZoom(vNewPercent);
			}
			break;		
		}
		
		case MSG_ZOOM_PLUS:
		{
			int percent = (int)(Preferences::GetInstance()->GetZoom()*100.0f);
			float vNewPercent = 0.0f;
			
			os::ImageButton* but = (os::ImageButton*)my->_ZoomRestoreNode->GetView();
			
			switch (percent)
			{
				case _Private::ZOOM_LOWEST:
				{
					vNewPercent = 0.50f;
					break;
				}
				
				case  _Private::ZOOM_LOWER:
				{
					vNewPercent = 0.66f;
					break;
				}
				
				case  _Private::ZOOM_LOW:
				{
					vNewPercent = 1.0f;
					break;
				}
				
				case  _Private::ZOOM_NORMAL:
				{
					vNewPercent = 1.5f;
					break;
				}
				
				case  _Private::ZOOM_HIGH:
				{
					vNewPercent = 1.75f;
					break;
				}
				
				case  _Private::ZOOM_HIGHER:
				{
					vNewPercent = 2.0f;
					break;
				}
				
				case  _Private::ZOOM_HIGHEST:
				{
					vNewPercent = 4.0f;
					break;
				}
				
				case  _Private::ZOOM_MEGA:
				{
					 my->_Statusbar->SendMessage(Status::STATUS_ERROR,"This is the highest you can go");
					break;
				}
			}
			
			if (vNewPercent > 0.0f)
			{
				Preferences::GetInstance()->SetZoom(vNewPercent);
				int new_percent = (int)(vNewPercent*100.0f);
				os::String _UpdateString = os::String().Format("%d %%",new_percent);
				but->SetLabel(_UpdateString);
			
			
				std::vector<TabBase*> tabs = my->_GetAllTabs();
				for (int32_t i=0;i<tabs.size();i++)
					tabs[i]->SetZoom(vNewPercent);
			}
			break;
		}
		
		
		case MSG_BROWSER_OPEN_TAB:
		{
			TabBase* tab = my->_GetCurrentTab();
			os::String path = tab->GetFile();
			my->_UpdatePath(path,true,true,true);
			break;
		}
		
		case MSG_BROWSER_OPEN_WINDOW:
		{
			TabBase* tab = my->_GetCurrentTab();
			os::String path = tab->GetFile();
			os::String app_path = GetApplicationPath() + "/EFileBrowser";
			
			if (fork() == 0)
			{
				execl(app_path.c_str(),path.c_str(),NULL);
			}
			
			break;
		}
		
		case SIDEBAR:
		{
			void* pSidebar=NULL;
			printf("we did get here\n");
			break;
		}
		
		/*case TOOLBAR:
		{
			void* pToolBar=NULL;

			if (pcMessage->FindPointer("toolbar",&pToolBar) == 0)
			{
				os::ToolBar* pcToolBar = static_cast<os::ToolBar*>(pToolBar);
				SetToolBar(pcToolBar);
			}
			break;
		}*/

		/*case VIEW:
		{
			void* pView=NULL;

			if (pcMessage->FindPointer("view",&pView) == 0)
			{
				os::View* pcNewView = static_cast<os::View*>(pView);
				SetView(pcNewView);
			}
			break;
		}*/

		/*case ADDON:
		{
			void* pItem=NULL;
			os::String cPluginName;
			if (pcMessage->FindPointer("menuitem",&pItem) == 0)
			{
				os::Message* pcMenuClicked;
				pcMessage->FindString("plugin_name",&cPluginName);
				os::MenuItem* pcItem = static_cast<os::MenuItem*>(pItem);

				if (pcItem)
				{
					pcMenuClicked = pcItem->GetMessage();
					pcMenuClicked->AddInt32("code",pcMenuClicked->GetCode());
					pcMenuClicked->AddString("addon",cPluginName);
					pcMenuClicked->SetCode(M_ADDON_CLICKED);
					pcItem->SetTarget(this);
					AddAddon(pcItem,cPluginName);
				}
			}
			break;
		}*/

		/*case M_ADDON_CLICKED:
		{
			int32 nCode;
			String cAddon;
			pcMessage->FindInt32("code",&nCode);
			pcMessage->FindString("addon",&cAddon);
			pcMessage->SetCode(nCode);

			for (uint i=0; i<m_pcPlugins.size(); i++)
			{
				if (m_pcPlugins[i]->GetIdentifier() == cAddon)
				{
					printf("setting target\n");
					PostMessage(pcMessage,m_pcPlugins[i]);
				}
			}
			break;
		}*/	
	// DEFAULT
		default:
			View::HandleMessage( pcMessage );	
	}
}

void Layouter::AddPlugin(Plugin* plugin)
{
	for (uint i=0; i<my->m_cPlugins.size(); i++)
	{
		if(my->m_cPlugins[i]->GetIdentifier() == plugin->GetIdentifier())
		{	
			return;
		}
	}

	plugin->SetApp(Application::GetInstance());
	//pcPlugin->SetCWD(GetCWD());	
	plugin->Initialize();
	my->m_cPlugins.push_back(plugin);
}











