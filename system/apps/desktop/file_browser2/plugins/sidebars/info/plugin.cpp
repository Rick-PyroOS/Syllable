#ifndef __SIDEBAR_INFO_H
#define __SIDEBAR_INFO_H

#include <efilebrowser/plugin.h>
#include "sidebar_info.h"

class SideBarInfoPlugin : public Plugin
{
public:
	os::String GetIdentifer()
	{
		return "SideBarInfo";
	}
	
	os::Path GetPath()
	{
	}
	
	void Initialize()
	{
	}	
};

extern "c"
{
	Plugin* init_file_browser_plugin(os::Path cPath)
	{
		return (new SideBarInfoPlugin(cPath));
	}
};
