/*  RingBuffer - os::RingBuffer, a fixed-size queue.
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

#ifndef	__F_UTIL_RINGBUFFER_H__
#define	__F_UTIL_RINGBUFFER_H__

#include <vector>
#include <algorithm>
#include <iterator>
#include <cstddef>

namespace os
{

template<class T> class RingBuffer;

/** The iterators of os::RingBuffer: from the oldest item to the newest
 *  (R is a RingBuffer or a const one, V its T or a const T) */
template<class R, class V> class RingBufferIterator
{
public:
	typedef std::bidirectional_iterator_tag iterator_category;
	typedef V value_type;
	typedef ptrdiff_t difference_type;
	typedef V* pointer;
	typedef V& reference;

	RingBufferIterator() : m_pcBuffer( NULL ), m_nIndex( 0 ) {}
	RingBufferIterator( R* pcBuffer, int nIndex ) : m_pcBuffer( pcBuffer ), m_nIndex( nIndex ) {}
	/* An iterator converts to a const_iterator */
	template<class R2, class V2> RingBufferIterator( const RingBufferIterator<R2, V2>& cOther ) : m_pcBuffer( cOther.m_pcBuffer ), m_nIndex( cOther.m_nIndex ) {}

	V& operator*() const { return ( *m_pcBuffer )[m_nIndex]; }
	V* operator->() const { return &( *m_pcBuffer )[m_nIndex]; }
	RingBufferIterator& operator++() { ++m_nIndex; return *this; }
	RingBufferIterator operator++( int ) { RingBufferIterator cOld = *this; ++m_nIndex; return cOld; }
	RingBufferIterator& operator--() { --m_nIndex; return *this; }
	RingBufferIterator operator--( int ) { RingBufferIterator cOld = *this; --m_nIndex; return cOld; }
	template<class R2, class V2> bool operator==( const RingBufferIterator<R2, V2>& cOther ) const { return m_nIndex == cOther.m_nIndex; }
	template<class R2, class V2> bool operator!=( const RingBufferIterator<R2, V2>& cOther ) const { return m_nIndex != cOther.m_nIndex; }

	R* m_pcBuffer;
	int m_nIndex;		// 0 is the oldest item
};

/** A queue of a fixed size that never allocates once it's made.
 * \ingroup util
 * \par Description:
 *	os::RingBuffer holds up to GetCapacity() items in an array made once,
 *	going round it: Push() puts an item after the newest, Pop() takes
 *	out the oldest.  When it is full, Push() either overwrites the oldest
 *	item (OVERWRITE, the default: a log that keeps the last 1000 lines,
 *	the last second of audio) or refuses the new one (REJECT: input
 *	events, where the ones already waiting matter more).  Either way it
 *	counts what was lost, in GetDropped().  Push(), Pop() and At() are
 *	O(1), and Write() and Read() move many items at once, for samples.
 * \code
 *	os::RingBuffer<os::String> cLog( 1000 );		// the last 1000 lines
 *	cLog.Push( "boot" );
 *	for( int i = 0; i < cLog.Count(); i++ )
 *	    printf( "%s\n", cLog[i].c_str() );		// oldest first
 *
 *	os::RingBuffer<int16> cAudio( 44100 * 2 );		// a second of stereo
 *	cAudio.Write( pnSamples, nFrames * 2 );		// from the decoder
 *	int nGot = cAudio.Read( pnOut, 1024 );		// to the sound card
 *
 *	os::RingBuffer<os::Message> cEvents( 256, os::RingBuffer<os::Message>::REJECT );
 *	if( !cEvents.Push( cEvent ) )
 *	    dbprintf( "input queue full, %d events lost\n", cEvents.GetDropped() );
 * \endcode
 *
 *	Pop(), PopBack(), Peek( &value ) return false when it is empty.
 *	Peek() and PeekBack() give the oldest and newest item itself, At()
 *	and operator[] the one at a position (0 is the oldest); they must be
 *	there.  Discard() drops the oldest items.  SetCapacity() makes it
 *	bigger or smaller, keeping the newest items.  An item taken out is
 *	set to T(), so it lets go of what it holds at once.  ForEach() and
 *	begin()/end() go from the oldest to the newest.  Iterators stay valid
 *	until something is pushed or popped.
 *
 *	os::RingBuffer is not thread-safe: guard it with an os::Locker to
 *	share it between threads.  T needs a default constructor.  (For a
 *	byte FIFO that grows, see os::CircularBuffer.)
 */
template<class T>
class RingBuffer
{
public:
	/** What Push() and Write() do when the buffer is full */
	enum WhenFull
	{
		OVERWRITE,	///< the oldest items go to make room
		REJECT		///< the new items are not stored
	};
	typedef T value_type;
	typedef RingBufferIterator<RingBuffer, T> iterator;
	typedef RingBufferIterator<const RingBuffer, const T> const_iterator;
	typedef std::reverse_iterator<iterator> reverse_iterator;
	typedef std::reverse_iterator<const_iterator> const_reverse_iterator;
	typedef size_t size_type;

	/** A buffer for nCapacity items (at least 1) */
	explicit RingBuffer( int nCapacity, WhenFull eWhenFull = OVERWRITE )
		: m_cItems( std::max( nCapacity, 1 ) ), m_eWhenFull( eWhenFull ), m_nHead( 0 ), m_nCount( 0 ), m_nDropped( 0 ) {}

	/** Puts cValue after the newest item.  When it is full, OVERWRITE
	 *  drops the oldest item to make room, and REJECT drops cValue.
	 * \return false if cValue was not stored (REJECT and full). */
	bool Push( const T& cValue )
	{
		if( m_nCount == Capacity() )
		{
			m_nDropped++;
			if( m_eWhenFull == REJECT )
				return false;
			m_cItems[m_nHead] = cValue;		// over the oldest
			m_nHead = Wrap( m_nHead + 1 );
			return true;
		}
		m_cItems[Slot( m_nCount )] = cValue;
		m_nCount++;
		return true;
	}

	/** Pushes nCount items from pValues, in order, copying in at most two
	 *  runs.  \return how many it stored: all of them with OVERWRITE (if
	 *	there are more than fit, only the last GetCapacity() stay), up
	 *	to GetFree() with REJECT. */
	int Write( const T* pValues, int nCount )
	{
		if( nCount <= 0 )
			return 0;
		int nCapacity = Capacity();
		int nStored = nCount;
		if( m_eWhenFull == REJECT )
		{
			nStored = std::min( nCount, nCapacity - m_nCount );
			m_nDropped += nCount - nStored;
		}
		else if( nCount >= nCapacity )
		{
			// only the last nCapacity stay: start afresh with them
			m_nDropped += m_nCount + nCount - nCapacity;
			Release( 0, m_nCount );
			pValues += nCount - nCapacity;
			std::copy( pValues, pValues + nCapacity, m_cItems.begin() );
			m_nHead = 0;
			m_nCount = nCapacity;
			return nStored;
		}
		else if( m_nCount + nCount > nCapacity )
			Discard( m_nCount + nCount - nCapacity, true );
		CopyIn( pValues, nStored );
		return nStored;
	}

	/** Takes the oldest item out, giving it to pcValue (when not NULL).
	 * \return false, changing nothing, if it is empty. */
	bool Pop( T* pcValue = NULL )
	{
		if( m_nCount == 0 )
			return false;
		if( pcValue != NULL )
			*pcValue = m_cItems[m_nHead];
		m_cItems[m_nHead] = T();
		m_nHead = Wrap( m_nHead + 1 );
		m_nCount--;
		return true;
	}
	/** Takes the newest item out (undoing a Push()), giving it to pcValue
	 *  (when not NULL).  \return false if it is empty. */
	bool PopBack( T* pcValue = NULL )
	{
		if( m_nCount == 0 )
			return false;
		size_t nSlot = Slot( m_nCount - 1 );
		if( pcValue != NULL )
			*pcValue = m_cItems[nSlot];
		m_cItems[nSlot] = T();
		m_nCount--;
		return true;
	}

	/** Takes up to nCount of the oldest items out into pValues, copying
	 *  in at most two runs.  \return how many it took. */
	int Read( T* pValues, int nCount )
	{
		nCount = std::max( 0, std::min( nCount, m_nCount ) );
		int nFirstRun = std::min( nCount, Capacity() - m_nHead );
		std::copy( m_cItems.begin() + m_nHead, m_cItems.begin() + m_nHead + nFirstRun, pValues );
		std::copy( m_cItems.begin(), m_cItems.begin() + ( nCount - nFirstRun ), pValues + nFirstRun );
		Discard( nCount );
		return nCount;
	}

	/** Drops up to nCount of the oldest items.  \return how many it dropped
	 *  (they don't count in GetDropped(): you asked for it). */
	int Discard( int nCount ) { return Discard( nCount, false ); }

	/** The oldest item, the next Pop().  It must not be empty. */
	T& Peek() { return m_cItems[m_nHead]; }
	const T& Peek() const { return m_cItems[m_nHead]; }
	/** Gives the oldest item to pcValue.  \return false if it is empty. */
	bool Peek( T* pcValue ) const
	{
		if( m_nCount == 0 )
			return false;
		*pcValue = m_cItems[m_nHead];
		return true;
	}
	/** The newest item.  It must not be empty. */
	T& PeekBack() { return m_cItems[Slot( m_nCount - 1 )]; }
	const T& PeekBack() const { return m_cItems[Slot( m_nCount - 1 )]; }

	/** The item at nIndex (0 is the oldest).  It must be there. */
	T& At( int nIndex ) { return m_cItems[Slot( nIndex )]; }
	const T& At( int nIndex ) const { return m_cItems[Slot( nIndex )]; }
	T& operator[]( int nIndex ) { return m_cItems[Slot( nIndex )]; }
	const T& operator[]( int nIndex ) const { return m_cItems[Slot( nIndex )]; }
	/** The item at nIndex, or cDefault when there is none */
	T Get( int nIndex, const T& cDefault = T() ) const
	{
		return nIndex >= 0 && nIndex < m_nCount ? m_cItems[Slot( nIndex )] : cDefault;
	}

	/** Calls cFunc( item ) for each item, from the oldest to the newest */
	template<class F> void ForEach( F cFunc ) const
	{
		for( int i = 0; i < m_nCount; i++ )
			cFunc( m_cItems[Slot( i )] );
	}
	template<class F> void ForEach( F cFunc )
	{
		for( int i = 0; i < m_nCount; i++ )
			cFunc( m_cItems[Slot( i )] );
	}

	/** The items, from the oldest to the newest */
	std::vector<T> Items() const
	{
		std::vector<T> cItems;
		cItems.reserve( m_nCount );
		for( int i = 0; i < m_nCount; i++ )
			cItems.push_back( m_cItems[Slot( i )] );
		return cItems;
	}

	/** Makes room for nCapacity items (at least 1); if fewer than are in,
	 *  the newest stay.  This one allocates. */
	void SetCapacity( int nCapacity )
	{
		nCapacity = std::max( nCapacity, 1 );
		int nKeep = std::min( m_nCount, nCapacity );
		std::vector<T> cNew( nCapacity );
		for( int i = 0; i < nKeep; i++ )
			cNew[i] = m_cItems[Slot( m_nCount - nKeep + i )];
		m_nDropped += m_nCount - nKeep;
		m_cItems.swap( cNew );
		m_nHead = 0;
		m_nCount = nKeep;
	}
	int GetCapacity() const { return Capacity(); }
	WhenFull GetWhenFull() const { return m_eWhenFull; }
	void SetWhenFull( WhenFull eWhenFull ) { m_eWhenFull = eWhenFull; }

	/** How many items were lost since it was made (or ResetDropped()):
	 *  overwritten, rejected, or cut by SetCapacity() */
	int GetDropped() const { return m_nDropped; }
	void ResetDropped() { m_nDropped = 0; }

	bool IsEmpty() const { return m_nCount == 0; }
	bool IsFull() const { return m_nCount == Capacity(); }
	/** How many more items fit before it is full */
	int GetFree() const { return Capacity() - m_nCount; }
	/** How many items there are.  Count() is the same, as in os::Map. */
	int Size() const { return m_nCount; }
	int Count() const { return m_nCount; }
	/** Empties it (the capacity and GetDropped() stay) */
	void Clear()
	{
		Release( 0, m_nCount );
		m_nHead = 0;
		m_nCount = 0;
	}
	void Swap( RingBuffer& cOther )
	{
		m_cItems.swap( cOther.m_cItems );
		std::swap( m_eWhenFull, cOther.m_eWhenFull );
		std::swap( m_nHead, cOther.m_nHead );
		std::swap( m_nCount, cOther.m_nCount );
		std::swap( m_nDropped, cOther.m_nDropped );
	}

	/** The same items in the same order (the capacities may differ) */
	bool operator==( const RingBuffer& cOther ) const
	{
		if( m_nCount != cOther.m_nCount )
			return false;
		for( int i = 0; i < m_nCount; i++ )
			if( !( At( i ) == cOther.At( i ) ) )
				return false;
		return true;
	}
	bool operator!=( const RingBuffer& cOther ) const { return !( *this == cOther ); }

	// The C++ library's names, for templates and range-for: oldest to newest
	iterator begin() { return iterator( this, 0 ); }
	iterator end() { return iterator( this, m_nCount ); }
	const_iterator begin() const { return const_iterator( this, 0 ); }
	const_iterator end() const { return const_iterator( this, m_nCount ); }
	reverse_iterator rbegin() { return reverse_iterator( end() ); }
	reverse_iterator rend() { return reverse_iterator( begin() ); }
	const_reverse_iterator rbegin() const { return const_reverse_iterator( end() ); }
	const_reverse_iterator rend() const { return const_reverse_iterator( begin() ); }
	size_type size() const { return size_type( m_nCount ); }
	bool empty() const { return m_nCount == 0; }

private:
	int Capacity() const { return int( m_cItems.size() ); }
	/* nIndex less than twice the capacity, back into it */
	int Wrap( int nIndex ) const { return nIndex >= Capacity() ? nIndex - Capacity() : nIndex; }
	/* the array slot of the item at nIndex (0 is the oldest) */
	size_t Slot( int nIndex ) const { return size_t( Wrap( m_nHead + nIndex ) ); }

	/* sets the items nFrom to nFrom + nCount (by index) to T() */
	void Release( int nFrom, int nCount )
	{
		for( int i = nFrom; i < nFrom + nCount; i++ )
			m_cItems[Slot( i )] = T();
	}
	int Discard( int nCount, bool bLost )
	{
		nCount = std::max( 0, std::min( nCount, m_nCount ) );
		Release( 0, nCount );
		m_nHead = Wrap( m_nHead + nCount );
		m_nCount -= nCount;
		if( bLost )
			m_nDropped += nCount;
		return nCount;
	}
	/* copies nCount items in after the newest; they fit */
	void CopyIn( const T* pValues, int nCount )
	{
		int nTail = int( Slot( m_nCount ) );
		int nFirstRun = std::min( nCount, Capacity() - nTail );
		std::copy( pValues, pValues + nFirstRun, m_cItems.begin() + nTail );
		std::copy( pValues + nFirstRun, pValues + nCount, m_cItems.begin() );
		m_nCount += nCount;
	}

	std::vector<T> m_cItems;	// the ring: m_nCount items from m_nHead, going round
	WhenFull m_eWhenFull;
	int m_nHead;			// the slot of the oldest item
	int m_nCount;
	int m_nDropped;
};

} // namespace os

#endif // __F_UTIL_RINGBUFFER_H__
