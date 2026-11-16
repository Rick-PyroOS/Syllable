#ifndef _SETTINGS_DIALOG_H
#define _SETTINGS_DIALOG_H

#include <gui/layoutview.h>

#include <gui/dropdownmenu.h>
#include <gui/textview.h>
#include <gui/stringview.h>
#include <gui/button.h>
#include <gui/window.h>
#include <gui/view.h>
#include <gui/image.h>
#include <util/string.h>
#include <algorithm>
#include <list>

#include "settingsnode.h"
#include "../shortcuts.h"
#include "../syll_gui/iconview.h"

using namespace os;


class SettingsDialog : public os::Window
{
	public:
		SettingsDialog(os::Window* pcProjectWindow);

		virtual bool OkToQuit( void );
		virtual void HandleMessage( os::Message* pcMessage );
		void SetTabs();
	private:
		os::LayoutView*		m_pcView, *pcSettingsView;
		os::HLayoutNode*	m_pcHLayout;
		
		
		IconView*		m_pcIconView;
		
		os::Button*			m_pcOk;
		os::Button*			m_pcCancel;
		
		os::Window*			m_pcProjectWindow;
		
		std::vector<SettingsNode*> m_cSettingNodes;
		tShortcuts m_cShortcuts;
};

#endif
















