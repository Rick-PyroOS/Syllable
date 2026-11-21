#include "./dockplugin.h"
#include "gui/image.h"
#include "storage/registrar.h"

#include <gui/view.h>
#include <gui/imagebutton.h>
#include <util/map.h>


class DockWindowIcon{
public:
	DockWindowIcon();
private:
	DockWindowIcon* SetApplication(const os::String&);
	DockWindowIcon* SetLabel(const os::String&);
	DockWindowIcon* SetImage(const os::BitmapImage*);
};

class DockWindow {
public:
	DockWindow();

	void Draw(os::View*);
	void AddWindow(const DockWindowIcon&);
	void RemoveWindow(const os::String& cLabel);

private:
	std::vector<DockWindowIcon> m_cIcons;
};

class DockWindows : public os::View{
private:
	os::RegistrarManager* m_pcManager;
	os::Map<os::String,DockWindows> m_cIcons;
};

DockWindows::DockWindows(){
	m_pcManager = os::RegistrarManager::Get();
	m_cIcons = os::Map<os::String, DockWindowIcon> ();
}

void DockWindows::_Init(){
	m_pcManager->GetAppList();

	for (

}


