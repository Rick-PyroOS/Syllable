#ifndef __PROVIDER_THREAD__H__
#define __PROVIDER_THREAD__H__

#include "util/looper.h"
#include "providers/provider.h"

using namespace assistant;

/*
 * Owns every provider plugin. It is a Looper, so it has its own thread, and
 * that thread is the only one that ever touches the plugin list or calls
 * into a plugin. Other threads talk to it only with messages (see
 * messages.h), so there are no locks here.
 *
 * Lock rule: the window and the application may wait on this looper
 * (M_SHUTDOWN_PROVIDERS is sent synchronously), so this looper must never
 * wait on them. It only posts replies.
 */
class ProviderThread : public os::Looper
{
public:
	ProviderThread();
	~ProviderThread();
public:
	static ProviderThread* GetInstance();
	void HandleMessage( os::Message* pcMessage );
private:
	class Private;
	Private* m;
private:
	static ProviderThread* s_pcInstance;
private:
	ProviderThread( const ProviderThread& );
	ProviderThread& operator=( const ProviderThread& );

	status_t _LoadPlugins();
	void _UnloadPlugins();
	bool _Disabled( const os::String& );
	void _Query( os::Message* );
	void _Launch( os::Message* );
	Provider* _FindProvider( const os::String& ) const;
};

#endif
