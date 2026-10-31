#ifndef APPLICATION_PROVIDER_H
#define APPLICATION_PROVIDER_H

#include "../provider.h"
#include "storage/nodemonitor.h"
#include "syllable/inttypes.h"
#include "util/handler.h"
#include "util/variant.h"
#include <vector>

using namespace assistant;

/*
 * Every method runs on the ProviderThread looper (see ../provider.h), so
 * nothing here is locked.
 */
class ApplicationProvider : public assistant::Provider
{
public:
	ApplicationProvider();
	~ApplicationProvider();
	std::vector<Result> Process( const os::Variant & );

	status_t Initialize();
	status_t Launch( const Result &result );

	const os::Settings& Settings() const;
	status_t Watch();
	status_t RebuildCache();
	status_t SaveSettings();
	os::View *SettingsView();
	void WriteCache();
public:
	enum messages {
		M_FUZZY_CHANGED = 3000,
		M_APPLICATION_VISIBILITY_TOGGLED,
		M_APPLICATION_PATH_ADDED,
		M_APPLICATION_PATH_REMOVED
	};
private:
	void LoadApplicationDirectories();
	status_t WatchPath( const os::String );
	void HandleMessage( os::Message *pcMessage );
	void TimerTick( int nID );
	std::vector<os::String> GetApplications(const os::String& cPath);

private:
	os::RegistrarManager *m_pcManager;
	std::vector<Result> m_cCache;
	os::Settings m_cSettings;
	std::vector<os::String> m_cPaths;
	int m_nFuzzyScore;


	std::vector<os::NodeMonitor*> m_cNodeMonitors;
};

#endif
