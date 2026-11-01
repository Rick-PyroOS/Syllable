#ifndef __SETTINGS__H__
#define __SETTINGS__H__

#include "storage/nodemonitor.h"
#include "util/message.h"
#include <util/handler.h>
#include <util/settings.h>

/*
 * App settings (~/Settings/assistant/Settings). Lives on the App looper:
 * call get() only from the App thread. When the file changes it reloads its
 * own copy and tells the ProviderThread, which keeps a copy of its own, so
 * no Settings object is ever shared between threads.
 */
class SettingsHandler : public os::Handler
{
public:
	SettingsHandler();
	~SettingsHandler();
	static SettingsHandler* GetInstance() {
		return s_mpcInstance;
	}
	const os::Settings& get() const;
	/* Call after the handler has been added to a looper */
	status_t Watch();
private:
	void HandleMessage(os::Message*);
private:
	os::Settings m_cSettings;
	void _WatchFile();
	os::NodeMonitor* m_pcWatchMonitor;
	os::NodeMonitor* m_pcFileMonitor;
	static SettingsHandler* s_mpcInstance;
};


#endif
