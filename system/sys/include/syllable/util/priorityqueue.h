/*  PriorityQueue - os::PriorityQueue, a binary heap.
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

#ifndef	__F_UTIL_PRIORITYQUEUE_H__
#define	__F_UTIL_PRIORITYQUEUE_H__

#include <vector>
#include <algorithm>
#include <functional>
#include <cstddef>

namespace os
{

/** A queue that gives out the first item in an order: a binary heap.
 * \ingroup util
 * \par Description:
 *	os::PriorityQueue keeps its items so that Pop() always gives the one
 *	that comes first by Compare: the smallest by operator< unless you
 *	give it another order (std::greater<T> for the largest first).  It
 *	suits timers and schedulers, where the next thing to do is the one
 *	due soonest.  Push() and Pop() are O(log n), Peek() is O(1).
 *
 *	Items that are equal by Compare come out in the order they went in,
 *	so two timers due at the same time fire in the order they were set,
 *	and threads of the same priority take turns.  (std::priority_queue
 *	makes no such promise.)
 * \code
 *	struct Timer
 *	{
 *	    bigtime_t m_nDue; int m_nID;
 *	    bool operator<( const Timer& cOther ) const { return m_nDue < cOther.m_nDue; }
 *	    bool operator==( const Timer& cOther ) const { return m_nID == cOther.m_nID; }
 *	};
 *	struct Due
 *	{
 *	    bigtime_t m_nNow;
 *	    bool operator()( const Timer& cTimer ) const { return cTimer.m_nDue <= m_nNow; }
 *	};
 *
 *	os::PriorityQueue<Timer> cTimers;
 *	cTimers.Push( cBlink ).Push( cAutosave ).Push( cTooltip );
 *	Due cDue = { get_system_time() };
 *	Timer cTimer;
 *	while( cTimers.PopIf( cDue, &cTimer ) )	// every timer that is due, soonest first
 *	    Fire( cTimer );
 *	if( !cTimers.IsEmpty() )
 *	    snooze( cTimers.Peek().m_nDue - cDue.m_nNow );
 *	cTimers.Remove( cTooltip );			// cancelled
 * \endcode
 *
 *	Pop(), Peek( &value ) and PopIf() return false when there is nothing
 *	to give.  Peek() without an argument gives the first item to read;
 *	there is no changing it in place, which would break the heap (Remove()
 *	it and Push() it again).  Remove() and RemoveIf() take items out from
 *	anywhere in O(n).  ForEach() visits the items in no particular
 *	order; Sorted() lists them in the order Pop() would give them.
 *
 *	os::PriorityQueue is not thread-safe: guard it with an os::Locker to
 *	share it between threads.
 */
template<class T, class Compare = std::less<T> >
class PriorityQueue
{
public:
	typedef T value_type;
	typedef Compare value_compare;
	typedef size_t size_type;

	explicit PriorityQueue( const Compare& cCompare = Compare() ) : m_cCompare( cCompare ), m_nNextOrder( 0 ) {}

	/** Adds cValue.  \return the queue, to chain Push()es. */
	PriorityQueue& Push( const T& cValue )
	{
		if( m_nNextOrder == size_t( -1 ) )
			Renumber();
		m_cHeap.push_back( Entry( cValue, m_nNextOrder++ ) );
		SiftUp( m_cHeap.size() - 1 );
		return *this;
	}

	/** Takes the first item out, giving it to pcValue (when not NULL).
	 * \return false, changing nothing, if the queue is empty. */
	bool Pop( T* pcValue = NULL )
	{
		if( m_cHeap.empty() )
			return false;
		if( pcValue != NULL )
			*pcValue = m_cHeap[0].m_cValue;
		RemoveAt( 0 );
		return true;
	}

	/** Takes the first item out if bPred( item ) is true for it: for
	 *  timers, "is it due?", in a loop that pops all that are.
	 * \return false, changing nothing, if the queue is empty or bPred is
	 *	false for the first item. */
	template<class P> bool PopIf( P bPred, T* pcValue = NULL )
	{
		if( m_cHeap.empty() || !bPred( m_cHeap[0].m_cValue ) )
			return false;
		return Pop( pcValue );
	}

	/** The first item, the next Pop().  The queue must not be empty. */
	const T& Peek() const { return m_cHeap[0].m_cValue; }

	/** Gives the first item to pcValue, leaving it in.
	 * \return false if the queue is empty. */
	bool Peek( T* pcValue ) const
	{
		if( m_cHeap.empty() )
			return false;
		*pcValue = m_cHeap[0].m_cValue;
		return true;
	}

	/** Whether an item == cValue is in, in O(n) */
	bool Has( const T& cValue ) const { return IndexOf( cValue ) != size_t( -1 ); }

	/** Takes out an item == cValue (the first pushed, if there are more).
	 * \return false if there is none. */
	bool Remove( const T& cValue )
	{
		size_t nIndex = IndexOf( cValue );
		if( nIndex == size_t( -1 ) )
			return false;
		RemoveAt( nIndex );
		return true;
	}

	/** Takes out the items bPred( item ) is true for, in O(n).
	 * \return how many it took out. */
	template<class P> int RemoveIf( P bPred )
	{
		size_t nKept = 0;
		for( size_t i = 0; i < m_cHeap.size(); i++ )
		{
			if( !bPred( m_cHeap[i].m_cValue ) )
			{
				if( nKept != i )
					m_cHeap[nKept] = m_cHeap[i];
				nKept++;
			}
		}
		int nRemoved = int( m_cHeap.size() - nKept );
		if( nRemoved > 0 )
		{
			m_cHeap.erase( m_cHeap.begin() + nKept, m_cHeap.end() );
			for( size_t i = m_cHeap.size() / 2; i-- > 0; )
				SiftDown( i );
		}
		return nRemoved;
	}

	/** Calls cFunc( item ) for each item, in no particular order */
	template<class F> void ForEach( F cFunc ) const
	{
		for( size_t i = 0; i < m_cHeap.size(); i++ )
			cFunc( m_cHeap[i].m_cValue );
	}

	/** The items in the order Pop() would give them, in O(n log n) */
	std::vector<T> Sorted() const
	{
		std::vector<Entry> cEntries( m_cHeap );
		std::sort( cEntries.begin(), cEntries.end(), EntryLess( m_cCompare ) );
		std::vector<T> cItems;
		cItems.reserve( cEntries.size() );
		for( size_t i = 0; i < cEntries.size(); i++ )
			cItems.push_back( cEntries[i].m_cValue );
		return cItems;
	}

	bool IsEmpty() const { return m_cHeap.empty(); }
	/** How many items there are.  Count() is the same, as in os::Map. */
	int Size() const { return int( m_cHeap.size() ); }
	int Count() const { return Size(); }
	void Clear() { m_cHeap.clear(); m_nNextOrder = 0; }
	/** Makes room for nCount items, so Push() does not reallocate */
	void Reserve( size_t nCount ) { m_cHeap.reserve( nCount ); }
	void Swap( PriorityQueue& cOther )
	{
		m_cHeap.swap( cOther.m_cHeap );
		std::swap( m_cCompare, cOther.m_cCompare );
		std::swap( m_nNextOrder, cOther.m_nNextOrder );
	}
	const Compare& GetCompare() const { return m_cCompare; }

	// The C++ library's names, for templates
	size_type size() const { return m_cHeap.size(); }
	bool empty() const { return m_cHeap.empty(); }

private:
	/* An item and when it was pushed, to keep equal items in order */
	struct Entry
	{
		Entry( const T& cValue, size_t nOrder ) : m_cValue( cValue ), m_nOrder( nOrder ) {}
		T m_cValue;
		size_t m_nOrder;
	};
	/* a before b: a comes first by Compare, or neither does and a went in first */
	struct EntryLess
	{
		explicit EntryLess( const Compare& cCompare ) : m_cCompare( cCompare ) {}
		bool operator()( const Entry& cA, const Entry& cB ) const
		{
			if( m_cCompare( cA.m_cValue, cB.m_cValue ) )
				return true;
			if( m_cCompare( cB.m_cValue, cA.m_cValue ) )
				return false;
			return cA.m_nOrder < cB.m_nOrder;
		}
		Compare m_cCompare;
	};

	bool Before( size_t a, size_t b ) const { return EntryLess( m_cCompare )( m_cHeap[a], m_cHeap[b] ); }

	void SiftUp( size_t i )
	{
		while( i > 0 )
		{
			size_t nParent = ( i - 1 ) / 2;
			if( !Before( i, nParent ) )
				break;
			std::swap( m_cHeap[i], m_cHeap[nParent] );
			i = nParent;
		}
	}
	void SiftDown( size_t i )
	{
		size_t nSize = m_cHeap.size();
		for( ;; )
		{
			size_t nFirst = i, nLeft = 2 * i + 1, nRight = nLeft + 1;
			if( nLeft < nSize && Before( nLeft, nFirst ) )
				nFirst = nLeft;
			if( nRight < nSize && Before( nRight, nFirst ) )
				nFirst = nRight;
			if( nFirst == i )
				break;
			std::swap( m_cHeap[i], m_cHeap[nFirst] );
			i = nFirst;
		}
	}
	/* The last item takes the place of the one at nIndex, then moves to
	 * where it belongs: down, or up when it came from another branch. */
	void RemoveAt( size_t nIndex )
	{
		size_t nLast = m_cHeap.size() - 1;
		if( nIndex != nLast )
		{
			std::swap( m_cHeap[nIndex], m_cHeap[nLast] );
			m_cHeap.pop_back();
			SiftDown( nIndex );
			SiftUp( nIndex );
		}
		else
			m_cHeap.pop_back();
	}
	size_t IndexOf( const T& cValue ) const
	{
		size_t nFound = size_t( -1 );
		for( size_t i = 0; i < m_cHeap.size(); i++ )
			if( m_cHeap[i].m_cValue == cValue && ( nFound == size_t( -1 ) || m_cHeap[i].m_nOrder < m_cHeap[nFound].m_nOrder ) )
				nFound = i;
		return nFound;
	}
	/* The push counter ran out (after 4 billion Push()es on 32-bit):
	 * number the items again from 0, in the order they will come out.
	 * A sorted array is a heap, so it stays one. */
	void Renumber()
	{
		std::sort( m_cHeap.begin(), m_cHeap.end(), EntryLess( m_cCompare ) );
		for( size_t i = 0; i < m_cHeap.size(); i++ )
			m_cHeap[i].m_nOrder = i;
		m_nNextOrder = m_cHeap.size();
	}

	std::vector<Entry> m_cHeap;
	Compare m_cCompare;
	size_t m_nNextOrder;

	friend class PriorityQueueTest;
};

} // namespace os

#endif // __F_UTIL_PRIORITYQUEUE_H__
