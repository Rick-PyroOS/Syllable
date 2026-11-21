/*  LRUCache - os::LRUCache, a cache that forgets what was used longest ago.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of version 2 of the GNU Library
 *  General Public License as published by the Free Software
 *  Foundation.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 */

#ifndef	__F_UTIL_LRUCACHE_H__
#define	__F_UTIL_LRUCACHE_H__

#include <util/hashmap.h>
#include <vector>
#include <algorithm>
#include <stddef.h>

namespace os
{

namespace lrucache_private
{
/* A cached value and what it costs */
template<class V> struct Slot
{
	Slot() : m_cValue(), m_nCost( 0 ) {}	// m_cValue(): 0 for numbers and pointers
	V m_cValue;
	size_t m_nCost;
	bool operator==( const Slot& cOther ) const { return m_nCost == cOther.m_nCost && m_cValue == cOther.m_cValue; }
};
} // namespace lrucache_private

/** A cache of keys to values that, when full, forgets the least recently
 *  used.
 * \ingroup util
 * \par Description:
 *	os::LRUCache keeps values that are slow to make (icons scaled from
 *	files, glyphs from a font, thumbnails) up to a capacity.  Each value
 *	has a cost, 1 unless you say otherwise, and the capacity is the most
 *	the values may cost together: give the cost in bytes and the
 *	capacity is a memory budget, or leave it at 1 and the capacity is a
 *	number of entries.  When a new value would go over it, the values
 *	used longest ago go first, each handed to an eviction function if
 *	you set one (to free a bitmap, say).
 *
 *	Get() and Find() count as a use and make the key the newest; Peek()
 *	and Has() look without using.  Everything is O(1) on average.
 *
 *	It is an os::HashMap kept in the order of use: oldest first, so the
 *	next to go is at the front and a use moves its key to the back.  (A
 *	HashMap keeps its own order, so it needs no list beside it.)
 * \code
 *	static void FreeIcon( const os::String& cPath, os::Bitmap*& pcIcon, void* )
 *	{
 *	    delete pcIcon;
 *	}
 *
 *	os::LRUCache<os::String, os::Bitmap*> cIcons( 4 * 1024 * 1024 );	// 4 MB
 *	cIcons.SetEvictHook( FreeIcon );
 *
 *	os::Bitmap* pcIcon = cIcons.Get( cPath, NULL );
 *	if( pcIcon == NULL )
 *	{
 *	    pcIcon = LoadIcon( cPath );
 *	    cIcons.Set( cPath, pcIcon, pcIcon->GetBytesPerRow() * pcIcon->GetBounds().Height() );
 *	}
 * \endcode
 *
 *	Set() adds or replaces a value (the old one is not handed to the
 *	eviction function: you are replacing it) and makes it the newest.  A
 *	value that costs more than the whole capacity is not kept (Has()
 *	tells).  Remove(), Take() and PopOldest() take a key out
 *	without the eviction function; Clear(), SetCapacity() and the
 *	destructor call it for what they drop.  A cache can't be copied (the
 *	eviction function would see each value twice), only Swap()ped.  GetHits() and GetMisses() count how Get() and Find() did, to
 *	tune the capacity.
 *
 *	Adding a key, or using one, can move the others in memory, so a
 *	pointer from Find() or Peek() is good until the next Set(), Get() or
 *	Find().  K needs operator== and an os::Hash (or a hash class H), and
 *	both K and V a default constructor.  os::LRUCache is not thread-safe:
 *	guard it with an os::Locker to share it between threads.
 */
template<class K, class V, class H = Hash<K> >
class LRUCache
{
public:
	/** Called with each key and value the cache drops to make room, and
	 *  the pUser given to SetEvictHook() */
	typedef void (*EvictHook)( const K& cKey, V& cValue, void* pUser );
	typedef K key_type;
	typedef V mapped_type;
	typedef size_t size_type;

	/** A cache for values costing up to nCapacity together */
	explicit LRUCache( size_t nCapacity, const H& cHash = H() )
		: m_cMap( cHash ), m_nCapacity( nCapacity ), m_nCost( 0 ), m_pfEvict( NULL ), m_pEvictUser( NULL ), m_nHits( 0 ), m_nMisses( 0 ) {}
	/** Drops every entry through the eviction function, as Clear() */
	~LRUCache() { Clear(); }

	/** Puts the value of a key in, as the newest, replacing the one there
	 *  was; then drops the oldest until it is within its capacity.  A value
	 *  that costs more than the capacity is not kept (and the key's old
	 *  value is gone too): Has() says whether it was.
	 * \return the cache, to chain Set()s. */
	LRUCache& Set( const K& cKey, const V& cValue, size_t nCost = 1 )
	{
		Slot* pcOld = m_cMap.Find( cKey );
		if( pcOld != NULL )
		{
			m_nCost -= pcOld->m_nCost;
			m_cMap.Remove( cKey );
		}
		if( nCost > m_nCapacity )
			return *this;
		MakeRoom( m_nCapacity - nCost );
		Slot cSlot;
		cSlot.m_cValue = cValue;
		cSlot.m_nCost = nCost;
		m_cMap.Push( cKey, cSlot );
		m_nCost += nCost;
		return *this;
	}

	/** The value of a key, made the newest, or cDefault (a miss) */
	V Get( const K& cKey, const V& cDefault = V() )
	{
		V* pcValue = Find( cKey );
		return pcValue != NULL ? *pcValue : cDefault;
	}

	/** The value of a key, made the newest, to read or change in place; or
	 *  NULL (a miss) */
	V* Find( const K& cKey )
	{
		Slot* pcSlot = m_cMap.Find( cKey );
		if( pcSlot == NULL )
		{
			m_nMisses++;
			return NULL;
		}
		m_nHits++;
		return &Touch( cKey, pcSlot )->m_cValue;
	}

	/** The value of a key without using it (its place stays), or NULL */
	const V* Peek( const K& cKey ) const
	{
		const Slot* pcSlot = m_cMap.Find( cKey );
		return pcSlot != NULL ? &pcSlot->m_cValue : NULL;
	}
	/** Whether a key is in, without using it */
	bool Has( const K& cKey ) const { return m_cMap.Has( cKey ); }
	/** What a key's value costs, or 0 if it's not in */
	size_t GetCostOf( const K& cKey ) const
	{
		const Slot* pcSlot = m_cMap.Find( cKey );
		return pcSlot != NULL ? pcSlot->m_nCost : 0;
	}

	/** Takes a key out, without the eviction function.
	 * \return false if it was not in. */
	bool Remove( const K& cKey ) { return Take( cKey, NULL ); }
	/** Takes a key out, giving its value to pcValue (when not NULL),
	 *  without the eviction function.  \return false if it was not in. */
	bool Take( const K& cKey, V* pcValue )
	{
		Slot cSlot;
		if( !m_cMap.Take( cKey, &cSlot ) )
			return false;
		m_nCost -= cSlot.m_nCost;
		if( pcValue != NULL )
			*pcValue = cSlot.m_cValue;
		return true;
	}

	/** Takes the oldest entry out (the next to be dropped), without the
	 *  eviction function.  \return false if it is empty. */
	bool PopOldest( K* pcKey = NULL, V* pcValue = NULL )
	{
		K cKey;
		Slot cSlot;
		if( !m_cMap.PopFront( &cKey, &cSlot ) )
			return false;
		m_nCost -= cSlot.m_nCost;
		if( pcKey != NULL )
			*pcKey = cKey;
		if( pcValue != NULL )
			*pcValue = cSlot.m_cValue;
		return true;
	}

	/** Drops every entry, calling the eviction function for each */
	void Clear()
	{
		K cKey;
		Slot cSlot;
		while( m_cMap.PopFront( &cKey, &cSlot ) )
		{
			if( m_pfEvict != NULL )
				m_pfEvict( cKey, cSlot.m_cValue, m_pEvictUser );
		}
		m_cMap.Clear();
		m_nCost = 0;
	}

	/** Changes the capacity, dropping the oldest (through the eviction
	 *  function) until it is within it */
	void SetCapacity( size_t nCapacity )
	{
		m_nCapacity = nCapacity;
		MakeRoom( nCapacity );
	}
	size_t GetCapacity() const { return m_nCapacity; }
	/** What the values in it cost together */
	size_t GetCost() const { return m_nCost; }

	/** Sets the function called for what the cache drops (NULL for none) */
	void SetEvictHook( EvictHook pfEvict, void* pUser = NULL )
	{
		m_pfEvict = pfEvict;
		m_pEvictUser = pUser;
	}

	/** How often Get() and Find() found the key, and didn't */
	int GetHits() const { return m_nHits; }
	int GetMisses() const { return m_nMisses; }
	void ResetStats() { m_nHits = m_nMisses = 0; }

	/** The keys, from the oldest used to the newest */
	std::vector<K> Keys() const { return m_cMap.Keys(); }
	/** Calls cFunc( key, value ) for each entry, oldest first, without
	 *  using them */
	template<class F> void ForEach( F cFunc ) const
	{
		for( typename Map::const_iterator i = m_cMap.begin(); i != m_cMap.end(); ++i )
			cFunc( i->first, i->second.m_cValue );
	}

	int Count() const { return m_cMap.Count(); }
	bool IsEmpty() const { return m_cMap.IsEmpty(); }
	void Swap( LRUCache& cOther )
	{
		m_cMap.Swap( cOther.m_cMap );
		std::swap( m_nCapacity, cOther.m_nCapacity );
		std::swap( m_nCost, cOther.m_nCost );
		std::swap( m_pfEvict, cOther.m_pfEvict );
		std::swap( m_pEvictUser, cOther.m_pEvictUser );
		std::swap( m_nHits, cOther.m_nHits );
		std::swap( m_nMisses, cOther.m_nMisses );
	}

	// The C++ library's names, for templates
	size_type size() const { return m_cMap.size(); }
	bool empty() const { return m_cMap.empty(); }

private:
	typedef lrucache_private::Slot<V> Slot;
	typedef HashMap<K, Slot, H> Map;

	/* Moves a key to the back, as the newest, without copying its value
	 * (it is swapped out and back in).  \return where it is now. */
	Slot* Touch( const K& cKey, Slot* pcSlot )
	{
		Slot cMoving;
		std::swap( cMoving.m_cValue, pcSlot->m_cValue );
		cMoving.m_nCost = pcSlot->m_nCost;
		m_cMap.Push( cKey, Slot() );
		Slot* pcNew = m_cMap.Find( cKey );
		std::swap( pcNew->m_cValue, cMoving.m_cValue );
		pcNew->m_nCost = cMoving.m_nCost;
		return pcNew;
	}

	/* Drops the oldest, through the eviction function, until the values
	 * cost no more than nCost */
	void MakeRoom( size_t nCost )
	{
		while( m_nCost > nCost )
		{
			K cKey;
			Slot cSlot;
			if( !m_cMap.PopFront( &cKey, &cSlot ) )
				break;
			m_nCost -= cSlot.m_nCost;
			if( m_pfEvict != NULL )
				m_pfEvict( cKey, cSlot.m_cValue, m_pEvictUser );
		}
	}

	// not copied: the eviction function would see each value twice
	LRUCache( const LRUCache& );
	LRUCache& operator=( const LRUCache& );

	Map m_cMap;		// oldest used first
	size_t m_nCapacity;
	size_t m_nCost;
	EvictHook m_pfEvict;
	void* m_pEvictUser;
	int m_nHits;
	int m_nMisses;
};

} // namespace os

#endif // __F_UTIL_LRUCACHE_H__
