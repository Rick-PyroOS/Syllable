#ifndef APPLICATION_H
#define APPLICATION_H

#include <util/application.h>
#include <gui/window.h>


using namespace os;

class App : public Application
{
public:
	App();
private:
	Window* m_pcMainWindow;
};

#endif

