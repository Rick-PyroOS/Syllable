/*  HashSet - a hash set that keeps the order of its keys.
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

#ifndef	__F_UTIL_HASHSET_H__
#define	__F_UTIL_HASHSET_H__

#include <util/hashmap.h>
#include <vector>
#include <iterator>
#include <stddef.h>

namespace os
{

namespace hashset_private
{
/* The value of a HashSet's HashMap: nothing */
struct None
{
	bool operator==( const None& ) const { return true; }
};
} // namespace hashset_private

/** The iterator of os::HashSet: goes through the keys, in order */
template<class K, class H> class HashSetIterator
{
public:
	typedef typename HashMap<K, hashset_private::None, H>::const_iterator MapIterator;
	typedef std::bidirectional_iterator_tag iterator_category;
	typedef K value_type;
	typedef ptrdiff_t difference_type;
	typedef const K* pointer;
	typedef const K& reference;

	HashSetIterator() {}
	explicit HashSetIterator( const MapIterator& cPos ) : m_cPos( cPos ) {}

	const K& operator*() const { return m_cPos->first; }
	const K* operator->() const { return &m_cPos->first; }
	HashSetIterator& operator++() { ++m_cPos; return *this; }
	HashSetIterator operator++( int ) { HashSetIterator cOld = *this; ++m_cPos; return cOld; }
	HashSetIterator& operator--() { --m_cPos; return *this; }
	HashSetIterator operator--( int ) { HashSetIterator cOld = *this; --m_cPos; return cOld; }
	bool operator==( const HashSetIterator& cOther ) const { return m_cPos == cOther.m_cPos; }
	bool operator!=( const HashSetIterator& cOther ) const { return m_cPos != cOther.m_cPos; }

	MapIterator m_cPos;
};

/** A hash set: keys, each in it once, that keeps the order of its keys.
 * \ingroup util
 * \par Description:
 *	os::HashSet is os::HashMap without the values (it is one, with values
 *	of nothing).  Finding, adding and removing a key take about the same
 *	time however big it is, and the keys stay in an order you choose: the
 *	order they were added in unless you move them with Push(),
 *	PushFront(), InsertAt(), Reverse() or Sort().  Iterating goes
 *	through them in that order.
 * \code
 *	os::HashSet<os::String> cSeen;
 *	cSeen.Add( "b.png" ).Add( "a.png" ).Add( "b.png" );	// b.png, a.png
 *	if( !cSeen.Insert( "c.png" ) )		// true: it was new
 *	    printf( "seen before\n" );
 *	cSeen.Push( "b.png" );			// moves it last: a.png, c.png, b.png
 *	cSeen.Sort();				// a.png, b.png, c.png
 *	os::HashSet<os::String> cWanted;
 *	cWanted.Add( "c.png" ).Add( "d.png" );
 *	cWanted.Difference( cSeen );		// d.png: wanted, not seen yet
 *	for( os::HashSet<os::String>::const_iterator i = cSeen.begin(); i != cSeen.end(); ++i )
 *	    printf( "%s\n", i->c_str() );
 * \endcode
 *
 *	Add() adds a key at the end and chains; Insert() says whether it was
 *	new.  Push() puts the key at the end and PushFront() at the start,
 *	moving it there if it's in.  Pop() and PopFront() take the last and
 *	first keys out.  Union(), Intersection(), Difference() and
 *	SymmetricDifference() make a new set (this set's keys first, in its
 *	order); Merge(), Intersect() and Subtract() change this one.  The
 *	costs are os::HashMap's: finding, Add(), Push(), Remove(), Pop() and
 *	PopFront() are O(1) on average, PushFront(), InsertAt(), Reverse() and
 *	Sort() O(n) or O(n log n).  Adding a key can move the others in
 *	memory, so it invalidates iterators and the pointers Find() gives.
 *
 *	K needs operator==, an os::Hash (or a hash class H) and a default
 *	constructor.
 */
template<class K, class H = Hash<K> >
class HashSet
{
public:
	typedef bool (*KeyCompare)( const K&, const K& );
	typedef HashMap<K, hashset_private::None, H> Container;
	typedef K key_type;
	typedef K value_type;
	typedef HashSetIterator<K, H> const_iterator;
	typedef const_iterator iterator;	// the keys can't change in place
	typedef std::reverse_iterator<const_iterator> const_reverse_iterator;
	typedef const_reverse_iterator reverse_iterator;
	typedef size_t size_type;

	HashSet() {}
	explicit HashSet( const H& cHash ) : m_cMap( cHash ) {}
	/** A set of the keys from cFirst to cLast, in that order */
	template<class I> HashSet( I cFirst, I cLast )
	{
		for( ; cFirst != cLast; ++cFirst )
			Add( *cFirst );
	}

	/** Adds the key at the end if it's not in yet.  \return true if it was added. */
	bool Insert( const K& cKey ) { return m_cMap.Insert( cKey, None() ); }

	/** Adds the key at the end (it stays where it is if it's in).
	 * \return the set, to chain Add()s. */
	HashSet& Add( const K& cKey )
	{
		m_cMap.Insert( cKey, None() );
		return *this;
	}
	/** Puts the key at the end, moving it there if it's in */
	HashSet& Push( const K& cKey )
	{
		m_cMap.Push( cKey, None() );
		return *this;
	}
	/** Puts the key at the start, moving it there if it's in.  O(n). */
	HashSet& PushFront( const K& cKey )
	{
		m_cMap.PushFront( cKey, None() );
		return *this;
	}
	/** Puts the key at position nPos (0 is the start; past the end is the
	 *  end), moving it there if it's in.  O(n). */
	HashSet& InsertAt( int nPos, const K& cKey )
	{
		m_cMap.InsertAt( nPos, cKey, None() );
		return *this;
	}

	bool Has( const K& cKey ) const { return m_cMap.Has( cKey ); }

	/** The key in the set that is equal to cKey, or NULL if none is */
	const K* Find( const K& cKey ) const
	{
		return m_cMap.FindKey( cKey );
	}

	/** Removes a key.  \return false if it was not in. */
	bool Remove( const K& cKey ) { return m_cMap.Remove( cKey ); }

	/** Removes a key, giving the one that was in to pcKey (when not NULL).
	 * \return false if it was not in. */
	bool Take( const K& cKey, K* pcKey )
	{
		if( pcKey != NULL )
		{
			const K* pcIn = Find( cKey );
			if( pcIn == NULL )
				return false;
			*pcKey = *pcIn;
		}
		return m_cMap.Remove( cKey );
	}

	/** Removes the key an iterator is at.  \return the next one.  The
	 *  other iterators stay valid. */
	iterator Erase( iterator i )
	{
		typedef typename Container::Entry Entry;
		typename Container::iterator cPos( const_cast<Entry*>( i.m_cPos.m_pBegin ), const_cast<Entry*>( i.m_cPos.m_pPos ), const_cast<Entry*>( i.m_cPos.m_pEnd ) );
		return iterator( typename Container::const_iterator( m_cMap.Erase( cPos ) ) );
	}

	/** Removes the keys bPred( key ) is true for, keeping the order of the
	 *  others.  \return how many it removed. */
	template<class P> int RemoveIf( P bPred ) { return m_cMap.RemoveIf( KeyPred<P>( bPred ) ); }

	/** Calls cFunc( key ) for each key, in order */
	template<class F> void ForEach( F cFunc ) const
	{
		for( const_iterator i = begin(); i != end(); ++i )
			cFunc( *i );
	}

	/** The first key.  \return false if the set is empty. */
	bool First( K* pcKey ) const { return m_cMap.First( pcKey ); }
	/** The last key.  \return false if the set is empty. */
	bool Last( K* pcKey ) const { return m_cMap.Last( pcKey ); }
	/** Takes the last key out.  \return false if the set is empty. */
	bool Pop( K* pcKey = NULL ) { return m_cMap.Pop( pcKey ); }
	/** Takes the first key out.  \return false if the set is empty. */
	bool PopFront( K* pcKey = NULL ) { return m_cMap.PopFront( pcKey ); }

	/** The key at position nIndex (0 to Count() - 1) */
	const K& KeyAt( int nIndex ) const { return m_cMap.KeyAt( nIndex ); }
	/** The position of a key, or -1 if it's not in */
	int IndexOf( const K& cKey ) const { return m_cMap.IndexOf( cKey ); }

	/** Turns the order of the keys around.  O(n). */
	void Reverse() { m_cMap.Reverse(); }
	/** Sorts the keys with pfCompare (operator< for NULL), ascending or
	 *  descending.  O(n log n).  Keys added later go at the end. */
	void Sort( KeyCompare pfCompare = NULL, bool bDescending = false ) { m_cMap.Sort( pfCompare, bDescending ); }

	/** The keys, in order */
	std::vector<K> Keys() const { return m_cMap.Keys(); }

	/** Adds the keys of another set, in its order, at the end.  \return this set. */
	HashSet& Merge( const HashSet& cOther )
	{
		m_cMap.Merge( cOther.m_cMap, false );
		return *this;
	}
	/** Keeps only the keys that are in cOther too.  \return this set. */
	HashSet& Intersect( const HashSet& cOther )
	{
		m_cMap.RemoveIf( NotIn( cOther, false ) );
		return *this;
	}
	/** Removes the keys that are in cOther.  \return this set. */
	HashSet& Subtract( const HashSet& cOther )
	{
		if( cOther.Count() < Count() )
		{
			for( const_iterator i = cOther.begin(); i != cOther.end(); ++i )
				m_cMap.Remove( *i );
		}
		else
			m_cMap.RemoveIf( NotIn( cOther, true ) );
		return *this;
	}

	/** The keys in either set: this set's, then cOther's new ones */
	HashSet Union( const HashSet& cOther ) const { HashSet cNew( *this ); return cNew.Merge( cOther ); }
	/** The keys in both sets, in this set's order */
	HashSet Intersection( const HashSet& cOther ) const { HashSet cNew( *this ); return cNew.Intersect( cOther ); }
	/** The keys in this set and not in cOther, in this set's order */
	HashSet Difference( const HashSet& cOther ) const { HashSet cNew( *this ); return cNew.Subtract( cOther ); }
	/** The keys in one of the sets but not both: this set's, then cOther's */
	HashSet SymmetricDifference( const HashSet& cOther ) const
	{
		HashSet cNew( Difference( cOther ) );
		for( const_iterator i = cOther.begin(); i != cOther.end(); ++i )
			if( !Has( *i ) )
				cNew.Add( *i );
		return cNew;
	}

	/** Whether every key of this set is in cOther */
	bool IsSubsetOf( const HashSet& cOther ) const
	{
		if( Count() > cOther.Count() )
			return false;
		for( const_iterator i = begin(); i != end(); ++i )
			if( !cOther.Has( *i ) )
				return false;
		return true;
	}
	/** Whether every key of cOther is in this set */
	bool IsSupersetOf( const HashSet& cOther ) const { return cOther.IsSubsetOf( *this ); }
	/** Whether the sets have a key in common */
	bool Intersects( const HashSet& cOther ) const
	{
		const HashSet& cSmall = Count() <= cOther.Count() ? *this : cOther;
		const HashSet& cLarge = &cSmall == this ? cOther : *this;
		for( const_iterator i = cSmall.begin(); i != cSmall.end(); ++i )
			if( cLarge.Has( *i ) )
				return true;
		return false;
	}

	/** Makes room for nCount keys without growing the index */
	void Reserve( size_t nCount ) { m_cMap.Reserve( nCount ); }
	int Count() const { return m_cMap.Count(); }
	bool IsEmpty() const { return m_cMap.IsEmpty(); }
	void Clear() { m_cMap.Clear(); }
	void Swap( HashSet& cOther ) { m_cMap.Swap( cOther.m_cMap ); }
	/** The size of the index (a power of two) */
	size_t GetCapacity() const { return m_cMap.GetCapacity(); }

	/** The same keys (the order doesn't matter; compare Keys() for that) */
	bool operator==( const HashSet& cOther ) const { return m_cMap == cOther.m_cMap; }
	bool operator!=( const HashSet& cOther ) const { return !( *this == cOther ); }

	const_iterator begin() const { return const_iterator( m_cMap.begin() ); }
	const_iterator end() const { return const_iterator( m_cMap.end() ); }
	const_reverse_iterator rbegin() const { return const_reverse_iterator( end() ); }
	const_reverse_iterator rend() const { return const_reverse_iterator( begin() ); }
	size_type size() const { return m_cMap.size(); }
	bool empty() const { return m_cMap.empty(); }

private:
	typedef hashset_private::None None;

	template<class P> struct KeyPred
	{
		explicit KeyPred( P bPred ) : m_bPred( bPred ) {}
		bool operator()( const K& cKey, const None& ) { return m_bPred( cKey ); }
		P m_bPred;
	};
	/* true for keys not in cSet (or, with bIn, for keys in it) */
	struct NotIn
	{
		NotIn( const HashSet& cSet, bool bIn ) : m_pcSet( &cSet ), m_bIn( bIn ) {}
		bool operator()( const K& cKey, const None& ) const { return m_pcSet->Has( cKey ) == m_bIn; }
		const HashSet* m_pcSet;
		bool m_bIn;
	};

	Container m_cMap;
};

} // namespace os

#endif // __F_UTIL_HASHSET_H__
