#ifndef _EFILE_BROWSER_PLUGIN_H
#define _EFILE_BROWSER_PLUGIN_H

#include <util/handler.h>
#include <util/string.h>
#include <storage/path.h>

#include <gui/menu.h>

#include <util/looper.h>
#include "sidebar_plugin.h"
enum Menus
{
	MAIN_MENU = 0 ,
	APP_MENU,
	EDIT_MENU,
	VIEW_MENU,
	HELP_MENU
};


enum Items
{
	TOOLBAR=60000,
	MENU,
	SIDEBAR,
	PANE,
	ITEMS_COUNT,
};

/** 
 * \par Description:
 * FileBrowser Application Plugin class... 
 * \author	Rick Caudill ( syllable_desktop@hotpop.com )
 *****************************************************************************/
class Plugin : public os::Handler
{
public:
	Plugin() : Handler("EPlugin")
	{
	};

	virtual ~Plugin()
	{
	};

	/**
 	 * \par Description:
 	 *	Overload this to set the name of your plugin
 	 * \author	Rick Caudill 
 	 *****************************************************************************/
	virtual os::String GetIdentifier()=0;
	
	/**
 	 * \par Description:
 	 *	Initialize is where you add all the details for your plugin.
 	 * \author	Rick Caudill 
 	 *****************************************************************************/
	virtual void Initialize()=0;
	
	/**
 	 * \par Description:
 	 *	Overload GetPath to get current path of plugin file.
 	 * \author	Rick Caudill
 	 *****************************************************************************/	
	virtual os::Path GetPath() = 0;
	
	/**
 	 * \par Description:
 	 *	Set the application looper here.
 	 * \author	Rick Caudill
 	 *****************************************************************************/
	void SetApp(os::Looper* pcApp)
	{
		m_pcApp = pcApp; 
	};

	/**
 	 * \par Description:
 	 *	Adds a toolbar.  Sends it via a TOOLBAR message to Application/Window looper
 	 *  To see it as a toolbar do something like the following in you application/window looper:
 	 *	case TOOLBAR:
	 *	{
	 *		void* pToolBar=NULL;
	 *
	 *		if (pcMessage->FindPointer("toolbar",&pToolBar) == 0)
	 *		{
	 *			printf("found toolbar\n");
	 *			os::ToolBar* pcToolBar = static_cast<os::ToolBar*>(pToolBar);
	 *			
	 *			//now you can add it to the window
	 *		}
	 *		break;
	 *	}	
 	 * \author	Rick Caudill
 	 *****************************************************************************/
	/*void AddToolBar(os::ToolBar* pcToolBar)
	{
		os::Message cMessage(TOOLBAR);
		cMessage.AddPointer("toolbar",pcToolBar);
		m_pcApp->PostMessage(&cMessage,m_pcApp);
	};*/

	/**
 	 * \par Description:
 	 *	Adds a menu.  Sends it via a MENU message to Application/Window looper
 	 *  To see it as a menu do something like the following in you application/window looper:
 	 *	case MENU:
	 *	{
	 *		void* pMenu=NULL;
	 *
	 *		if (pcMessage->FindPointer("menu",&pMenu) == 0)
	 *		{		
	 *			printf("found menu\n");
	 *			os::Menu* pcMenu = static_cast<os::Menu*>(pMenu);
	 *			
	 *			//now get the position
	 *			pcMessage->FindInt32("position",&nMenu)
	 *
	 *			//now add it to the window or whatever
	 *		}
	 *		break;
	 *	}	
 	 * \author	Rick Caudill
 	 *****************************************************************************/
	void AddMenu(os::Menu* pcMenu,Menus nMenu)
	{
		os::Message cMessage(MENU);
		cMessage.AddPointer("menu",pcMenu);
		cMessage.AddInt32("position",nMenu);
		m_pcApp->PostMessage(&cMessage,m_pcApp);
	};

	/**
 	 * \par Description:
 	 *	Adds a menuitem.  Sends it via a MENU message to Application/Window looper
 	 *  To see it as a menuitem do something like the following in you application/window looper:
 	 *	case MENU:
	 *	{
	 *		void* pMenuItem=NULL;
	 *
	 *		if (pcMessage->FindPointer("menu",&pMenuItem) == 0)
	 *		{		
	 *			printf("found menuitem\n");
	 *			os::MenuItem* pcItem = static_cast<os::MenuItem*>(pMenuItem);
	 *			
	 *			//now get the position
	 *			pcMessage->FindInt32("position",&nMenu)
	 *
	 *			//now add it to the window or whatever
	 *		}
	 *		break;
	 *	}	
 	 * \author	Rick Caudill
 	 *****************************************************************************/
	void AddMenuItem(os::MenuItem* pcItem, Menus nMenu)
	{
		os::Message cMessage(MENU);
		cMessage.AddPointer("menuitem",pcItem);
		cMessage.AddInt32("position",nMenu);
		m_pcApp->PostMessage(&cMessage,m_pcApp);		
	};
	
	
	void AddSideBar(SideBarPlugin* sidebar)
	{
		os::Message cMessage(SIDEBAR);
		cMessage.AddPointer("sidebar",sidebar);
		m_pcApp->PostMessage(&cMessage,m_pcApp);
	};	
private:
	os::Looper* m_pcApp;
};

#endif








