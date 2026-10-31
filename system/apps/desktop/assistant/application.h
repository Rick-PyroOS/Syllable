#ifndef APPLICATION_H
#define APPLICATION_H

#include <util/application.h>
#include <gui/keyevent.h>
#include <util/message.h>
#include <util/event.h>

#include "settings.h"
#include "mainwindow.h"
#include "provider_thread.h"
#include "common.h"

class App : public os::Application
{
public:
	App();
	~App();
	
	virtual void HandleMessage( os::Message* );
	void Show();
	void Started();
	bool OkToQuit();
	
private:
	MainWindow* m_pcMainWindow;
	os::ShortcutKey m_cShortcutKey;
	os::KeyboardEvent m_cKeyBoardEvent;
	os::Event* m_pcEvent;
	ProviderThread* m_pcProviderThread;
	SettingsHandler* m_pcSettings;
};

#endif





