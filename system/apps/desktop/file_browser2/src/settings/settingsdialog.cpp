
#include "settingsdialog.h"
#include "../common.h"
#include "../messages.h"
#include "keyboardsettings.h"


#include <util/settings.h>
#include <util/shortcutkey.h>
#include <stdio.h>
#include <stdarg.h>

using namespace os;



SettingsDialog::SettingsDialog(os::Window* pcProjectWindow) : Window(os::Rect(0,0,500,300),"side_settings_window","Global Settings...",os::WND_NOT_RESIZABLE)
{
	m_pcView = new os::LayoutView(GetBounds(),"settings_view",NULL,os::CF_FOLLOW_ALL);
	
	m_pcProjectWindow = pcProjectWindow;

	m_cSettingNodes.push_back(new KeyboardSettings());
	
	for (uint i=0; i<m_cSettingNodes.size(); i++)
	{
		m_cSettingNodes[i]->LayoutSystem();
		m_cSettingNodes[i]->LoadSettings();
	}
	os::VLayoutNode* m_pcRoot = new os::VLayoutNode("root");
	m_pcRoot->SetVAlignment(os::ALIGN_RIGHT);
	m_pcRoot->SetHAlignment(os::ALIGN_RIGHT);
	
	m_pcHLayout = new os::HLayoutNode("major");
	m_pcHLayout->SetBorders(os::Rect(5,5,5,5));	
	m_pcHLayout->SetVAlignment(os::ALIGN_LEFT);
	m_pcHLayout->SetHAlignment(os::ALIGN_LEFT);
	
	m_pcIconView = new IconView(os::Rect(),"",os::CF_FOLLOW_NONE);
	
	for (uint i=0; i<m_cSettingNodes.size(); i++)
		m_pcIconView->AddIconString( m_pcIconView->AddIcon(m_cSettingNodes[i]->GetSettingsIcon(),new IconData()),m_cSettingNodes[i]->GetSettingsName());
	
	m_pcIconView->SetView(IconView::VIEW_ICONS);
	m_pcIconView->Layout();
	
	m_pcIconView->SetSelChangeMsg(new os::Message(MSG_SETTINGS_VIEW_CHANGED));
	m_pcHLayout->AddChild(m_pcIconView);
	m_pcHLayout->AddChild(new os::HLayoutSpacer("",15,15));
	pcSettingsView = new os::LayoutView(GetBounds(),"settings_view",NULL,os::CF_FOLLOW_ALL);
	pcSettingsView->SetRoot(m_cSettingNodes[0]);
	m_pcHLayout->AddChild(pcSettingsView);

	/*turn cancel and ok around to form cancel/ok*/
	/*add an export button*/
	os::HLayoutNode* pcButtonLayout = new os::HLayoutNode("buttons");
	pcButtonLayout->SetVAlignment(os::ALIGN_RIGHT);
	pcButtonLayout->SetHAlignment(os::ALIGN_RIGHT);
	pcButtonLayout->AddChild(m_pcCancel = new os::Button(os::Rect(),"Cancel_BUT","_Cancel",new os::Message(MSG_BUTTON_CANCEL)));
	pcButtonLayout->AddChild(new os::HLayoutSpacer("",5,5));
	pcButtonLayout->AddChild(m_pcOk = new os::Button(os::Rect(),"Save_BUT","_Save",new os::Message(MSG_BUTTON_OK)));
	pcButtonLayout->AddChild(new os::HLayoutSpacer("",5,5));
	pcButtonLayout->AddChild(new os::HLayoutSpacer("",5,5));
	pcButtonLayout->SameWidth("Save_BUT","Cancel_BUT",NULL);
	pcButtonLayout->SameHeight("Save_BUT","Cancel_BUT",NULL);
	pcButtonLayout->LimitMaxSize(os::Point(pcButtonLayout->GetPreferredSize(false).x+48,pcButtonLayout->GetPreferredSize(false).y));

	m_pcRoot->AddChild(m_pcHLayout);
	m_pcRoot->AddChild(new os::VLayoutSpacer("",5,5));
	m_pcRoot->AddChild(pcButtonLayout);
	m_pcRoot->AddChild(new os::VLayoutSpacer("",5,5));	
	m_pcView->SetRoot(m_pcRoot);
	
	AddChild(m_pcView);
	ResizeTo(os::Point(m_pcView->GetPreferredSize(false).x+114,m_pcView->GetPreferredSize(false).y));

	m_pcIconView->SetIconSelected(0,true);
}

bool SettingsDialog::OkToQuit()
{
	m_pcProjectWindow->PostMessage( new os::Message( MSG_SETTINGS_CLOSED ), m_pcProjectWindow );
	Close();
	return true;
}

void SettingsDialog::HandleMessage(os::Message* pcMessage)
{
	switch (pcMessage->GetCode())
	{
		case MSG_BUTTON_OK:
		{
			for (uint i=0; i<m_cSettingNodes.size(); i++)
			{
				m_cSettingNodes[i]->SaveSettings();
			}
			OkToQuit();
			break;
		}
		
		case MSG_BUTTON_CANCEL:
		{
			OkToQuit();
			break;
		}
		
		case MSG_BUTTON_NEXT:
		{
			KeyboardSettings* pcSettings = (KeyboardSettings*)m_cSettingNodes[0];
			pcSettings->Next();
			break;
		}
		
		case MSG_BUTTON_PREV:
		{
			KeyboardSettings* pcSettings = (KeyboardSettings*)m_cSettingNodes[0];
			pcSettings->Prev();
			break;
		}
	}
}














