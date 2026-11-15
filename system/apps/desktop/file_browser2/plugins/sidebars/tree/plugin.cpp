#ifndef __SIDEBAR_INFO_H
#define __SIDEBAR_INFO_H

#include <efilebrowser/plugin.h>
#include "sidebar_tree.h"

class SideBarTreePlugin : public Plugin
{
public:
	os::String GetIdentifer()
	{
		return "SideBarTree";
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
		return (new SideBarTreePlugin(cPath));
	}
};

