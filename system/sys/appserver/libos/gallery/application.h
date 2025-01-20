#ifndef APPLICATION_H
#define APPLICATION_H

#include <util/application.h>
#include <gui/window.h>

#include <vector>
#include <map>
#include "./defines.h"

class App : public os::Application
{
public:
	App();
public:
	uint32 GetState(const os::String& state) const;
	os::String GetState(uint32 state) const;
	uint32 GetAlignment(const os::String& alignment) const;
	os::String GetAlignment(uint32 alignment) const;

	std::map<os::String,uint32> GetAlignments() const;
	std::map<os::String,uint32> GetStates() const;
private:
	os::Window* m_pcMainWindow;
	std::map<os::String,uint32> m_cAlignments;
	std::map<os::String,uint32> m_cStates;
};

#endif
