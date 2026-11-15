#include <gui/view.h>
#include <util/string.h>

#ifndef __SIDEBAR_PLUGIN_H__
#define __SIDEBAR_PLUGIN_H__

class SideBarPlugin : public os::View
{
public:
	SideBarPlugin(const os::Rect& cRect,const os::String title) : os::View(cRect,title){};
	~SideBarPlugin();

	virtual void SetPath( os::String path ) {};
	virtual void SelectFile( os::String path ) {};

	virtual os::String GetIdentifer()=0;
private:
};

#endif


