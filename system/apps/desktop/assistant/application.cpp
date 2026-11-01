#include "application.h"
#include "mainwindow.h"
#include "messages.h"
#include "provider_thread.h"

#include <util/shortcutkey.h>
#include <util/messenger.h>

#define APP_ID "application/x-vnd.rgc-assistant"

App::App() : os::Application( APP_ID ), m_pcEvent( NULL )
{
	SetPublic(true);
	SetDefaultHandler(this);
	UnregisterKeyEvent(APP_ID);
	m_pcSettings = new SettingsHandler();
	AddHandler(m_pcSettings);
	m_pcMainWindow = new MainWindow();
	m_pcProviderThread = new ProviderThread();
}


App::~App(){
	UnregisterKeyEvent(APP_ID);
	delete m_pcEvent;
	RemoveHandler(m_pcSettings);
	delete m_pcSettings;
}

void App::Started(){
	m_cShortcutKey = os::ShortcutKey(0x5e, os::QUAL_CTRL);
	m_cKeyBoardEvent = os::KeyboardEvent(m_cShortcutKey,APP_ID);
	os::Application::GetInstance()->RegisterKeyEvent(m_cKeyBoardEvent);
	SetDefaultHandler(this);
	
	m_pcEvent = new os::Event();
	m_pcEvent->SetToRemote(APP_ID,-1);
	m_pcEvent->SetMonitorEnabled(true,this,M_KEY_RECEIVED);

	m_pcSettings->Watch();

	/* Starts the provider thread, which then loads the plugins on its own thread */
	m_pcProviderThread->Run();
	m_pcProviderThread->PostMessage( M_LOAD_PROVIDERS, m_pcProviderThread );
}

bool App::OkToQuit(){
	if( !os::Application::OkToQuit() ){
		return false;
	}
	if( m_pcProviderThread != NULL ){
		/* Wait for the plugins to write their caches. Safe: the provider thread never waits on us */
		os::Message cReply;
		os::Messenger( m_pcProviderThread, m_pcProviderThread ).SendMessage( M_SHUTDOWN_PROVIDERS, &cReply );
		/* The looper deletes itself when it quits */
		m_pcProviderThread->PostMessage( os::M_QUIT );
		m_pcProviderThread = NULL;
	}
	return true;
}


void App::HandleMessage(os::Message* pcMessage){	

	switch (pcMessage->GetCode()){
		case M_KEY_RECEIVED:{
			os::String cApp;
			if (pcMessage->FindString("id", &cApp) == 0){
			}
			this->Show();
			break;
		}
		default:
			printf("%d\n",pcMessage->GetCode());
			Application::HandleMessage(pcMessage);
			break;
	}
}

void App::Show(){
	if (!m_pcMainWindow->IsVisible()){
		m_pcMainWindow->CenterInScreen();
		m_pcMainWindow->Show();
		m_pcMainWindow->SetFocus();
	}
	else{
		m_pcMainWindow->CenterInScreen();
		m_pcMainWindow->SetFocus();
	}
}

