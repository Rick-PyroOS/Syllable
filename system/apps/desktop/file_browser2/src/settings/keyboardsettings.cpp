#include "keyboardsettings.h"
#include "../resources/lang.h"
#include "../messages.h"
#include "../shortcuts.h"

using namespace os;

/*this macro just finds the item in the drop down menu and selects it*/
#define FIND_ITEM(drop,item) { \
	for(int i=0; i<drop->GetItemCount(); i++) \
		if (item == drop->GetItem(i)) { \
			drop->SetSelection(i); \
		} \
}

/*this constructs a shortcut from two strings*/
os::ShortcutKey ConstructKeyShortcut(os::String cKey1,os::String cKey2)
{
	os::String cShort = cKey1 + os::String("+") + cKey2;
	os::ShortcutKey cKey(cShort);
	return cKey;
}

/*this loads string1 and string2 with their appropriate string values from the shortcut*/
void LoadKeyShortcut(const os::ShortcutKey& cShortcut,os::String* cKey1, os::String* cKey2)
{
	if (cShortcut.GetQualifiers() == QUAL_CTRL)
		*cKey1 = "CTRL";
	else if (cShortcut.GetQualifiers() == QUAL_ALT)
		*cKey1 = "ALT";
	else
		*cKey1 = "SHIFT";
		
	cKey2->Format("%c",cShortcut.GetKeyCode());
}

os::String RemoveSpecialChars(const os::String& cString)
{
	//cString.Replace("_", " ");
	return cString;
}

os::String UpcaseWords(const os::String& cString)
{
}

KeysDrop::KeysDrop(const os::String& cName) : os::DropdownMenu(os::Rect(),cName)
{
	AppendItem("ALT");
	AppendItem("CTRL");
	AppendItem("SHIFT");
	
	SetMinPreferredSize(2);
	SetMaxPreferredSize(2);
	
	SetReadOnly(true);
	SetSelection(0);
}

LettersDrop::LettersDrop(const os::String& cName) : os::DropdownMenu(os::Rect(),cName)
{
	for (int i=65; i<91; i++)
	{
		os::String cString;
		cString.Format("%c",i);
		AppendItem(cString.c_str());
	}
	
	SetMinPreferredSize(1);
	SetMaxPreferredSize(1);
	SetReadOnly(true);
	SetSelection(0);
}

ShortcutLayout::ShortcutLayout(const os::String& cName) : HLayoutNode(cName)
{
	AddChild(new StringView(os::Rect(),"shortcut_layout_pk",cName));
	AddChild(m_pcString = new StringView(os::Rect(),"shortcut_layout_string",cName));
	AddChild(new HLayoutSpacer("",10,10));
	AddChild(m_pcKeysDrop1 = new KeysDrop("drop1"));
	AddChild(new HLayoutSpacer("",2,2));
	AddChild(m_pcLettersDrop = new LettersDrop("drop3")); 
	SameWidth("drop1","drop3",NULL);
}

os::ShortcutKey ShortcutLayout::GetShortcut()
{
	return ConstructKeyShortcut(m_pcKeysDrop1->GetCurrentString(),m_pcLettersDrop->GetCurrentString());
}


void ShortcutLayout::LoadKeys(const os::ShortcutKey& cKey)
{
	os::String cKeyString1,cKeyString2;
	LoadKeyShortcut(cKey,&cKeyString1,&cKeyString2);
	
	FIND_ITEM(m_pcKeysDrop1,cKeyString1);
	FIND_ITEM(m_pcLettersDrop,cKeyString2);
}

KeyboardSettings::KeyboardSettings() : SettingsNode("Keyboard Settings")
{
	m_cShortcuts = Shortcuts::GetInstance()->GetShortcuts();
	m_pcIcon = LoadImage("keyboard.png");
	m_pcIcon->SetSize(os::Point(32,32));
	m_nCount = 0;
	pcView = new os::LayoutView(GetBounds(),"root");
	AddChild(pcView);
	
	m_cNodes.push_back(new os::VLayoutNode("node0"));
	
	tShortcuts::iterator i;
	for (i=m_cShortcuts.begin();i!=m_cShortcuts.end(); ++i)
	{
		ShortcutLayout* layout = new ShortcutLayout(i->first);
		layout->LoadKeys(i->second);
		m_cLayouts.push_back(layout);
	}
	
	int nList=0;
	
	for (uint i=0; i<m_cLayouts.size(); i++)
	{
		if ( ((i%6) != 0) || (i==0))
		{
			m_cNodes[nList]->AddChild(m_cLayouts[i]);
			m_cNodes[nList]->AddChild(new os::VLayoutSpacer("",5,5));
		}
		else
		{
			nList++;
			os::String cString;
			cString.Format("node%d",nList);
			
			m_cNodes.push_back(new os::VLayoutNode(cString));	
			m_cNodes[nList]->AddChild(m_cLayouts[i]);
			m_cNodes[nList]->AddChild(new os::VLayoutSpacer("",5,5));
		}
	}
	
	pcButtonNode = new HLayoutNode("button");
	pcButtonNode->AddChild(pcPrevBut = new os::Button(os::Rect(),"prev_but",ID_MSG_BUTTON_PREV,new os::Message(MSG_BUTTON_PREV)));
	pcButtonNode->AddChild(new os::HLayoutSpacer("",5,5));
	pcButtonNode->AddChild(pcNextBut = new os::Button(os::Rect(),"next_but",ID_MSG_BUTTON_NEXT,new os::Message(MSG_BUTTON_NEXT)));
	pcButtonNode->SameWidth("prev_but","next_but",NULL);
	pcButtonNode->SameHeight("prev_but","next_but",NULL);
	
	SameWidth("node0","node1",NULL);
	SameHeight("node0","node1",NULL);
}

void KeyboardSettings::LayoutSystem()
{
	pcView->SetRoot(NULL);
	os::VLayoutNode* pcRoot = new os::VLayoutNode("root");
	pcRoot->AddChild(m_cNodes[m_nCount]);
	pcRoot->AddChild(new VLayoutSpacer("spacer",5,5));
	pcRoot->AddChild(pcButtonNode);
	pcView->SetRoot(pcRoot);
}

void KeyboardSettings::Next()
{
	if (m_nCount < m_cNodes.size()-1)
	{
		m_nCount +=1;
		LayoutSystem();
		
		if (m_nCount == m_cNodes.size()-1)
			pcNextBut->SetEnable(false);
			
		pcPrevBut->SetEnable(true);
	}
}

void KeyboardSettings::Prev()
{
	if (m_nCount > 0)
	{
		m_nCount -=1;
		
		if (m_nCount == 0)
			pcPrevBut->SetEnable(false);
		pcNextBut->SetEnable(true);
		
		LayoutSystem();
	}
}

void KeyboardSettings::LoadSettings()
{
	
}

void KeyboardSettings::SaveSettings()
{
	Shortcuts::GetInstance()->SaveSettings();
}

void KeyboardSettings::SetTabs()
{
}

















