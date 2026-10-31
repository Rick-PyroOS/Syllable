#ifndef __PROVIDER__H__
#define __PROVIDER__H__

#include <cerrno>
#include <typeinfo>
#include <util/string.h>
#include <util/variant.h>
#include <util/settings.h>
#include <util/handler.h>
#include <storage/path.h>
#include <unistd.h>

#include <vector>
#include <storage/registrar.h>
#include <util/looper.h>

#include "result.h"
#include "../compat_traits.h"

namespace assistant{

/*
 * A provider is a plugin that produces search results.
 *
 * Threading: the host adds every provider to the ProviderThread looper
 * before Initialize() is called, and every method below is called on that
 * looper's thread. Node monitors and timers aimed at the provider are
 * delivered there too, so a provider never needs a lock as long as it does
 * not start threads of its own. Never block on the Assistant window or
 * application from a provider: they may be waiting on this thread.
 */
class Provider : public os::Handler
{
public:
	Provider( const os::String cName ) : Handler( cName ),m_pcApp(NULL),m_nPlugin(-1) {};
	virtual ~Provider() {};
public:
	image_id GetPluginId()
	{
		return( m_nPlugin );
	}
	os::Path GetPath()
	{
		char zPath[PATH_MAX];

		/* Get path of the plugin */
		int nFd = open_image_file( GetPluginId() );
		if( nFd < 0 )
			return( os::Path() );

		get_directory_path( nFd, zPath, PATH_MAX );
		close( nFd );
		return( os::Path( zPath ) );
	}
	void SetApp( os::Looper* pcApp, image_id nPlugin )
	{
		m_pcApp = pcApp;
		m_nPlugin = nPlugin;
	}
	os::Looper* GetApp()
	{
		return( m_pcApp );
	}
private:
	os::Looper* m_pcApp;
	image_id	m_nPlugin;

public:

	virtual status_t Initialize() = 0;
	virtual std::vector<Result> Process( const os::Variant & ) = 0;
	virtual status_t Launch( const Result & ) = 0;
	/* os::Settings has no safe copy constructor, so hand out a reference */
	virtual const os::Settings& Settings() const = 0;
	virtual status_t Watch() = 0;
	virtual status_t RebuildCache() = 0;
	/* Builds the view on the provider thread; the caller attaches it to a window */
	virtual os::View *SettingsView() = 0;
	virtual void WriteCache() = 0;
};

typedef Provider* init_provider_plugin( os::Path cPluginFile );
}
#endif
