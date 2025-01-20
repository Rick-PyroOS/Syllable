#include "application.h"
#include "mainwindow.h"
#include "util/random.h"

App::App() : os::Application( "application/x-vnd.WidgetGallery" )
{

	m_cAlignments.insert(std::make_pair("Left",os::ALIGN_LEFT));
	m_cAlignments.insert(std::make_pair("Right",os::ALIGN_RIGHT));
	m_cAlignments.insert(std::make_pair("Center",os::ALIGN_CENTER));
	m_cAlignments.insert(std::make_pair("Top",os::ALIGN_TOP));
	m_cAlignments.insert(std::make_pair("Bottom",os::ALIGN_BOTTOM));

	m_cStates.insert(std::make_pair("Normal",os::NORMAL));
	m_cStates.insert(std::make_pair("Disabled",os::DISABLED));
	m_cStates.insert(std::make_pair("Hovered",os::HOVERED));
	m_cStates.insert(std::make_pair("Active",os::ACTIVE));

	m_pcMainWindow = new MainWindow();
	m_pcMainWindow->CenterInScreen();
	m_pcMainWindow->Show();
	m_pcMainWindow->MakeFocus();
}

std::map<os::String,uint32> App::GetStates() const
{
	return m_cStates;
}

uint32 App::GetState(const os::String& state) const {
	std::map<os::String,uint32> states = GetStates();

	for (std::map<os::String, uint32>::const_iterator it = states.begin(); it != states.end(); ++it){
		if (state == it->first){
			return it->second;
		}
	}
	return -1;
}

os::String App::GetState(uint32 state) const {
	std::map<os::String,uint32> states = GetStates();

	for (std::map<os::String, uint32>::const_iterator it = states.begin(); it != states.end(); ++it){
		if (state == it->second){
			return it->first;
		}
	}
	return "Unknown";
}





std::map<os::String,uint32> App::GetAlignments() const
{
	return m_cAlignments;
}


uint32 App::GetAlignment(const os::String& alignment) const {
	std::map<os::String,uint32> alignments = GetAlignments();

	for (std::map<os::String, uint32>::const_iterator it = alignments.begin(); it != alignments.end(); ++it){
		if (alignment == it->first){
			return it->second;
		}
	}
	return -1;
}

os::String App::GetAlignment(uint32 alignment) const {
	std::map<os::String,uint32> alignments = GetAlignments();

	for (std::map<os::String, uint32>::const_iterator it = alignments.begin(); it != alignments.end(); ++it){
		if (alignment == it->second){
			return it->first;
		}
	}
	return "Unknown";
}



