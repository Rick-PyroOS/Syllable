/*  Map - os::Map (ordered by key) and os::UnorderedMap (hashed).
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

#ifndef	__F_UTIL_MAP_H__
#define	__F_UTIL_MAP_H__

#include <util/hash.h>
#include <map>
#include <vector>
#include <utility>
#include <algorithm>
#include <functional>
#include <iterator>
#if __cplusplus >= 201103L
#include <unordered_map>
#else
#include <tr1/unordered_map>
#endif

namespace os
{

namespace map_private
{
/* os::Map's key order: a comparison function (operator< without one),
 * forwards or reversed.  std::map keeps a copy of it. */
template<class K> struct KeyOrder
{
	typedef bool (*Compare)( const K&, const K& );
	KeyOrder() : m_pfCompare( NULL ), m_bReversed( false ) {}
	KeyOrder( Compare pfCompare, bool bReversed ) : m_pfCompare( pfCompare ), m_bReversed( bReversed ) {}
	bool Less( const K& cA, const K& cB ) const
	{
		return m_pfCompare != NULL ? m_pfCompare( cA, cB ) : cA < cB;
	}
	bool operator()( const K& cA, const K& cB ) const
	{
		return m_bReversed ? Less( cB, cA ) : Less( cA, cB );
	}
	Compare m_pfCompare;
	bool m_bReversed;
};
} // namespace map_private

/** A map of keys to values, in the order of its keys.
 * \ingroup util
 * \par Description:
 *	os::Map keeps its entries sorted by key: ascending by the key's
 *	operator< unless you Sort() it with a comparison of your own, or
 *	Reverse() it.  The order stays as the map changes: an entry added
 *	later goes in its place.  It is a std::map (a red-black tree), so
 *	finding, adding and removing a key are O(log n), and it works with
 *	the iterators and algorithms of the C++ library.
 *
 *	Use os::Map when the order of the keys matters (a sorted list,
 *	ranges with LowerBound()/UpperBound(), the first or last key),
 *	os::UnorderedMap when it doesn't, and os::HashMap for a hash table
 *	that keeps the order you put the entries in.
 * \code
 *	os::Map<os::String, int> cAges;
 *	cAges.Set( "kurt", 41 ).Set( "anna", 33 ).Set( "rick", 50 );	// insert
 *	cAges.Set( "anna", 34 );					// update
 *	cAges.Insert( "anna", 99 );		// false: there is an anna already
 *	cAges.Update( "nobody", 1 );		// false: there is no nobody
 *	int nAge = cAges.Get( "kurt", -1 );
 *	cAges.Remove( "kurt" );
 *	for( os::Map<os::String, int>::const_iterator i = cAges.begin(); i != cAges.end(); ++i )
 *	    printf( "%s %d\n", i->first.c_str(), i->second );	// anna 34, rick 50
 *	cAges.Reverse();			// rick, anna
 *	os::String cName;
 *	cAges.PopFront( &cName, &nAge );	// rick 50
 * \endcode
 *
 *	Push() is Set() (the key goes where its order puts it); Pop() and
 *	PopFront() take the last and the first entry out.  Sort() with a
 *	comparison (a function, or a lambda without captures) changes the
 *	key order for good, and fails, changing nothing, if the comparison
 *	finds two keys equal.  To list the entries by value, ItemsByValue().
 *
 *	K needs operator< (or a comparison given to Sort()), and V needs a
 *	default constructor for Get() and operator[].
 */
template<class K, class V>
class Map
{
public:
	typedef bool (*KeyCompare)( const K&, const K& );
	typedef bool (*ValueCompare)( const V&, const V& );
	typedef map_private::KeyOrder<K> Order;
	typedef std::map<K, V, Order> Container;
	typedef K key_type;
	typedef V mapped_type;
	typedef typename Container::value_type value_type;
	typedef typename Container::iterator iterator;
	typedef typename Container::const_iterator const_iterator;
	typedef typename Container::reverse_iterator reverse_iterator;
	typedef typename Container::const_reverse_iterator const_reverse_iterator;
	typedef typename Container::size_type size_type;
	typedef std::pair<K, V> Item;

	Map() {}

	/** Adds the key with the value, if the key is not in yet.
	 * \return true if it was added, false if the key was in (its value
	 *	stays). */
	bool Insert( const K& cKey, const V& cValue ) { return m_cMap.insert( value_type( cKey, cValue ) ).second; }

	/** Changes the value of a key that is in.
	 * \return false, changing nothing, if the key is not in. */
	bool Update( const K& cKey, const V& cValue )
	{
		iterator i = m_cMap.find( cKey );
		if( i == m_cMap.end() )
			return false;
		i->second = cValue;
		return true;
	}

	/** Sets the value of a key: adds it, or changes it if it's in.
	 * \return the map, to chain Set()s. */
	Map& Set( const K& cKey, const V& cValue )
	{
		std::pair<iterator, bool> cAdded = m_cMap.insert( value_type( cKey, cValue ) );
		if( !cAdded.second )
			cAdded.first->second = cValue;
		return *this;
	}

	/** Set(): the key goes where the order puts it */
	Map& Push( const K& cKey, const V& cValue ) { return Set( cKey, cValue ); }

	/** The value of a key, added with V() when the key is not in */
	V& operator[]( const K& cKey ) { return m_cMap[cKey]; }

	/** The value of a key, or cDefault when it's not in */
	V Get( const K& cKey, const V& cDefault = V() ) const
	{
		const_iterator i = m_cMap.find( cKey );
		return i != m_cMap.end() ? i->second : cDefault;
	}

	/** The value of a key, to read or change in place, or NULL */
	V* Find( const K& cKey )
	{
		iterator i = m_cMap.find( cKey );
		return i != m_cMap.end() ? &i->second : NULL;
	}
	const V* Find( const K& cKey ) const
	{
		const_iterator i = m_cMap.find( cKey );
		return i != m_cMap.end() ? &i->second : NULL;
	}

	bool Has( const K& cKey ) const { return m_cMap.find( cKey ) != m_cMap.end(); }

	/** Removes a key.  \return false if it was not in. */
	bool Remove( const K& cKey ) { return m_cMap.erase( cKey ) != 0; }

	/** Removes a key, giving its value to pcValue (when not NULL).
	 * \return false if it was not in. */
	bool Take( const K& cKey, V* pcValue )
	{
		iterator i = m_cMap.find( cKey );
		if( i == m_cMap.end() )
			return false;
		if( pcValue != NULL )
			*pcValue = i->second;
		m_cMap.erase( i );
		return true;
	}

	/** For a map of vectors (or lists, deques: a V with push_back() and
	 *  erase()): adds cItem at the end of the key's vector, adding the key
	 *  with an empty one first when it's not in.  \return the map. */
	template<class T> Map& Append( const K& cKey, const T& cItem )
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

	/** Removes the entry an iterator is at.  \return the next one. */
	iterator Erase( iterator i )
	{
		iterator cNext = i;
		++cNext;
		m_cMap.erase( i );
		return cNext;
	}

	/** Removes the entries bPred( key, value ) is true for.
	 * \return how many it removed. */
	template<class P> int RemoveIf( P bPred )
	{
		int nRemoved = 0;
		for( iterator i = m_cMap.begin(); i != m_cMap.end(); )
		{
			if( bPred( i->first, i->second ) )
			{
				m_cMap.erase( i++ );
				nRemoved++;
			}
			else
				++i;
		}
		return nRemoved;
	}

	/** Calls cFunc( key, value ) for each entry, in order */
	template<class F> void ForEach( F cFunc ) const
	{
		for( const_iterator i = m_cMap.begin(); i != m_cMap.end(); ++i )
			cFunc( i->first, i->second );
	}
	template<class F> void ForEach( F cFunc )
	{
		for( iterator i = m_cMap.begin(); i != m_cMap.end(); ++i )
			cFunc( i->first, i->second );
	}

	/** The first entry in the order.  \return false if the map is empty. */
	bool First( K* pcKey, V* pcValue = NULL ) const
	{
		if( m_cMap.empty() )
			return false;
		Give( m_cMap.begin(), pcKey, pcValue );
		return true;
	}
	/** The last entry in the order.  \return false if the map is empty. */
	bool Last( K* pcKey, V* pcValue = NULL ) const
	{
		if( m_cMap.empty() )
			return false;
		Give( --m_cMap.end(), pcKey, pcValue );
		return true;
	}

	/** Takes the last entry out.  \return false if the map is empty. */
	bool Pop( K* pcKey = NULL, V* pcValue = NULL )
	{
		if( m_cMap.empty() )
			return false;
		iterator i = --m_cMap.end();
		Give( i, pcKey, pcValue );
		m_cMap.erase( i );
		return true;
	}
	/** Takes the first entry out.  \return false if the map is empty. */
	bool PopFront( K* pcKey = NULL, V* pcValue = NULL )
	{
		if( m_cMap.empty() )
			return false;
		Give( m_cMap.begin(), pcKey, pcValue );
		m_cMap.erase( m_cMap.begin() );
		return true;
	}

	/** Turns the order around (ascending becomes descending), in O(n) */
	void Reverse()
	{
		Order cOrder = m_cMap.key_comp();
		cOrder.m_bReversed = !cOrder.m_bReversed;
		Container cNew( cOrder );
		// in the old order backwards, each key is the last so far in the new
		for( reverse_iterator i = m_cMap.rbegin(); i != m_cMap.rend(); ++i )
			cNew.insert( cNew.end(), *i );
		m_cMap.swap( cNew );
	}
	bool IsReversed() const { return m_cMap.key_comp().m_bReversed; }

	/** Sorts by the keys with pfCompare from now on (operator< for NULL),
	 *  ascending, or descending with bDescending.
	 * \return false, changing nothing, if pfCompare finds two of the keys
	 *	equal (the map would have to drop one). */
	bool Sort( KeyCompare pfCompare = NULL, bool bDescending = false )
	{
		Container cNew( Order( pfCompare, bDescending ) );
		for( const_iterator i = m_cMap.begin(); i != m_cMap.end(); ++i )
			cNew.insert( cNew.end(), *i );
		if( cNew.size() != m_cMap.size() )
			return false;
		m_cMap.swap( cNew );
		return true;
	}

	/** The first entry that is not before cKey in the order, or end() */
	iterator LowerBound( const K& cKey ) { return m_cMap.lower_bound( cKey ); }
	const_iterator LowerBound( const K& cKey ) const { return m_cMap.lower_bound( cKey ); }
	/** The first entry that is after cKey in the order, or end() */
	iterator UpperBound( const K& cKey ) { return m_cMap.upper_bound( cKey ); }
	const_iterator UpperBound( const K& cKey ) const { return m_cMap.upper_bound( cKey ); }

	/** The keys, in order */
	std::vector<K> Keys() const
	{
		std::vector<K> cKeys;
		cKeys.reserve( m_cMap.size() );
		for( const_iterator i = m_cMap.begin(); i != m_cMap.end(); ++i )
			cKeys.push_back( i->first );
		return cKeys;
	}
	/** The values, in the order of their keys */
	std::vector<V> Values() const
	{
		std::vector<V> cValues;
		cValues.reserve( m_cMap.size() );
		for( const_iterator i = m_cMap.begin(); i != m_cMap.end(); ++i )
			cValues.push_back( i->second );
		return cValues;
	}
	/** The entries, in order */
	std::vector<Item> Items() const { return std::vector<Item>( m_cMap.begin(), m_cMap.end() ); }
	/** The entries, sorted by value with pfCompare (operator< for NULL),
	 *  ascending or descending; entries with equal values stay in the
	 *  map's order */
	std::vector<Item> ItemsByValue( ValueCompare pfCompare = NULL, bool bDescending = false ) const
	{
		std::vector<Item> cItems = Items();
		std::stable_sort( cItems.begin(), cItems.end(), map_private::SecondLess<Item, V>( pfCompare, bDescending ) );
		return cItems;
	}

	/** Adds the entries of another map; with bOverwrite, its values
	 *  replace those of the keys that are in both */
	Map& Merge( const Map& cOther, bool bOverwrite = true )
	{
		for( const_iterator i = cOther.begin(); i != cOther.end(); ++i )
		{
			if( bOverwrite )
				Set( i->first, i->second );
			else
				Insert( i->first, i->second );
		}
		return *this;
	}

	int Count() const { return int( m_cMap.size() ); }
	bool IsEmpty() const { return m_cMap.empty(); }
	void Clear() { m_cMap.clear(); }
	void Swap( Map& cOther ) { m_cMap.swap( cOther.m_cMap ); }

	/** The same keys with the same values (the order doesn't matter) */
	bool operator==( const Map& cOther ) const
	{
		if( m_cMap.size() != cOther.m_cMap.size() )
			return false;
		for( const_iterator i = m_cMap.begin(); i != m_cMap.end(); ++i )
		{
			const V* pcValue = cOther.Find( i->first );
			if( pcValue == NULL || !( *pcValue == i->second ) )
				return false;
		}
		return true;
	}
	bool operator!=( const Map& cOther ) const { return !( *this == cOther ); }

	/** The std::map in it */
	Container& GetContainer() { return m_cMap; }
	const Container& GetContainer() const { return m_cMap; }

	// The C++ library's names, for templates and range-for
	iterator begin() { return m_cMap.begin(); }
	iterator end() { return m_cMap.end(); }
	const_iterator begin() const { return m_cMap.begin(); }
	const_iterator end() const { return m_cMap.end(); }
	reverse_iterator rbegin() { return m_cMap.rbegin(); }
	reverse_iterator rend() { return m_cMap.rend(); }
	const_reverse_iterator rbegin() const { return m_cMap.rbegin(); }
	const_reverse_iterator rend() const { return m_cMap.rend(); }
	size_type size() const { return m_cMap.size(); }
	bool empty() const { return m_cMap.empty(); }

private:
	static void Give( const_iterator i, K* pcKey, V* pcValue )
	{
		if( pcKey != NULL )
			*pcKey = i->first;
		if( pcValue != NULL )
			*pcValue = i->second;
	}

	Container m_cMap;
};

/** A map of keys to values, in no particular order.
 * \ingroup util
 * \par Description:
 *	os::UnorderedMap is a hash table of the C++ library
 *	(std::unordered_map, or std::tr1::unordered_map before C++11) with
 *	os::Hash: finding, adding and removing a key take about the same
 *	time however big it is.  The order of its entries is the table's,
 *	and changes as it grows.  It has the methods of os::Map, except
 *	those about the order: Pop() takes out whichever entry comes first,
 *	and ItemsByKey() and ItemsByValue() give the entries sorted.
 * \code
 *	os::UnorderedMap<int, os::String> cNames;
 *	cNames.Set( 3, "three" ).Set( 1, "one" ).Set( 2, "two" );
 *	std::vector<os::UnorderedMap<int, os::String>::Item> cSorted = cNames.ItemsByKey();	// 1, 2, 3
 * \endcode
 *
 *	K needs operator== and an os::Hash (or a hash class H), and V a
 *	default constructor for Get() and operator[].
 */
template<class K, class V, class H = Hash<K> >
class UnorderedMap
{
public:
	typedef bool (*KeyCompare)( const K&, const K& );
	typedef bool (*ValueCompare)( const V&, const V& );
#if __cplusplus >= 201103L
	typedef std::unordered_map<K, V, H> Container;
#else
	typedef std::tr1::unordered_map<K, V, H> Container;
#endif
	typedef K key_type;
	typedef V mapped_type;
	typedef typename Container::value_type value_type;
	typedef typename Container::iterator iterator;
	typedef typename Container::const_iterator const_iterator;
	typedef typename Container::size_type size_type;
	typedef std::pair<K, V> Item;

	UnorderedMap() {}

	/** Adds the key with the value, if the key is not in yet.
	 * \return true if it was added. */
	bool Insert( const K& cKey, const V& cValue ) { return m_cMap.insert( value_type( cKey, cValue ) ).second; }

	/** Changes the value of a key that is in.  \return false if it's not. */
	bool Update( const K& cKey, const V& cValue )
	{
		iterator i = m_cMap.find( cKey );
		if( i == m_cMap.end() )
			return false;
		i->second = cValue;
		return true;
	}

	/** Adds the key, or changes its value if it's in.  \return the map. */
	UnorderedMap& Set( const K& cKey, const V& cValue )
	{
		std::pair<iterator, bool> cAdded = m_cMap.insert( value_type( cKey, cValue ) );
		if( !cAdded.second )
			cAdded.first->second = cValue;
		return *this;
	}
	UnorderedMap& Push( const K& cKey, const V& cValue ) { return Set( cKey, cValue ); }

	V& operator[]( const K& cKey ) { return m_cMap[cKey]; }

	V Get( const K& cKey, const V& cDefault = V() ) const
	{
		const_iterator i = m_cMap.find( cKey );
		return i != m_cMap.end() ? i->second : cDefault;
	}
	V* Find( const K& cKey )
	{
		iterator i = m_cMap.find( cKey );
		return i != m_cMap.end() ? &i->second : NULL;
	}
	const V* Find( const K& cKey ) const
	{
		const_iterator i = m_cMap.find( cKey );
		return i != m_cMap.end() ? &i->second : NULL;
	}
	bool Has( const K& cKey ) const { return m_cMap.find( cKey ) != m_cMap.end(); }

	bool Remove( const K& cKey ) { return m_cMap.erase( cKey ) != 0; }
	bool Take( const K& cKey, V* pcValue )
	{
		iterator i = m_cMap.find( cKey );
		if( i == m_cMap.end() )
			return false;
		if( pcValue != NULL )
			*pcValue = i->second;
		m_cMap.erase( i );
		return true;
	}
	/** For a map of vectors (or lists, deques: a V with push_back() and
	 *  erase()): adds cItem at the end of the key's vector, adding the key
	 *  with an empty one first when it's not in.  \return the map. */
	template<class T> UnorderedMap& Append( const K& cKey, const T& cItem )
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

	iterator Erase( iterator i )
	{
		iterator cNext = i;
		++cNext;
		m_cMap.erase( i );
		return cNext;
	}
	template<class P> int RemoveIf( P bPred )
	{
		int nRemoved = 0;
		for( iterator i = m_cMap.begin(); i != m_cMap.end(); )
		{
			if( bPred( i->first, i->second ) )
			{
				m_cMap.erase( i++ );
				nRemoved++;
			}
			else
				++i;
		}
		return nRemoved;
	}
	template<class F> void ForEach( F cFunc ) const
	{
		for( const_iterator i = m_cMap.begin(); i != m_cMap.end(); ++i )
			cFunc( i->first, i->second );
	}
	template<class F> void ForEach( F cFunc )
	{
		for( iterator i = m_cMap.begin(); i != m_cMap.end(); ++i )
			cFunc( i->first, i->second );
	}

	/** Takes out whichever entry comes first.  \return false if empty. */
	bool Pop( K* pcKey = NULL, V* pcValue = NULL )
	{
		if( m_cMap.empty() )
			return false;
		iterator i = m_cMap.begin();
		if( pcKey != NULL )
			*pcKey = i->first;
		if( pcValue != NULL )
			*pcValue = i->second;
		m_cMap.erase( i );
		return true;
	}

	/** The keys, in the table's order */
	std::vector<K> Keys() const
	{
		std::vector<K> cKeys;
		cKeys.reserve( m_cMap.size() );
		for( const_iterator i = m_cMap.begin(); i != m_cMap.end(); ++i )
			cKeys.push_back( i->first );
		return cKeys;
	}
	std::vector<V> Values() const
	{
		std::vector<V> cValues;
		cValues.reserve( m_cMap.size() );
		for( const_iterator i = m_cMap.begin(); i != m_cMap.end(); ++i )
			cValues.push_back( i->second );
		return cValues;
	}
	std::vector<Item> Items() const { return std::vector<Item>( m_cMap.begin(), m_cMap.end() ); }
	/** The keys sorted with pfCompare (operator< for NULL) */
	std::vector<K> SortedKeys( KeyCompare pfCompare = NULL, bool bDescending = false ) const
	{
		std::vector<K> cKeys = Keys();
		if( pfCompare != NULL )
			std::sort( cKeys.begin(), cKeys.end(), pfCompare );
		else
			std::sort( cKeys.begin(), cKeys.end() );
		if( bDescending )
			std::reverse( cKeys.begin(), cKeys.end() );
		return cKeys;
	}
	/** The entries sorted by key with pfCompare (operator< for NULL) */
	std::vector<Item> ItemsByKey( KeyCompare pfCompare = NULL, bool bDescending = false ) const
	{
		std::vector<Item> cItems = Items();
		std::sort( cItems.begin(), cItems.end(), map_private::FirstLess<Item, K>( pfCompare, bDescending ) );
		return cItems;
	}
	/** The entries sorted by value with pfCompare (operator< for NULL);
	 *  with equal values, the order of the keys is the table's */
	std::vector<Item> ItemsByValue( ValueCompare pfCompare = NULL, bool bDescending = false ) const
	{
		std::vector<Item> cItems = Items();
		std::stable_sort( cItems.begin(), cItems.end(), map_private::SecondLess<Item, V>( pfCompare, bDescending ) );
		return cItems;
	}

	UnorderedMap& Merge( const UnorderedMap& cOther, bool bOverwrite = true )
	{
		for( const_iterator i = cOther.begin(); i != cOther.end(); ++i )
		{
			if( bOverwrite )
				Set( i->first, i->second );
			else
				Insert( i->first, i->second );
		}
		return *this;
	}

	/** Makes room for nCount entries without growing the table */
	void Reserve( size_t nCount )
	{
		m_cMap.rehash( size_t( nCount / m_cMap.max_load_factor() ) + 1 );
	}
	size_t GetBucketCount() const { return m_cMap.bucket_count(); }
	float GetLoadFactor() const { return m_cMap.load_factor(); }

	int Count() const { return int( m_cMap.size() ); }
	bool IsEmpty() const { return m_cMap.empty(); }
	void Clear() { m_cMap.clear(); }
	void Swap( UnorderedMap& cOther ) { m_cMap.swap( cOther.m_cMap ); }

	bool operator==( const UnorderedMap& cOther ) const
	{
		if( m_cMap.size() != cOther.m_cMap.size() )
			return false;
		for( const_iterator i = m_cMap.begin(); i != m_cMap.end(); ++i )
		{
			const V* pcValue = cOther.Find( i->first );
			if( pcValue == NULL || !( *pcValue == i->second ) )
				return false;
		}
		return true;
	}
	bool operator!=( const UnorderedMap& cOther ) const { return !( *this == cOther ); }

	Container& GetContainer() { return m_cMap; }
	const Container& GetContainer() const { return m_cMap; }

	iterator begin() { return m_cMap.begin(); }
	iterator end() { return m_cMap.end(); }
	const_iterator begin() const { return m_cMap.begin(); }
	const_iterator end() const { return m_cMap.end(); }
	size_type size() const { return m_cMap.size(); }
	bool empty() const { return m_cMap.empty(); }

private:
	Container m_cMap;
};

} // namespace os

#endif // __F_UTIL_MAP_H__
