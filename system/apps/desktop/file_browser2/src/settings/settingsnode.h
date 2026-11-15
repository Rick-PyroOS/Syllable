#ifndef _SETTINGS_NODE_H
#define _SETTINGS_NODE_H

#include <gui/layoutview.h>
#include <gui/image.h>
#include <util/string.h>

class SettingsNode : public os::VLayoutNode
{
public:
	SettingsNode(const os::String& cName) : VLayoutNode(cName)
	{
	}
	
	virtual void 					LayoutSystem()=0;
	virtual void 					LoadSettings()=0;
	virtual void 					SaveSettings()=0;
	virtual void 					SetTabs()=0;

	virtual os::BitmapImage* 		GetSettingsIcon()=0;
	virtual	os::String				GetSettingsName()=0;


private:
};
#endif

