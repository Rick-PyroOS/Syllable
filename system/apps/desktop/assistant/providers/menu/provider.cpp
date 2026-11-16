#include "provider.h"
#include "storage/registrar.h"
#include <vector>
#include <gui/desktop.h>

class StubManager
{
public:
	os::Window* GetActiveWindow( bool bIgnoreSystemWindows ) const
	{
		os::Window* window = new os::Window();
		return window;
	}
};

class StubWindow
{
};

MenuProvider::MenuProvider() : Provider()
{
	m_pcManager = os::RegistrarManager::Get();
	os::Window* window = m_pcManager->GetActiveWindow();
	std::vector<os::Action> actions = window->GetActions();

	this->isInCache(window->GetApplication(),window->GetName();
}


std::vector<Result> MenuProvider::Process( const os::String& )
{
	return std::vector<Result>();
}

status_t MenuProvider::Launch( const Result& result )
{
	return 0;
}
