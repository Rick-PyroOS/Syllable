#ifndef __KEYBOARD_SETTINGS_H__
#define __KEYBOARD_SETTINGS_H__

#include <gui/dropdownmenu.h>
#include <gui/layoutview.h>
#include <gui/image.h>
#include <util/settings.h>
#include <gui/stringview.h>
#include <gui/button.h>

#include "../shortcuts.h"
#include "settingsnode.h"
#include "../common.h"

using namespace os;

class KeysDrop : public os::DropdownMenu
{
public:
	KeysDrop(const os::String&);
};

class LettersDrop : public os::DropdownMenu
{
public:
	LettersDrop(const os::String&);
};

class ShortcutLayout : public os::HLayoutNode
{
public:
	ShortcutLayout(const os::String&);
	
	os::ShortcutKey GetShortcut();
	void LoadKeys(const os::ShortcutKey&);
private:
	os::StringView* m_pcString;
	KeysDrop *m_pcKeysDrop1; 
	LettersDrop* m_pcLettersDrop;
};

class KeyboardSettings : public SettingsNode
{
public:
	KeyboardSettings();
	
	void LayoutSystem();
	void LoadSettings();
	void SaveSettings();
	void SetTabs();
	void Next();
	void Prev();
	
	os::BitmapImage* GetSettingsIcon(){return m_pcIcon;}
	os::String GetSettingsName() { return "Shortcuts"; }
private:
	os::Settings* pcSettings;
	
	os::Button *pcPrevBut,*pcNextBut;
	os::HLayoutNode* pcButtonNode;
	
	os::LayoutView* pcView;
	
	os::BitmapImage* m_pcIcon;
	
	tShortcuts m_cShortcuts;
	
	uint m_nCount;
	std::vector<ShortcutLayout*> m_cLayouts;
	std::vector<os::VLayoutNode*> m_cNodes;
};


#endif



