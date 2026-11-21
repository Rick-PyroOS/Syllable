/*  Queue - os::Queue, first in, first out.
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

#ifndef	__F_UTIL_QUEUE_H__
#define	__F_UTIL_QUEUE_H__

#include <deque>
#include <cstddef>

namespace os
{

/** A queue: the first item pushed is the first popped.
 * \ingroup util
 * \par Description:
 *	os::Queue is a std::deque that you push to at the back and pop from
 *	at the front.  Push(), Pop() and Peek() are O(1), and the items stay
 *	where they are as the queue grows and shrinks.
 * \code
 *	os::Queue<os::String> cJobs;
 *	cJobs.Push( "print" ).Push( "scan" ).Push( "fax" );
 *	cJobs.Peek();			// "print", the next one out
 *	cJobs.PeekBack();		// "fax", the last one in
 *	os::String cJob;
 *	while( cJobs.Pop( &cJob ) )
 *	    printf( "doing %s\n", cJob.c_str() );	// print, scan, fax
 * \endcode
 *
 *	Pop() and Peek( &value ) return false on an empty queue, so a loop
 *	needs no IsEmpty().  Peek() and PeekBack() without an argument give
 *	the item itself (to read or change), and the queue must not be
 *	empty for them.  ForEach() and begin()/end() go from the front (the
 *	next Pop()) to the back.
 *
 *	os::Queue is not thread-safe: guard it with an os::Locker to share
 *	it between threads.  Use os::Stack for last in, first out.
 */
template<class T>
class Queue
{
public:
	typedef std::deque<T> Container;
	typedef T value_type;
	typedef typename Container::iterator iterator;
	typedef typename Container::const_iterator const_iterator;
	typedef typename Container::reverse_iterator reverse_iterator;
	typedef typename Container::const_reverse_iterator const_reverse_iterator;
	typedef typename Container::size_type size_type;

	Queue() {}

	/** Puts cValue at the back.  \return the queue, to chain Push()es. */
	Queue& Push( const T& cValue )
	{
		m_cItems.push_back( cValue );
		return *this;
	}

	/** Takes the front item out, giving it to pcValue (when not NULL).
	 * \return false, changing nothing, if the queue is empty. */
	bool Pop( T* pcValue = NULL )
	{
		if( m_cItems.empty() )
			return false;
		if( pcValue != NULL )
			*pcValue = m_cItems.front();
		m_cItems.pop_front();
		return true;
	}

	/** The front item, the next Pop().  The queue must not be empty. */
	T& Peek() { return m_cItems.front(); }
	const T& Peek() const { return m_cItems.front(); }

	/** Gives the front item to pcValue, leaving it in.
	 * \return false if the queue is empty. */
	bool Peek( T* pcValue ) const
	{
		if( m_cItems.empty() )
			return false;
		*pcValue = m_cItems.front();
		return true;
	}

	/** The back item, the last pushed.  The queue must not be empty. */
	T& PeekBack() { return m_cItems.back(); }
	const T& PeekBack() const { return m_cItems.back(); }

	bool IsEmpty() const { return m_cItems.empty(); }
	/** How many items there are.  Count() is the same, as in os::Map. */
	int Size() const { return int( m_cItems.size() ); }
	int Count() const { return Size(); }
	void Clear() { m_cItems.clear(); }
	void Swap( Queue& cOther ) { m_cItems.swap( cOther.m_cItems ); }

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

	/** The same items in the same order */
	bool operator==( const Queue& cOther ) const { return m_cItems == cOther.m_cItems; }
	bool operator!=( const Queue& cOther ) const { return !( *this == cOther ); }

	/** The std::deque in it (the front is its front) */
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

#endif // __F_UTIL_QUEUE_H__
