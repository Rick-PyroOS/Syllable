/*  Deque - os::Deque, a queue open at both ends.
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

#ifndef	__F_UTIL_DEQUE_H__
#define	__F_UTIL_DEQUE_H__

#include <deque>
#include <vector>
#include <algorithm>
#include <cstddef>

namespace os
{

/** A double-ended queue: push and pop at the front and at the back.
 * \ingroup util
 * \par Description:
 *	os::Deque is a std::deque with the names of the other libsyllable
 *	containers.  Pushing and popping at either end is O(1), reaching an
 *	item by its position (At(), operator[]) is O(1), and the items stay
 *	where they are in memory as it grows at the ends.
 * \code
 *	os::Deque<os::String> cHistory;
 *	cHistory.Push( "/home" ).Push( "/home/rick" ).PushFront( "/" );	// /, /home, /home/rick
 *	cHistory.PeekBack();		// "/home/rick", the newest
 *	cHistory.PeekFront();		// "/", the oldest
 *	if( cHistory.Count() > 2 )
 *	    cHistory.PopFront();	// forget the oldest
 *	os::String cPath;
 *	cHistory.Pop( &cPath );		// "/home/rick": back
 *	cHistory.Get( 5, "none" );	// "none": there is no item 5
 * \endcode
 *
 *	Push() (PushBack()) and Pop() (PopBack()) work at the back, as in
 *	os::Map and os::HashMap; PushFront() and PopFront() at the front.
 *	Pop(), PopFront(), First( &value ) and Last( &value ) return false
 *	on an empty deque.  PeekFront(), PeekBack(), At() and operator[]
 *	give the item itself, to read or change, and it must be there; Get()
 *	gives a default instead.  InsertAt() and RemoveAt() work in the
 *	middle, in O(n).  Rotate() turns the items round, as a carousel.
 *	ForEach() and begin()/end() go from the front to the back.
 *
 *	os::Deque is not thread-safe: guard it with an os::Locker to share
 *	it between threads.  Use os::Stack or os::Queue when only one end
 *	(or one end each way) is needed.
 */
template<class T>
class Deque
{
public:
	typedef std::deque<T> Container;
	typedef T value_type;
	typedef typename Container::iterator iterator;
	typedef typename Container::const_iterator const_iterator;
	typedef typename Container::reverse_iterator reverse_iterator;
	typedef typename Container::const_reverse_iterator const_reverse_iterator;
	typedef typename Container::size_type size_type;

	Deque() {}
	/** A deque of the items from cFirst to cLast, in that order */
	template<class I> Deque( I cFirst, I cLast ) : m_cItems( cFirst, cLast ) {}

	/** Puts cValue at the back.  \return the deque, to chain. */
	Deque& Push( const T& cValue )
	{
		m_cItems.push_back( cValue );
		return *this;
	}
	Deque& PushBack( const T& cValue ) { return Push( cValue ); }
	/** Puts cValue at the front.  \return the deque, to chain. */
	Deque& PushFront( const T& cValue )
	{
		m_cItems.push_front( cValue );
		return *this;
	}

	/** Takes the back item out, giving it to pcValue (when not NULL).
	 * \return false, changing nothing, if the deque is empty. */
	bool Pop( T* pcValue = NULL )
	{
		if( m_cItems.empty() )
			return false;
		if( pcValue != NULL )
			*pcValue = m_cItems.back();
		m_cItems.pop_back();
		return true;
	}
	bool PopBack( T* pcValue = NULL ) { return Pop( pcValue ); }
	/** Takes the front item out, giving it to pcValue (when not NULL).
	 * \return false, changing nothing, if the deque is empty. */
	bool PopFront( T* pcValue = NULL )
	{
		if( m_cItems.empty() )
			return false;
		if( pcValue != NULL )
			*pcValue = m_cItems.front();
		m_cItems.pop_front();
		return true;
	}

	/** The front item.  The deque must not be empty. */
	T& PeekFront() { return m_cItems.front(); }
	const T& PeekFront() const { return m_cItems.front(); }
	/** The back item.  The deque must not be empty. */
	T& PeekBack() { return m_cItems.back(); }
	const T& PeekBack() const { return m_cItems.back(); }

	/** Gives the front item to pcValue.  \return false if the deque is empty. */
	bool First( T* pcValue ) const
	{
		if( m_cItems.empty() )
			return false;
		*pcValue = m_cItems.front();
		return true;
	}
	/** Gives the back item to pcValue.  \return false if the deque is empty. */
	bool Last( T* pcValue ) const
	{
		if( m_cItems.empty() )
			return false;
		*pcValue = m_cItems.back();
		return true;
	}

	/** The item at nIndex (0 is the front).  It must be there. */
	T& At( int nIndex ) { return m_cItems[nIndex]; }
	const T& At( int nIndex ) const { return m_cItems[nIndex]; }
	T& operator[]( int nIndex ) { return m_cItems[nIndex]; }
	const T& operator[]( int nIndex ) const { return m_cItems[nIndex]; }
	/** The item at nIndex, or cDefault when there is none */
	T Get( int nIndex, const T& cDefault = T() ) const
	{
		return nIndex >= 0 && size_t( nIndex ) < m_cItems.size() ? m_cItems[nIndex] : cDefault;
	}

	/** Puts cValue at position nPos (0 is the front; past the end is the
	 *  back), moving the items after it along.  O(n).  \return the deque. */
	Deque& InsertAt( int nPos, const T& cValue )
	{
		size_t nAt = nPos < 0 ? 0 : std::min( size_t( nPos ), m_cItems.size() );
		m_cItems.insert( m_cItems.begin() + nAt, cValue );
		return *this;
	}
	/** Takes out the item at nIndex, giving it to pcValue (when not NULL).
	 *  O(n).  \return false if there is no item nIndex. */
	bool RemoveAt( int nIndex, T* pcValue = NULL )
	{
		if( nIndex < 0 || size_t( nIndex ) >= m_cItems.size() )
			return false;
		if( pcValue != NULL )
			*pcValue = m_cItems[nIndex];
		m_cItems.erase( m_cItems.begin() + nIndex );
		return true;
	}
	/** Removes the item an iterator is at.  \return the next one. */
	iterator Erase( iterator i ) { return m_cItems.erase( i ); }

	/** The position of the first item == cValue, or -1 */
	int IndexOf( const T& cValue ) const
	{
		const_iterator i = std::find( m_cItems.begin(), m_cItems.end(), cValue );
		return i == m_cItems.end() ? -1 : int( i - m_cItems.begin() );
	}
	bool Has( const T& cValue ) const { return IndexOf( cValue ) >= 0; }
	/** Removes the first item == cValue.  \return false if there is none. */
	bool Remove( const T& cValue ) { return RemoveAt( IndexOf( cValue ) ); }

	/** Removes the items bPred( item ) is true for, keeping the order of
	 *  the others.  \return how many it removed. */
	template<class P> int RemoveIf( P bPred )
	{
		size_t nBefore = m_cItems.size();
		m_cItems.erase( std::remove_if( m_cItems.begin(), m_cItems.end(), bPred ), m_cItems.end() );
		return int( nBefore - m_cItems.size() );
	}

	/** Calls cFunc( item ) for each item, from the front to the back */
	template<class F> void ForEach( F cFunc ) const
	{
		for( const_iterator i = m_cItems.begin(); i != m_cItems.end(); ++i )
			cFunc( *i );
	}
	template<class F> void ForEach( F cFunc )
	{
		for( iterator i = m_cItems.begin(); i != m_cItems.end(); ++i )
			cFunc( *i );
	}

	/** Turns the items round by nSteps: the front item goes to the back
	 *  nSteps times (with a negative nSteps, the back item to the front).
	 *  O(n). */
	void Rotate( int nSteps )
	{
		if( m_cItems.size() < 2 )
			return;
		int nSize = int( m_cItems.size() );
		int nFirst = nSteps % nSize;
		if( nFirst < 0 )
			nFirst += nSize;
		std::rotate( m_cItems.begin(), m_cItems.begin() + nFirst, m_cItems.end() );
	}
	/** Turns the order of the items around.  O(n). */
	void Reverse() { std::reverse( m_cItems.begin(), m_cItems.end() ); }

	/** The items, front to back */
	std::vector<T> Items() const { return std::vector<T>( m_cItems.begin(), m_cItems.end() ); }

	bool IsEmpty() const { return m_cItems.empty(); }
	/** How many items there are.  Count() is the same, as in os::Map. */
	int Size() const { return int( m_cItems.size() ); }
	int Count() const { return Size(); }
	void Clear() { m_cItems.clear(); }
	void Swap( Deque& cOther ) { m_cItems.swap( cOther.m_cItems ); }

	/** The same items in the same order */
	bool operator==( const Deque& cOther ) const { return m_cItems == cOther.m_cItems; }
	bool operator!=( const Deque& cOther ) const { return !( *this == cOther ); }

	/** The std::deque in it */
	Container& GetContainer() { return m_cItems; }
	const Container& GetContainer() const { return m_cItems; }

	// The C++ library's names, for templates and range-for: front to back
	iterator begin() { return m_cItems.begin(); }
	iterator end() { return m_cItems.end(); }
	const_iterator begin() const { return m_cItems.begin(); }
	const_iterator end() const { return m_cItems.end(); }
	reverse_iterator rbegin() { return m_cItems.rbegin(); }
	reverse_iterator rend() { return m_cItems.rend(); }
	const_reverse_iterator rbegin() const { return m_cItems.rbegin(); }
	const_reverse_iterator rend() const { return m_cItems.rend(); }
	size_type size() const { return m_cItems.size(); }
	bool empty() const { return m_cItems.empty(); }

private:
	Container m_cItems;
};

} // namespace os

#endif // __F_UTIL_DEQUE_H__
