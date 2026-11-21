/*  HashMap - a hash table that keeps the order of its entries.
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

#ifndef	__F_UTIL_HASHMAP_H__
#define	__F_UTIL_HASHMAP_H__

#include <util/hash.h>
#include <vector>
#include <utility>
#include <algorithm>
#include <iterator>
#include <stddef.h>

namespace os
{

/** An entry of an os::HashMap: its key (first) and value (second).
 *  Don't change first through an iterator: the table finds it by it. */
template<class K, class V> struct HashMapEntry
{
	HashMapEntry() : m_nHash( 0 ), m_bLive( false ) {}
	HashMapEntry( const K& cKey, const V& cValue, size_t nHash ) : first( cKey ), second( cValue ), m_nHash( nHash ), m_bLive( true ) {}

	K first;
	V second;
	size_t m_nHash;		// os::Hash of first
	bool m_bLive;		// false: removed, waiting to be compacted away
};

/** The iterators of os::HashMap (E is a HashMapEntry, or a const one) */
template<class E> class HashMapIterator
{
public:
	typedef std::bidirectional_iterator_tag iterator_category;
	typedef E value_type;
	typedef ptrdiff_t difference_type;
	typedef E* pointer;
	typedef E& reference;

	HashMapIterator() : m_pBegin( NULL ), m_pPos( NULL ), m_pEnd( NULL ) {}
	HashMapIterator( E* pBegin, E* pPos, E* pEnd ) : m_pBegin( pBegin ), m_pPos( pPos ), m_pEnd( pEnd ) { SkipForward(); }
	/* An iterator converts to a const_iterator */
	template<class E2> HashMapIterator( const HashMapIterator<E2>& cOther ) : m_pBegin( cOther.m_pBegin ), m_pPos( cOther.m_pPos ), m_pEnd( cOther.m_pEnd ) {}

	E& operator*() const { return *m_pPos; }
	E* operator->() const { return m_pPos; }
	HashMapIterator& operator++() { ++m_pPos; SkipForward(); return *this; }
	HashMapIterator operator++( int ) { HashMapIterator cOld = *this; ++*this; return cOld; }
	HashMapIterator& operator--()
	{
		do
			--m_pPos;
		while( m_pPos != m_pBegin && !m_pPos->m_bLive );
		return *this;
	}
	HashMapIterator operator--( int ) { HashMapIterator cOld = *this; --*this; return cOld; }
	template<class E2> bool operator==( const HashMapIterator<E2>& cOther ) const { return m_pPos == cOther.m_pPos; }
	template<class E2> bool operator!=( const HashMapIterator<E2>& cOther ) const { return m_pPos != cOther.m_pPos; }

	E* m_pBegin;
	E* m_pPos;
	E* m_pEnd;
private:
	void SkipForward()
	{
		while( m_pPos != m_pEnd && !m_pPos->m_bLive )
			++m_pPos;
	}
};

/** A hash table of keys to values that keeps the order of its entries.
 * \ingroup util
 * \par Description:
 *	os::HashMap finds, adds and removes a key in about the same time
 *	however big it is, as os::UnorderedMap does, and keeps its entries
 *	in an order you choose, as a list does: the order they were added in
 *	unless you move them with Push(), PushFront(), InsertAt(), Reverse(),
 *	Sort() or SortByValue().  Iterating goes through them in that order.
 *
 *	It is a table of its own, built like Python's dict: the entries are
 *	in an array in their order, and an open-addressing index of powers of
 *	two (with triangular probing) points into it.  Each entry keeps its
 *	hash, so a lookup compares keys only when the hashes are equal, and
 *	growing or sorting never hashes a key again.  Removing an entry leaves
 *	a hole in the array, which is compacted away once the holes outnumber
 *	the entries.
 * \code
 *	os::HashMap<os::String, int> cScores;
 *	cScores.Set( "kurt", 3 ).Set( "anna", 9 ).Set( "rick", 5 );	// kurt, anna, rick
 *	cScores.Set( "kurt", 4 );		// update: kurt stays first
 *	cScores.Push( "kurt", 7 );		// push: kurt goes last (anna, rick, kurt)
 *	cScores.PushFront( "zoe", 1 );		// zoe, anna, rick, kurt
 *	cScores.Sort();				// anna, kurt, rick, zoe
 *	cScores.SortByValue( NULL, true );	// anna 9, kurt 7, rick 5, zoe 1
 *	cScores.Reverse();			// zoe, rick, kurt, anna
 *	os::String cName;  int nScore;
 *	cScores.Pop( &cName, &nScore );	// anna 9
 *	for( os::HashMap<os::String, int>::const_iterator i = cScores.begin(); i != cScores.end(); ++i )
 *	    printf( "%s %d\n", i->first.c_str(), i->second );
 * \endcode
 *
 *	A map of vectors has Append() and RemoveAt() for the items of a key
 *	(os::Map and os::UnorderedMap have them too):
 * \code
 *	os::HashMap<os::String, std::vector<os::JSON> > cByUser;
 *	cByUser.Append( "rick", cEvent ).Append( "rick", cOther );	// the vector is made for the first
 *	cByUser.RemoveAt( "rick", 0 );			// cOther is left
 * \endcode
 *
 *	Set() adds a key at the end, or changes its value where it is.
 *	Push() puts the key at the end and PushFront() at the start, moving
 *	it there if it's in.  Pop() and PopFront() take the last and first
 *	entries out.  Finding, Set(), Push(), Remove(), Pop() and PopFront()
 *	are O(1) on average; PushFront(), InsertAt(), Reverse() and the sorts
 *	are O(n) (the sorts O(n log n)), since they rebuild the index.  At()
 *	and IndexOf() are O(1) while nothing was removed since the last
 *	compaction, O(n) otherwise.  Adding an entry can move the others in
 *	memory, so it invalidates iterators and the pointers Find() gives.
 *
 *	K needs operator== and an os::Hash (or a hash class H), and both K
 *	and V a default constructor (a removed entry's key and value are set
 *	to K() and V(), so they let go of what they hold at once).
 */
template<class K, class V, class H = Hash<K> >
class HashMap
{
public:
	typedef bool (*KeyCompare)( const K&, const K& );
	typedef bool (*ValueCompare)( const V&, const V& );
	typedef HashMapEntry<K, V> Entry;
	typedef K key_type;
	typedef V mapped_type;
	typedef Entry value_type;
	typedef HashMapIterator<Entry> iterator;
	typedef HashMapIterator<const Entry> const_iterator;
	typedef std::reverse_iterator<iterator> reverse_iterator;
	typedef std::reverse_iterator<const_iterator> const_reverse_iterator;
	typedef size_t size_type;
	typedef std::pair<K, V> Item;

	HashMap() : m_nCount( 0 ), m_nHoles( 0 ), m_nDeleted( 0 ), m_nHead( 0 ) {}
	explicit HashMap( const H& cHash ) : m_cHash( cHash ), m_nCount( 0 ), m_nHoles( 0 ), m_nDeleted( 0 ), m_nHead( 0 ) {}

	/** Adds the key with the value at the end, if it's not in yet.
	 * \return true if it was added, false if the key was in (its value
	 *	stays). */
	bool Insert( const K& cKey, const V& cValue ) { return Add( cKey, cValue, false ).second; }

	/** Changes the value of a key that is in, where it is.
	 * \return false, changing nothing, if the key is not in. */
	bool Update( const K& cKey, const V& cValue )
	{
		V* pcValue = Find( cKey );
		if( pcValue == NULL )
			return false;
		*pcValue = cValue;
		return true;
	}

	/** Adds the key at the end, or changes its value where it is.
	 * \return the map, to chain Set()s. */
	HashMap& Set( const K& cKey, const V& cValue )
	{
		Add( cKey, cValue, true );
		return *this;
	}

	/** Puts the key with the value at the end, moving it there if it's in */
	HashMap& Push( const K& cKey, const V& cValue )
	{
		size_t nHash = m_cHash( cKey );
		int nSlot = Probe( cKey, nHash, NULL );
		if( nSlot >= 0 )
		{
			size_t nEntry = size_t( m_cIndex[nSlot] );
			if( nEntry + 1 == m_cEntries.size() )
			{
				m_cEntries[nEntry].second = cValue;	// last already
				return *this;
			}
			Kill( nSlot, true );	// tidy, or Push()es alone would pile up holes
		}
		Add( cKey, cValue, true );
		return *this;
	}

	/** Puts the key with the value at the start, moving it there if it's
	 *  in.  O(n). */
	HashMap& PushFront( const K& cKey, const V& cValue ) { return InsertAt( 0, cKey, cValue ); }

	/** Puts the key with the value at position nPos (0 is the start; past
	 *  the end is the end), moving it there if it's in.  O(n). */
	HashMap& InsertAt( int nPos, const K& cKey, const V& cValue )
	{
		size_t nHash = m_cHash( cKey );
		int nSlot = Probe( cKey, nHash, NULL );
		if( nSlot >= 0 )
			Kill( nSlot, false );
		Compact();
		size_t nAt = nPos < 0 ? 0 : std::min( size_t( nPos ), m_cEntries.size() );
		m_cEntries.insert( m_cEntries.begin() + nAt, Entry( cKey, cValue, nHash ) );
		m_nCount++;
		Reindex( m_cEntries.size() );
		return *this;
	}

	/** The value of a key, added with V() at the end when it's not in */
	V& operator[]( const K& cKey ) { return m_cEntries[Add( cKey, V(), false ).first].second; }

	/** The value of a key, or cDefault when it's not in */
	V Get( const K& cKey, const V& cDefault = V() ) const
	{
		const V* pcValue = Find( cKey );
		return pcValue != NULL ? *pcValue : cDefault;
	}

	/** The value of a key, to read or change in place, or NULL */
	V* Find( const K& cKey )
	{
		int nEntry = FindEntry( cKey );
		return nEntry >= 0 ? &m_cEntries[nEntry].second : NULL;
	}
	const V* Find( const K& cKey ) const
	{
		int nEntry = FindEntry( cKey );
		return nEntry >= 0 ? &m_cEntries[nEntry].second : NULL;
	}
	bool Has( const K& cKey ) const { return FindEntry( cKey ) >= 0; }

	/** The key in the map that is equal to cKey, or NULL if it's not in
	 *  (for keys that are equal without being the same, and os::HashSet) */
	const K* FindKey( const K& cKey ) const
	{
		int nEntry = FindEntry( cKey );
		return nEntry >= 0 ? &m_cEntries[nEntry].first : NULL;
	}

	/** Removes a key.  \return false if it was not in. */
	bool Remove( const K& cKey ) { return Take( cKey, NULL ); }

	/** Removes a key, giving its value to pcValue (when not NULL).
	 * \return false if it was not in. */
	bool Take( const K& cKey, V* pcValue )
	{
		int nSlot = Probe( cKey, m_cHash( cKey ), NULL );
		if( nSlot < 0 )
			return false;
		if( pcValue != NULL )
			*pcValue = m_cEntries[m_cIndex[nSlot]].second;
		Kill( nSlot, true );
		return true;
	}

	/** For a map of vectors (or lists, deques: a V with push_back() and
	 *  erase()): adds cItem at the end of the key's vector, adding the key
	 *  with an empty one first when it's not in.  \return the map. */
	template<class T> HashMap& Append( const K& cKey, const T& cItem )
	{
		( *this )[cKey].push_back( cItem );
		return *this;
	}

	/** For a map of vectors: removes item nIndex from the key's vector;
	 *  with bRemoveEmpty, the key goes too when that was its last item.
	 * \return false, changing nothing, if the key is not in or has no
	 *	item nIndex. */
	bool RemoveAt( const K& cKey, int nIndex, bool bRemoveEmpty = false )
	{
		V* pcItems = Find( cKey );
		if( pcItems == NULL || nIndex < 0 || size_t( nIndex ) >= pcItems->size() )
			return false;
		typename V::iterator i = pcItems->begin();
		std::advance( i, nIndex );
		pcItems->erase( i );
		if( bRemoveEmpty && pcItems->empty() )
			Remove( cKey );
		return true;
	}

	/** Removes the entry an iterator is at.  \return the next one.  The
	 *  other iterators stay valid (Erase() never compacts). */
	iterator Erase( iterator i )
	{
		size_t nEntry = size_t( i.m_pPos - &m_cEntries[0] );
		Kill( SlotOf( nEntry ), false );
		return ++i;
	}

	/** Removes the entries bPred( key, value ) is true for, keeping the
	 *  order of the others.  \return how many it removed. */
	template<class P> int RemoveIf( P bPred )
	{
		int nRemoved = 0;
		for( size_t i = 0; i < m_cEntries.size(); i++ )
		{
			Entry& cEntry = m_cEntries[i];
			if( cEntry.m_bLive && bPred( cEntry.first, cEntry.second ) )
			{
				Kill( SlotOf( i ), false );
				nRemoved++;
			}
		}
		Tidy();
		return nRemoved;
	}

	/** Calls cFunc( key, value ) for each entry, in order */
	template<class F> void ForEach( F cFunc ) const
	{
		for( const_iterator i = begin(); i != end(); ++i )
			cFunc( i->first, i->second );
	}
	template<class F> void ForEach( F cFunc )
	{
		for( iterator i = begin(); i != end(); ++i )
			cFunc( i->first, i->second );
	}

	/** The first entry.  \return false if the map is empty. */
	bool First( K* pcKey, V* pcValue = NULL ) const
	{
		int nEntry = FirstEntry();
		if( nEntry < 0 )
			return false;
		Give( m_cEntries[nEntry], pcKey, pcValue );
		return true;
	}
	/** The last entry.  \return false if the map is empty. */
	bool Last( K* pcKey, V* pcValue = NULL ) const
	{
		int nEntry = LastEntry();
		if( nEntry < 0 )
			return false;
		Give( m_cEntries[nEntry], pcKey, pcValue );
		return true;
	}

	/** Takes the last entry out.  \return false if the map is empty. */
	bool Pop( K* pcKey = NULL, V* pcValue = NULL )
	{
		int nEntry = LastEntry();
		if( nEntry < 0 )
			return false;
		Give( m_cEntries[nEntry], pcKey, pcValue );
		Kill( SlotOf( nEntry ), true );
		return true;
	}
	/** Takes the first entry out.  \return false if the map is empty. */
	bool PopFront( K* pcKey = NULL, V* pcValue = NULL )
	{
		int nEntry = FirstEntry();
		if( nEntry < 0 )
			return false;
		Give( m_cEntries[nEntry], pcKey, pcValue );
		m_nHead = size_t( nEntry ) + 1;
		Kill( SlotOf( nEntry ), true );
		return true;
	}

	/** The key and value at position nIndex (0 to Count() - 1) */
	const K& KeyAt( int nIndex ) const { return m_cEntries[EntryAt( nIndex )].first; }
	V& ValueAt( int nIndex ) { return m_cEntries[EntryAt( nIndex )].second; }
	const V& ValueAt( int nIndex ) const { return m_cEntries[EntryAt( nIndex )].second; }

	/** The position of a key, or -1 if it's not in */
	int IndexOf( const K& cKey ) const
	{
		int nEntry = FindEntry( cKey );
		if( nEntry < 0 || m_nHoles == 0 )
			return nEntry;
		int nIndex = 0;
		for( int i = 0; i < nEntry; i++ )
			nIndex += m_cEntries[i].m_bLive;
		return nIndex;
	}

	/** Turns the order of the entries around.  O(n). */
	void Reverse()
	{
		Compact();
		std::reverse( m_cEntries.begin(), m_cEntries.end() );
		Reindex( m_cEntries.size() );
	}

	/** Sorts the entries by key with pfCompare (operator< for NULL),
	 *  ascending or descending.  O(n log n). */
	void Sort( KeyCompare pfCompare = NULL, bool bDescending = false )
	{
		Compact();
		std::sort( m_cEntries.begin(), m_cEntries.end(), map_private::FirstLess<Entry, K>( pfCompare, bDescending ) );
		Reindex( m_cEntries.size() );
	}

	/** Sorts the entries by value with pfCompare (operator< for NULL),
	 *  ascending or descending; entries with equal values keep their
	 *  order.  O(n log n). */
	void SortByValue( ValueCompare pfCompare = NULL, bool bDescending = false )
	{
		Compact();
		std::stable_sort( m_cEntries.begin(), m_cEntries.end(), map_private::SecondLess<Entry, V>( pfCompare, bDescending ) );
		Reindex( m_cEntries.size() );
	}

	/** The keys, in order */
	std::vector<K> Keys() const
	{
		std::vector<K> cKeys;
		cKeys.reserve( m_nCount );
		for( const_iterator i = begin(); i != end(); ++i )
			cKeys.push_back( i->first );
		return cKeys;
	}
	/** The values, in order */
	std::vector<V> Values() const
	{
		std::vector<V> cValues;
		cValues.reserve( m_nCount );
		for( const_iterator i = begin(); i != end(); ++i )
			cValues.push_back( i->second );
		return cValues;
	}
	/** The entries, in order */
	std::vector<Item> Items() const
	{
		std::vector<Item> cItems;
		cItems.reserve( m_nCount );
		for( const_iterator i = begin(); i != end(); ++i )
			cItems.push_back( Item( i->first, i->second ) );
		return cItems;
	}

	/** Adds the entries of another map, in its order, at the end; with
	 *  bOverwrite, its values replace those of keys that are in both
	 *  (which stay where they are) */
	HashMap& Merge( const HashMap& cOther, bool bOverwrite = true )
	{
		Reserve( m_nCount + cOther.m_nCount );
		for( const_iterator i = cOther.begin(); i != cOther.end(); ++i )
			Add( i->first, i->second, bOverwrite );
		return *this;
	}

	/** Makes room for nCount entries without growing the index */
	void Reserve( size_t nCount )
	{
		nCount = std::max( nCount, m_nCount );
		if( ( nCount + m_nDeleted ) * 3 >= m_cIndex.size() * 2 )
		{
			Compact();
			m_cEntries.reserve( nCount );
			Reindex( nCount );
		}
	}

	int Count() const { return int( m_nCount ); }
	bool IsEmpty() const { return m_nCount == 0; }
	void Clear()
	{
		m_cEntries.clear();
		m_cIndex.clear();
		m_nCount = m_nHoles = m_nDeleted = m_nHead = 0;
	}
	void Swap( HashMap& cOther )
	{
		m_cEntries.swap( cOther.m_cEntries );
		m_cIndex.swap( cOther.m_cIndex );
		std::swap( m_cHash, cOther.m_cHash );
		std::swap( m_nCount, cOther.m_nCount );
		std::swap( m_nHoles, cOther.m_nHoles );
		std::swap( m_nDeleted, cOther.m_nDeleted );
		std::swap( m_nHead, cOther.m_nHead );
	}
	/** The size of the index (a power of two) */
	size_t GetCapacity() const { return m_cIndex.size(); }

	/** The same keys with the same values (the order doesn't matter;
	 *  compare Items() for that) */
	bool operator==( const HashMap& cOther ) const
	{
		if( m_nCount != cOther.m_nCount )
			return false;
		for( const_iterator i = begin(); i != end(); ++i )
		{
			const V* pcValue = cOther.Find( i->first );
			if( pcValue == NULL || !( *pcValue == i->second ) )
				return false;
		}
		return true;
	}
	bool operator!=( const HashMap& cOther ) const { return !( *this == cOther ); }

	iterator begin() { return iterator( Begin(), Begin() + m_nHead, End() ); }
	iterator end() { return iterator( Begin(), End(), End() ); }
	const_iterator begin() const { return const_iterator( Begin(), Begin() + m_nHead, End() ); }
	const_iterator end() const { return const_iterator( Begin(), End(), End() ); }
	reverse_iterator rbegin() { return reverse_iterator( end() ); }
	reverse_iterator rend() { return reverse_iterator( begin() ); }
	const_reverse_iterator rbegin() const { return const_reverse_iterator( end() ); }
	const_reverse_iterator rend() const { return const_reverse_iterator( begin() ); }
	size_type size() const { return m_nCount; }
	bool empty() const { return m_nCount == 0; }

private:
	enum { EMPTY = -1, DELETED = -2, MIN_CAPACITY = 8 };

	Entry* Begin() { return m_cEntries.empty() ? NULL : &m_cEntries[0]; }
	Entry* End() { return Begin() + m_cEntries.size(); }
	const Entry* Begin() const { return m_cEntries.empty() ? NULL : &m_cEntries[0]; }
	const Entry* End() const { return Begin() + m_cEntries.size(); }

	static void Give( const Entry& cEntry, K* pcKey, V* pcValue )
	{
		if( pcKey != NULL )
			*pcKey = cEntry.first;
		if( pcValue != NULL )
			*pcValue = cEntry.second;
	}

	/* The index slot of a key, or -1.  With pnFree, gives the slot an
	 * insert of it would take: the first deleted one on the way, or the
	 * empty one that ended the search.  There is always an empty slot. */
	int Probe( const K& cKey, size_t nHash, int* pnFree ) const
	{
		if( __builtin_expect( m_cIndex.empty(), 0 ) )
		{
			if( pnFree != NULL )
				*pnFree = -1;
			return -1;
		}
		const size_t nMask = m_cIndex.size() - 1;
		size_t nSlot = nHash & nMask;
		int nFree = -1;
		for( size_t nStep = 1; ; nStep++ )
		{
			const int nEntry = m_cIndex[nSlot];
			if( nEntry == EMPTY )
			{
				if( pnFree != NULL )
					*pnFree = nFree >= 0 ? nFree : int( nSlot );
				return -1;
			}
			if( nEntry == DELETED )
			{
				if( nFree < 0 )
					nFree = int( nSlot );
			}
			else
			{
				const Entry& cEntry = m_cEntries[nEntry];
				if( __builtin_expect( cEntry.m_nHash == nHash, 1 ) && cEntry.first == cKey )
					return int( nSlot );
			}
			nSlot = ( nSlot + nStep ) & nMask;	// triangular: visits every slot
		}
	}

	int FindEntry( const K& cKey ) const
	{
		int nSlot = Probe( cKey, m_cHash( cKey ), NULL );
		return nSlot >= 0 ? m_cIndex[nSlot] : -1;
	}

	/* The slot that points at an entry */
	int SlotOf( size_t nEntry ) const
	{
		const size_t nMask = m_cIndex.size() - 1;
		size_t nSlot = m_cEntries[nEntry].m_nHash & nMask;
		for( size_t nStep = 1; m_cIndex[nSlot] != int( nEntry ); nStep++ )
			nSlot = ( nSlot + nStep ) & nMask;
		return int( nSlot );
	}

	/* Finds or adds a key (at the end).  Returns its entry and whether it
	 * was added. */
	std::pair<size_t, bool> Add( const K& cKey, const V& cValue, bool bOverwrite )
	{
		const size_t nHash = m_cHash( cKey );
		int nFree = -1;
		int nSlot = Probe( cKey, nHash, &nFree );
		if( nSlot >= 0 )
		{
			size_t nEntry = size_t( m_cIndex[nSlot] );
			if( bOverwrite )
				m_cEntries[nEntry].second = cValue;
			return std::make_pair( nEntry, false );
		}
		// keep a third of the index empty, counting the deleted slots
		if( __builtin_expect( nFree < 0 || ( m_nCount + m_nDeleted + 1 ) * 3 > m_cIndex.size() * 2, 0 ) )
		{
			Compact();
			Reindex( m_nCount + 1 );
			Probe( cKey, nHash, &nFree );
		}
		if( m_cIndex[nFree] == DELETED )
			m_nDeleted--;
		m_cIndex[nFree] = int( m_cEntries.size() );
		m_cEntries.push_back( Entry( cKey, cValue, nHash ) );
		m_nCount++;
		return std::make_pair( m_cEntries.size() - 1, true );
	}

	/* Removes the entry a slot points at.  With bTidy, the holes at the
	 * end go, and the array is compacted once it is mostly holes. */
	void Kill( int nSlot, bool bTidy )
	{
		Entry& cEntry = m_cEntries[m_cIndex[nSlot]];
		m_cIndex[nSlot] = DELETED;
		m_nDeleted++;
		cEntry.first = K();
		cEntry.second = V();
		cEntry.m_bLive = false;
		m_nCount--;
		m_nHoles++;
		if( bTidy )
			Tidy();
	}

	void Tidy()
	{
		// holes at the end aren't pointed at, so they can just go
		while( !m_cEntries.empty() && !m_cEntries.back().m_bLive )
		{
			m_cEntries.pop_back();
			m_nHoles--;
		}
		if( m_nHead > m_cEntries.size() )
			m_nHead = m_cEntries.size();
		if( m_nCount == 0 )
			Clear();
		else if( m_nHoles > 16 && m_nHoles > m_nCount )
		{
			Compact();
			Reindex( m_nCount );
		}
	}

	/* Takes the holes out of the array (the index must be rebuilt after) */
	void Compact()
	{
		if( m_nHoles == 0 )
			return;
		size_t nTo = 0;
		for( size_t i = 0; i < m_cEntries.size(); i++ )
		{
			if( m_cEntries[i].m_bLive )
			{
				if( nTo != i )
					m_cEntries[nTo] = m_cEntries[i];
				nTo++;
			}
		}
		m_cEntries.resize( nTo );
		m_nHoles = 0;
		m_nHead = 0;
		m_cIndex.clear();		// stale: Reindex() next
	}

	/* Builds the index anew for the (compacted) entries, with room for
	 * nRoom of them in two thirds of it */
	void Reindex( size_t nRoom )
	{
		size_t nCapacity = NextPowerOfTwo( std::max( size_t( MIN_CAPACITY ), nRoom + nRoom / 2 + 1 ) );
		m_cIndex.assign( nCapacity, int( EMPTY ) );
		m_nDeleted = 0;
		m_nHead = 0;
		const size_t nMask = nCapacity - 1;
		for( size_t i = 0; i < m_cEntries.size(); i++ )
		{
			if( i + 4 < m_cEntries.size() )
				__builtin_prefetch( &m_cEntries[i + 4] );
			size_t nSlot = m_cEntries[i].m_nHash & nMask;
			for( size_t nStep = 1; m_cIndex[nSlot] != EMPTY; nStep++ )
				nSlot = ( nSlot + nStep ) & nMask;
			m_cIndex[nSlot] = int( i );
		}
	}

	int FirstEntry() const
	{
		if( m_nCount == 0 )
			return -1;
		size_t i = m_nHead;
		while( !m_cEntries[i].m_bLive )
			i++;
		return int( i );
	}
	int LastEntry() const
	{
		if( m_nCount == 0 )
			return -1;
		size_t i = m_cEntries.size() - 1;
		while( !m_cEntries[i].m_bLive )
			i--;
		return int( i );
	}
	size_t EntryAt( int nIndex ) const
	{
		if( m_nHoles == 0 )
			return size_t( nIndex );
		for( size_t i = 0; ; i++ )
			if( m_cEntries[i].m_bLive && nIndex-- == 0 )
				return i;
	}

	std::vector<Entry> m_cEntries;	// in order, with holes where entries were removed
	std::vector<int> m_cIndex;	// slots: entry numbers, EMPTY or DELETED
	H m_cHash;
	size_t m_nCount;		// live entries
	size_t m_nHoles;		// removed entries still in m_cEntries
	size_t m_nDeleted;		// DELETED slots in m_cIndex
	size_t m_nHead;			// no live entry before this one
};

} // namespace os

#endif // __F_UTIL_HASHMAP_H__
