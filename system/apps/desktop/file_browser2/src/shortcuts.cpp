#include "shortcuts.h"

Shortcuts* Shortcuts::m_theInstance = NULL;

Shortcuts* Shortcuts :: GetInstance()
{
	if( m_theInstance == NULL )
	{
		m_theInstance = new Shortcuts();
	}

	return m_theInstance;
}

Shortcuts::Shortcuts()
{
	LoadSettings();
}

void Shortcuts::LoadSettings()
{
	m_Shortcuts.insert(std::map<os::String,os::String>::value_type("quit","CTRL+Q"));
	m_Shortcuts.insert(std::map<os::String,os::String>::value_type("cut","CTRL+X"));
	m_Shortcuts.insert(std::map<os::String,os::String>::value_type("copy","CTRL+C"));
	m_Shortcuts.insert(std::map<os::String,os::String>::value_type("paste","CTRL+P"));
	m_Shortcuts.insert(std::map<os::String,os::String>::value_type("select_all","CTRL+A"));
	m_Shortcuts.insert(std::map<os::String,os::String>::value_type("invert_selection","CTRL+I"));
	m_Shortcuts.insert(std::map<os::String,os::String>::value_type("configure_labels","CTRL+L"));
	m_Shortcuts.insert(std::map<os::String,os::String>::value_type("fullscreen","F11"));
//	m_Shortcuts.insert(std::map<os::String,os::String>::value_type("paste","CTRL+P"));
//	m_Shortcuts.insert(std::map<os::String,os::String>::value_type("paste","CTRL+P"));
}

void Shortcuts::SaveSettings()
{
}

tShortcuts Shortcuts::GetShortcuts()
{
	return m_Shortcuts;
}


os::String Shortcuts::operator[] (const char* string)
{
	return m_Shortcuts[string];
}










