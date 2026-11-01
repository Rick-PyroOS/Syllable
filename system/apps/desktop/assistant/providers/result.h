#ifndef __REUSLT_H__
#define __REUSLT_H__

#include <util/string.h>
#include <gui/image.h>
#include <map>
#include <tr1/memory>

namespace assistant
{
/*
 * Results are copied from the provider thread to the window thread, so the
 * icon is reference counted: the last copy to go away deletes it, on
 * whichever thread that happens.
 */
typedef std::tr1::shared_ptr<os::Image> IconPtr;

class Result
{
public:
	//TODO: use a GCC primative to see if you can get the caller name instead of passing it.
	Result() : m_vScore( 0.0f )
	{
	}

	/* Takes ownership of icon */
	Result(
		os::String provider,
		os::String name,
		os::String description,
		os::Image *icon = NULL,
		float score = 0.0f ) : m_cProvider(provider), m_cName(name), m_cDescription(description),m_pcIcon(icon),m_cMetaData(),m_vScore(score)
	{
	}

	Result(
		os::String provider,
		os::String name,
		os::String description,
		const IconPtr& icon,
		float score = 0.0f ) : m_cProvider(provider), m_cName(name), m_cDescription(description),m_pcIcon(icon),m_cMetaData(),m_vScore(score)
	{
	}

	const os::String provider() const
	{
		return m_cProvider;
	}

	const os::String name() const
	{
		return m_cName;
	}

	const os::String description() const
	{
		return m_cDescription;
	}

	const std::map<os::String, os::String> &metaData() const
	{
		return m_cMetaData;
	}

	/* Returns "" when the key is missing */
	os::String metaData( const os::String& key ) const
	{
		std::map<os::String, os::String>::const_iterator i = m_cMetaData.find( key );
		return ( i == m_cMetaData.end() ) ? os::String() : i->second;
	}

	void addMetaData( std::pair<os::String, os::String> d )
	{
		m_cMetaData.insert( d );
	}

	os::Image *icon() const
	{
		return m_pcIcon.get();
	}

	const IconPtr& iconPtr() const
	{
		return m_pcIcon;
	}

	void setScore( float score )
	{
		m_vScore = score;
	}

	const float score() const
	{
		return m_vScore;
	}

public:
	const bool Valid() const
	{
		return ( m_cName != "" && m_cProvider != "" );
	}

private:
	os::String m_cProvider;
	os::String m_cName;
	os::String m_cDescription;
	IconPtr m_pcIcon;
	std::map<os::String, os::String> m_cMetaData;
	float m_vScore;
};

/* Highest score first, for std::stable_sort */
inline bool ResultScoreGreater( const Result& a, const Result& b )
{
	return a.score() > b.score();
}
}

#endif
