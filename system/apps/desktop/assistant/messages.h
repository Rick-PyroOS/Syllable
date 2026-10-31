#ifndef MESSAGES_H
#define MESSAGES_H

/* Application and window messages */
enum
{
	M_APP_QUIT = 1000,
	M_KEY_RECEIVED,
	M_CHANGED,
	M_CYCLE,
	M_ENTER,
};

/*
 * Messages handled by the ProviderThread looper. Every call into a provider
 * happens on that one thread, so providers need no locks of their own.
 *
 * M_LOAD_PROVIDERS      load every plugin in /system/extensions/assistant
 * M_QUERY               "input" (string), "generation" (int32); replied to with M_RESULTS
 * M_RESULTS             "generation" (int32), "results" (pointer to a heap
 *                       std::vector<Result>, the receiver deletes it)
 * M_LAUNCH              "provider", "name", "description" (strings),
 *                       "meta_key"/"meta_value" (repeated strings)
 * M_SETTINGS_CHANGED    the app settings file changed, reload it
 * M_SHUTDOWN_PROVIDERS  write caches and unload every plugin; replied to when done
 */
enum
{
	M_LOAD_PROVIDERS = 2000,
	M_QUERY,
	M_RESULTS,
	M_LAUNCH,
	M_SETTINGS_CHANGED,
	M_SHUTDOWN_PROVIDERS,
};

#endif
