#ifndef FILE_PROVIDER_H
#define FILE_PROVIDER_H

#include "../provider.h"
#include "storage/nodemonitor.h"
#include "syllable/inttypes.h"
#include "util/handler.h"
#include "util/locker.h"
#include "util/variant.h"
#include <vector>

using namespace assistant;

class FileProvider : public Provider
{
public:
	FileProvider();
	virtual ~FileProvider();

	std::vector<Result> Process( const os::Variant & );
	status_t Initialize();
	status_t Launch( const Result &result );
	os::Settings Settings() const;
	status_t Watch();
	status_t RebuildCache();
	status_t SaveSettings();
	os::View *SettingsView();
	void WriteCache();
public:
	enum messages {
		M_FUZZY_CHANGED,
		M_APPLICATION_VISIBILITY_TOGGLED,
		M_APPLICATION_PATH_ADDED,
		M_APPLICATION_PATH_REMOVED
	};
private:
	status_t WatchPath( const os::String );
	void HandleMessage( os::Message *pcMessage );

private:
	os::RegistrarManager *m_pcManager;
	os::NodeMonitor* m_pcNodeMonitor;
	std::vector<Result> m_cCache;
	os::Settings m_cSettings;

	int m_nFuzzyScore;
	os::Locker m_cLock;
};

#endif

