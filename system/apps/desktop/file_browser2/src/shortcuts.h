#ifndef __SHORT_CUTS_H__
#define __SHORT_CUTS_H__

#include <util/message.h>
#include <util/string.h>
#include <util/shortcutkey.h>
#include <map>

typedef std::map<os::String,os::String> tShortcuts;

class Shortcuts
{
public:
	Shortcuts();
	
	void LoadSettings();
	void SaveSettings();
	
	static Shortcuts* GetInstance();	
	tShortcuts GetShortcuts();
	os::String operator[] (const char*);	
private:
	tShortcuts m_Shortcuts;
	static Shortcuts* m_theInstance;	
};

#endif







