/*  Stack - os::Stack, last in, first out.
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

#ifndef	__F_UTIL_STACK_H__
#define	__F_UTIL_STACK_H__

#include <deque>
#include <cstddef>

namespace os
{

/** A stack: the last item pushed is the first popped.
 * \ingroup util
 * \par Description:
 *	os::Stack is a std::deque that you push to and pop from at one end,
 *	the top.  Push(), Pop() and Peek() are O(1), and the items stay
 *	where they are as the stack grows (a deque grows in blocks, so it
 *	does not copy them all the way a vector does).
 * \code
 *	os::Stack<os::String> cUndo;
 *	cUndo.Push( "type a" ).Push( "type b" ).Push( "delete" );
 *	cUndo.Peek();			// "delete"
 *	os::String cStep;
 *	while( cUndo.Pop( &cStep ) )
 *	    printf( "undo %s\n", cStep.c_str() );	// delete, type b, type a
 * \endcode
 *
 *	Pop() and Peek( &value ) return false on an empty stack, so a loop
 *	needs no IsEmpty().  Peek() without an argument gives the top item
 *	itself (to read or change), and the stack must not be empty for it.
 *	ForEach() and begin()/end() go from the bottom (the first pushed) to
 *	the top; ForEachFromTop() and rbegin()/rend() the other way.
 *
 *	Use os::Queue for first in, first out.
 */
template<class T>
class Stack
{
public:
	typedef std::deque<T> Container;
	typedef T value_type;
	typedef typename Container::iterator iterator;
	typedef typename Container::const_iterator const_iterator;
	typedef typename Container::reverse_iterator reverse_iterator;
	typedef typename Container::const_reverse_iterator const_reverse_iterator;
	typedef typename Container::size_type size_type;

	Stack() {}

	/** Puts cValue on the top.  \return the stack, to chain Push()es. */
	Stack& Push( const T& cValue )
	{
		m_cItems.push_back( cValue );
		return *this;
	}

	/** Takes the top item off, giving it to pcValue (when not NULL).
	 * \return false, changing nothing, if the stack is empty. */
	bool Pop( T* pcValue = NULL )
	{
		if( m_cItems.empty() )
			return false;
		if( pcValue != NULL )
			*pcValue = m_cItems.back();
		m_cItems.pop_back();
		return true;
	}

	/** The top item.  The stack must not be empty. */
	T& Peek() { return m_cItems.back(); }
	const T& Peek() const { return m_cItems.back(); }

	/** Gives the top item to pcValue, leaving it on.
	 * \return false if the stack is empty. */
	bool Peek( T* pcValue ) const
	{
		if( m_cItems.empty() )
			return false;
		*pcValue = m_cItems.back();
		return true;
	}

	bool IsEmpty() const { return m_cItems.empty(); }
	/** How many items there are.  Count() is the same, as in os::Map. */
	int Size() const { return int( m_cItems.size() ); }
	int Count() const { return Size(); }
	void Clear() { m_cItems.clear(); }
	void Swap( Stack& cOther ) { m_cItems.swap( cOther.m_cItems ); }

	/** Calls cFunc( item ) for each item, from the bottom to the top */
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
	/** Calls cFunc( item ) for each item, from the top (the next Pop()) down */
	template<class F> void ForEachFromTop( F cFunc ) const
	{
		for( const_reverse_iterator i = m_cItems.rbegin(); i != m_cItems.rend(); ++i )
			cFunc( *i );
	}
	template<class F> void ForEachFromTop( F cFunc )
	{
		for( reverse_iterator i = m_cItems.rbegin(); i != m_cItems.rend(); ++i )
			cFunc( *i );
	}

	/** The same items in the same order */
	bool operator==( const Stack& cOther ) const { return m_cItems == cOther.m_cItems; }
	bool operator!=( const Stack& cOther ) const { return !( *this == cOther ); }

	/** The std::deque in it (the top is its back) */
	Container& GetContainer() { return m_cItems; }
	const Container& GetContainer() const { return m_cItems; }

	// The C++ library's names, for templates and range-for: bottom to top
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

#endif // __F_UTIL_STACK_H__
