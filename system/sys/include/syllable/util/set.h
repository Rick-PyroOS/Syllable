/*  Set - os::Set, a set of keys in their order.
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

#ifndef	__F_UTIL_SET_H__
#define	__F_UTIL_SET_H__

#include <util/map.h>
#include <set>
#include <vector>

namespace os
{

/** A set of keys, each in it once, in the order of the keys.
 * \ingroup util
 * \par Description:
 *	os::Set is os::Map without the values: it keeps its keys sorted,
 *	ascending by operator< unless you Sort() it with a comparison of your
 *	own or Reverse() it, and a key added later goes in its place.  It is
 *	a std::set (a red-black tree), so finding, adding and removing a key
 *	are O(log n).
 *
 *	Use os::Set when the order of the keys matters, and os::HashSet for a
 *	hash table that keeps the order you put the keys in.
 * \code
 *	os::Set<os::String> cTags;
 *	cTags.Add( "work" ).Add( "home" ).Add( "urgent" );	// home, urgent, work
 *	cTags.Insert( "home" );		// false: it's in already
 *	if( cTags.Has( "urgent" ) )
 *	    cTags.Remove( "urgent" );
 *	os::Set<os::String> cOther;
 *	cOther.Add( "home" ).Add( "garden" );
 *	cTags.Union( cOther );			// garden, home, work
 *	cTags.Intersection( cOther );		// home
 *	cTags.Difference( cOther );		// work
 *	cTags.Merge( cOther );			// cTags itself: garden, home, work
 *	for( os::Set<os::String>::const_iterator i = cTags.begin(); i != cTags.end(); ++i )
 *	    printf( "%s\n", i->c_str() );
 * \endcode
 *
 *	Add() adds a key and chains; Insert() says whether it was new.
 *	Union(), Intersection(), Difference() and SymmetricDifference() make
 *	a new set (in this set's order); Merge(), Intersect() and Subtract()
 *	change this one.  IsSubsetOf(), IsSupersetOf() and Intersects()
 *	compare two sets.  Pop() and PopFront() take the last and the first
 *	key out.  Sort() with a comparison changes the key order for good,
 *	and fails, changing nothing, if the comparison finds two keys equal.
 *
 *	K needs operator< (or a comparison given to Sort()).
 */
template<class K>
class Set
{
public:
	typedef bool (*KeyCompare)( const K&, const K& );
	typedef map_private::KeyOrder<K> Order;
	typedef std::set<K, Order> Container;
	typedef K key_type;
	typedef K value_type;
	typedef typename Container::iterator iterator;
	typedef typename Container::const_iterator const_iterator;
	typedef typename Container::reverse_iterator reverse_iterator;
	typedef typename Container::const_reverse_iterator const_reverse_iterator;
	typedef typename Container::size_type size_type;

	Set() {}
	/** A set of the keys from cFirst to cLast (a vector's, a list's...) */
	template<class I> Set( I cFirst, I cLast ) : m_cSet( cFirst, cLast ) {}

	/** Adds the key if it's not in yet.  \return true if it was added. */
	bool Insert( const K& cKey ) { return m_cSet.insert( cKey ).second; }

	/** Adds the key (it stays once if it's in).  \return the set, to chain Add()s. */
	Set& Add( const K& cKey )
	{
		m_cSet.insert( cKey );
		return *this;
	}
	/** Add(): the key goes where the order puts it */
	Set& Push( const K& cKey ) { return Add( cKey ); }

	bool Has( const K& cKey ) const { return m_cSet.find( cKey ) != m_cSet.end(); }

	/** The key in the set that is equal to cKey, or NULL if none is */
	const K* Find( const K& cKey ) const
	{
		const_iterator i = m_cSet.find( cKey );
		return i != m_cSet.end() ? &*i : NULL;
	}

	/** Removes a key.  \return false if it was not in. */
	bool Remove( const K& cKey ) { return m_cSet.erase( cKey ) != 0; }

	/** Removes a key, giving the one that was in to pcKey (when not NULL).
	 * \return false if it was not in. */
	bool Take( const K& cKey, K* pcKey )
	{
		iterator i = m_cSet.find( cKey );
		if( i == m_cSet.end() )
			return false;
		if( pcKey != NULL )
			*pcKey = *i;
		m_cSet.erase( i );
		return true;
	}

	/** Removes the key an iterator is at.  \return the next one. */
	iterator Erase( iterator i )
	{
		iterator cNext = i;
		++cNext;
		m_cSet.erase( i );
		return cNext;
	}

	/** Removes the keys bPred( key ) is true for.  \return how many. */
	template<class P> int RemoveIf( P bPred )
	{
		int nRemoved = 0;
		for( iterator i = m_cSet.begin(); i != m_cSet.end(); )
		{
			if( bPred( *i ) )
			{
				m_cSet.erase( i++ );
				nRemoved++;
			}
			else
				++i;
		}
		return nRemoved;
	}

	/** Calls cFunc( key ) for each key, in order */
	template<class F> void ForEach( F cFunc ) const
	{
		for( const_iterator i = m_cSet.begin(); i != m_cSet.end(); ++i )
			cFunc( *i );
	}

	/** The first key in the order.  \return false if the set is empty. */
	bool First( K* pcKey ) const
	{
		if( m_cSet.empty() )
			return false;
		*pcKey = *m_cSet.begin();
		return true;
	}
	/** The last key in the order.  \return false if the set is empty. */
	bool Last( K* pcKey ) const
	{
		if( m_cSet.empty() )
			return false;
		*pcKey = *m_cSet.rbegin();
		return true;
	}
	/** Takes the last key out.  \return false if the set is empty. */
	bool Pop( K* pcKey = NULL )
	{
		if( m_cSet.empty() )
			return false;
		iterator i = --m_cSet.end();
		if( pcKey != NULL )
			*pcKey = *i;
		m_cSet.erase( i );
		return true;
	}
	/** Takes the first key out.  \return false if the set is empty. */
	bool PopFront( K* pcKey = NULL )
	{
		if( m_cSet.empty() )
			return false;
		if( pcKey != NULL )
			*pcKey = *m_cSet.begin();
		m_cSet.erase( m_cSet.begin() );
		return true;
	}

	/** Turns the order around (ascending becomes descending), in O(n) */
	void Reverse()
	{
		Order cOrder = m_cSet.key_comp();
		cOrder.m_bReversed = !cOrder.m_bReversed;
		Container cNew( cOrder );
		for( reverse_iterator i = m_cSet.rbegin(); i != m_cSet.rend(); ++i )
			cNew.insert( cNew.end(), *i );
		m_cSet.swap( cNew );
	}
	bool IsReversed() const { return m_cSet.key_comp().m_bReversed; }

	/** Sorts the keys with pfCompare from now on (operator< for NULL),
	 *  ascending, or descending with bDescending.
	 * \return false, changing nothing, if pfCompare finds two of the keys
	 *	equal (the set would have to drop one). */
	bool Sort( KeyCompare pfCompare = NULL, bool bDescending = false )
	{
		Container cNew( Order( pfCompare, bDescending ) );
		for( const_iterator i = m_cSet.begin(); i != m_cSet.end(); ++i )
			cNew.insert( *i );
		if( cNew.size() != m_cSet.size() )
			return false;
		m_cSet.swap( cNew );
		return true;
	}

	/** The first key not before cKey */
	iterator LowerBound( const K& cKey ) { return m_cSet.lower_bound( cKey ); }
	const_iterator LowerBound( const K& cKey ) const { return m_cSet.lower_bound( cKey ); }
	/** The first key after cKey */
	iterator UpperBound( const K& cKey ) { return m_cSet.upper_bound( cKey ); }
	const_iterator UpperBound( const K& cKey ) const { return m_cSet.upper_bound( cKey ); }

	/** The keys, in order */
	std::vector<K> Keys() const { return std::vector<K>( m_cSet.begin(), m_cSet.end() ); }

	/** Adds the keys of another set.  \return this set. */
	Set& Merge( const Set& cOther )
	{
		for( const_iterator i = cOther.begin(); i != cOther.end(); ++i )
			m_cSet.insert( *i );
		return *this;
	}
	/** Keeps only the keys that are in cOther too.  \return this set. */
	Set& Intersect( const Set& cOther )
	{
		for( iterator i = m_cSet.begin(); i != m_cSet.end(); )
			if( !cOther.Has( *i ) )
				m_cSet.erase( i++ );
			else
				++i;
		return *this;
	}
	/** Removes the keys that are in cOther.  \return this set. */
	Set& Subtract( const Set& cOther )
	{
		for( const_iterator i = cOther.begin(); i != cOther.end(); ++i )
			m_cSet.erase( *i );
		return *this;
	}

	/** The keys in either set */
	Set Union( const Set& cOther ) const { Set cNew( *this ); return cNew.Merge( cOther ); }
	/** The keys in both sets */
	Set Intersection( const Set& cOther ) const
	{
		Set cNew( Empty() );
		for( const_iterator i = m_cSet.begin(); i != m_cSet.end(); ++i )
			if( cOther.Has( *i ) )
				cNew.m_cSet.insert( cNew.m_cSet.end(), *i );
		return cNew;
	}
	/** The keys in this set and not in cOther */
	Set Difference( const Set& cOther ) const
	{
		Set cNew( Empty() );
		for( const_iterator i = m_cSet.begin(); i != m_cSet.end(); ++i )
			if( !cOther.Has( *i ) )
				cNew.m_cSet.insert( cNew.m_cSet.end(), *i );
		return cNew;
	}
	/** The keys in one of the sets but not both */
	Set SymmetricDifference( const Set& cOther ) const
	{
		Set cNew( Difference( cOther ) );
		for( const_iterator i = cOther.begin(); i != cOther.end(); ++i )
			if( !Has( *i ) )
				cNew.m_cSet.insert( *i );
		return cNew;
	}

	/** Whether every key of this set is in cOther */
	bool IsSubsetOf( const Set& cOther ) const
	{
		if( m_cSet.size() > cOther.m_cSet.size() )
			return false;
		for( const_iterator i = m_cSet.begin(); i != m_cSet.end(); ++i )
			if( !cOther.Has( *i ) )
				return false;
		return true;
	}
	/** Whether every key of cOther is in this set */
	bool IsSupersetOf( const Set& cOther ) const { return cOther.IsSubsetOf( *this ); }
	/** Whether the sets have a key in common */
	bool Intersects( const Set& cOther ) const
	{
		const Set& cSmall = m_cSet.size() <= cOther.m_cSet.size() ? *this : cOther;
		const Set& cLarge = &cSmall == this ? cOther : *this;
		for( const_iterator i = cSmall.begin(); i != cSmall.end(); ++i )
			if( cLarge.Has( *i ) )
				return true;
		return false;
	}

	int Count() const { return int( m_cSet.size() ); }
	bool IsEmpty() const { return m_cSet.empty(); }
	void Clear() { m_cSet.clear(); }
	void Swap( Set& cOther ) { m_cSet.swap( cOther.m_cSet ); }

	/** The same keys (the order doesn't matter) */
	bool operator==( const Set& cOther ) const
	{
		return m_cSet.size() == cOther.m_cSet.size() && IsSubsetOf( cOther );
	}
	bool operator!=( const Set& cOther ) const { return !( *this == cOther ); }

	/** The std::set in it */
	Container& GetContainer() { return m_cSet; }
	const Container& GetContainer() const { return m_cSet; }

	// The C++ library's names, for templates and range-for
	iterator begin() { return m_cSet.begin(); }
	iterator end() { return m_cSet.end(); }
	const_iterator begin() const { return m_cSet.begin(); }
	const_iterator end() const { return m_cSet.end(); }
	reverse_iterator rbegin() { return m_cSet.rbegin(); }
	reverse_iterator rend() { return m_cSet.rend(); }
	const_reverse_iterator rbegin() const { return m_cSet.rbegin(); }
	const_reverse_iterator rend() const { return m_cSet.rend(); }
	size_type size() const { return m_cSet.size(); }
	bool empty() const { return m_cSet.empty(); }

private:
	/* an empty set with this one's order */
	explicit Set( const Order& cOrder ) : m_cSet( cOrder ) {}
	Order Empty() const { return m_cSet.key_comp(); }

	Container m_cSet;
};

} // namespace os

#endif // __F_UTIL_SET_H__
