/*  Concurrent - os::ConcurrentQueue and os::ConcurrentRingBuffer, os::Queue
 *  and os::RingBuffer that threads can share.
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

#ifndef	__F_UTIL_CONCURRENT_H__
#define	__F_UTIL_CONCURRENT_H__

#include <util/locker.h>
#include <util/queue.h>
#include <util/ringbuffer.h>
#include <atheos/semaphore.h>
#include <atheos/time.h>
#include <errno.h>
#include <vector>

namespace os
{

namespace concurrent_private
{
/* A counting semaphore with a token for each item waiting to be taken:
 * a thread that wants an item takes a token first, sleeping until there
 * is one.  There can be a few tokens more than items (after Clear(), or
 * from Close()), never fewer; a thread that gets a token and finds no item
 * waits again. */
class Tokens
{
public:
	explicit Tokens( const char* pzName ) : m_hSema( create_semaphore( pzName, 0, 0 ) ) {}
	~Tokens() { delete_semaphore( m_hSema ); }

	void Post( int nCount = 1 )
	{
		if( nCount > 0 )
			unlock_semaphore_x( m_hSema, nCount, 0 );
	}
	/* Takes a token, waiting until nDeadline (INFINITE_TIMEOUT: for
	 * ever).  \return false if the time ran out. */
	bool Take( bigtime_t nDeadline )
	{
		for( ;; )
		{
			bigtime_t nWait = INFINITE_TIMEOUT;
			if( nDeadline != INFINITE_TIMEOUT )
			{
				nWait = nDeadline - get_system_time();
				if( nWait < 0 )
					nWait = 0;
			}
			if( lock_semaphore_x( m_hSema, 1, 0, nWait ) >= 0 )
				return true;
			if( errno != EINTR )
				return false;
		}
	}
	/* Takes up to nCount tokens that are there, without waiting */
	void Drain( int nCount )
	{
		while( nCount-- > 0 && lock_semaphore_x( m_hSema, 1, 0, 0 ) >= 0 )
			;
	}
	/* When a wait of nTimeout from now ends */
	static bigtime_t Deadline( bigtime_t nTimeout )
	{
		if( nTimeout == INFINITE_TIMEOUT )
			return INFINITE_TIMEOUT;
		if( nTimeout < 0 )
			nTimeout = 0;
		bigtime_t nNow = get_system_time();
		return nTimeout > INFINITE_TIMEOUT - nNow ? INFINITE_TIMEOUT : nNow + nTimeout;
	}

private:
	Tokens( const Tokens& );
	Tokens& operator=( const Tokens& );

	sem_id m_hSema;
};
} // namespace concurrent_private

/** An os::Queue that threads can share, with a Pop() that waits.
 * \ingroup util
 * \par Description:
 *	os::ConcurrentQueue is first in, first out, guarded by an os::Locker,
 *	for handing work from threads that make it to threads that do it.
 *	Push() never waits.  Pop() waits until there is an item, or until
 *	its timeout; TryPop() doesn't wait.  Close() says nothing more is
 *	coming: Push() refuses from then on, and Pop() gives what is left and
 *	then returns false at once, so the threads waiting on it can end.
 * \code
 *	os::ConcurrentQueue<Job> cJobs;
 *
 *	// the worker threads
 *	Job cJob;
 *	while( cJobs.Pop( &cJob ) )		// waits; false once closed and empty
 *	    cJob.Run();
 *
 *	// the main thread
 *	cJobs.Push( cScan );
 *	cJobs.Push( cIndex );
 *	cJobs.Close();				// the workers finish what's queued, then stop
 * \endcode
 *
 *	Push() returns false (and stores nothing) once the queue is closed, so
 *	it doesn't chain.  Timeouts are in microseconds, as everywhere in
 *	Syllable: Pop( &cJob, 100000 ) waits a tenth of a second at most.
 *	TakeAll() empties it into a vector in one go, for a thread that
 *	handles everything waiting at once.  Size(), IsEmpty() and Peek() say
 *	how things were when they looked; another thread may change it
 *	right after.  For several steps that must go together, Lock() and
 *	Unlock() it (the lock is recursive).
 *
 *	Don't destroy it while threads are waiting in Pop(): Close() it, and
 *	let them return first.
 */
template<class T>
class ConcurrentQueue
{
public:
	explicit ConcurrentQueue( const char* pzName = "concurrent_queue" )
		: m_cLock( pzName ), m_cTokens( pzName ), m_bClosed( false ) {}

	/** Puts cValue at the back, and wakes one thread waiting in Pop().
	 * \return false, storing nothing, if the queue is closed. */
	bool Push( const T& cValue )
	{
		{
			AutoLocker cGuard( &m_cLock );
			if( m_bClosed )
				return false;
			m_cItems.Push( cValue );
		}
		m_cTokens.Post();
		return true;
	}

	/** Takes the front item out, giving it to pcValue (when not NULL),
	 *  waiting up to nTimeout microseconds for one (INFINITE_TIMEOUT: for
	 *  as long as it takes).
	 * \return false if the time ran out, or the queue is closed and
	 *	empty. */
	bool Pop( T* pcValue = NULL, bigtime_t nTimeout = INFINITE_TIMEOUT )
	{
		const bigtime_t nDeadline = concurrent_private::Tokens::Deadline( nTimeout );
		for( ;; )
		{
			if( IsClosedAndEmpty() )
				return false;
			if( !m_cTokens.Take( nDeadline ) )
				return false;
			AutoLocker cGuard( &m_cLock );
			if( m_cItems.Pop( pcValue ) )
				return true;
			if( m_bClosed )
			{
				m_cTokens.Post();	// wake the next waiter, so it sees it too
				return false;
			}
			// a token with no item (Clear() was quicker): wait again
		}
	}
	/** Pop() without waiting */
	bool TryPop( T* pcValue = NULL ) { return Pop( pcValue, 0 ); }

	/** Takes every item out into pcItems (added at its end), without
	 *  waiting.  \return how many it took. */
	int TakeAll( std::vector<T>* pcItems )
	{
		int nCount;
		{
			AutoLocker cGuard( &m_cLock );
			nCount = m_cItems.Count();
			for( typename Queue<T>::iterator i = m_cItems.begin(); i != m_cItems.end(); ++i )
				pcItems->push_back( *i );
			m_cItems.Clear();
		}
		m_cTokens.Drain( nCount );
		return nCount;
	}

	/** Gives the front item to pcValue, leaving it in.
	 * \return false if the queue is empty. */
	bool Peek( T* pcValue ) const
	{
		AutoLocker cGuard( &m_cLock );
		return m_cItems.Peek( pcValue );
	}

	/** No more Push()es: Pop() gives what is left, then returns false.
	 *  Wakes the threads waiting in Pop(). */
	void Close()
	{
		{
			AutoLocker cGuard( &m_cLock );
			if( m_bClosed )
				return;
			m_bClosed = true;
		}
		m_cTokens.Post();	// one waiter wakes, and passes it on
	}
	bool IsClosed() const
	{
		AutoLocker cGuard( &m_cLock );
		return m_bClosed;
	}

	/** Empties it (a closed queue stays closed) */
	void Clear()
	{
		int nCount;
		{
			AutoLocker cGuard( &m_cLock );
			nCount = m_cItems.Count();
			m_cItems.Clear();
		}
		m_cTokens.Drain( nCount );
	}

	/** Calls cFunc( item ) for each item, front to back, with the queue
	 *  locked (cFunc must not wait on another thread that uses it) */
	template<class F> void ForEach( F cFunc ) const
	{
		AutoLocker cGuard( &m_cLock );
		m_cItems.ForEach( cFunc );
	}

	int Size() const
	{
		AutoLocker cGuard( &m_cLock );
		return m_cItems.Size();
	}
	int Count() const { return Size(); }
	bool IsEmpty() const { return Size() == 0; }

	/** Locks it, to do several things as one (Lock() and Unlock() pair up) */
	void Lock() const { m_cLock.Lock(); }
	void Unlock() const { m_cLock.Unlock(); }

private:
	ConcurrentQueue( const ConcurrentQueue& );
	ConcurrentQueue& operator=( const ConcurrentQueue& );

	bool IsClosedAndEmpty() const
	{
		AutoLocker cGuard( &m_cLock );
		return m_bClosed && m_cItems.IsEmpty();
	}

	mutable Locker m_cLock;
	concurrent_private::Tokens m_cTokens;
	Queue<T> m_cItems;
	bool m_bClosed;
};

/** An os::RingBuffer that threads can share, with a Pop() that waits.
 * \ingroup util
 * \par Description:
 *	os::ConcurrentRingBuffer is a fixed-size queue guarded by an
 *	os::Locker, for a thread that must never wait handing items to one
 *	that may: an input driver to the app server, a decoder to the sound
 *	card, any thread to a log writer.  Push() and Write() never wait:
 *	when it is full they overwrite the oldest items (OVERWRITE) or drop
 *	the new ones (REJECT), as os::RingBuffer does, and GetDropped()
 *	counts what was lost.  Pop() and Read() wait for an item, up to a
 *	timeout; TryPop() doesn't.  Close() wakes the threads waiting once
 *	nothing more is coming.
 * \code
 *	os::ConcurrentRingBuffer<int16> cSamples( 8192 );
 *
 *	// the decoder thread
 *	cSamples.Write( pnDecoded, nCount );
 *
 *	// the sound card thread
 *	int16 anOut[512];
 *	int nGot = cSamples.Read( anOut, 512, 20000 );	// waits up to 20 ms for some
 *	if( nGot < 512 )
 *	    memset( anOut + nGot, 0, ( 512 - nGot ) * sizeof( int16 ) );	// an underrun: silence
 * \endcode
 *
 *	Read() waits for the first item only, then takes as many as are
 *	there, up to what was asked.  Timeouts are in microseconds.  As with
 *	os::ConcurrentQueue, what Size() and the like say may change right
 *	after, Lock() and Unlock() make several steps one, and it must not be
 *	destroyed while threads wait in it.
 */
template<class T>
class ConcurrentRingBuffer
{
public:
	typedef typename RingBuffer<T>::WhenFull WhenFull;

	explicit ConcurrentRingBuffer( int nCapacity, WhenFull eWhenFull = RingBuffer<T>::OVERWRITE, const char* pzName = "concurrent_ringbuffer" )
		: m_cLock( pzName ), m_cTokens( pzName ), m_cItems( nCapacity, eWhenFull ), m_bClosed( false ) {}

	/** Puts cValue after the newest item (when full, as WhenFull says),
	 *  and wakes a thread waiting in Pop() or Read().
	 * \return false if cValue was not stored: REJECT and full, or closed. */
	bool Push( const T& cValue )
	{
		int nAdded;
		{
			AutoLocker cGuard( &m_cLock );
			if( m_bClosed )
				return false;
			int nBefore = m_cItems.Count();
			if( !m_cItems.Push( cValue ) )
				return false;
			nAdded = m_cItems.Count() - nBefore;	// 0 when it overwrote
		}
		m_cTokens.Post( nAdded );
		return true;
	}

	/** Pushes nCount items from pValues in one go.  \return how many it
	 *  stored (see os::RingBuffer::Write()); 0 once it is closed. */
	int Write( const T* pValues, int nCount )
	{
		int nStored, nAdded;
		{
			AutoLocker cGuard( &m_cLock );
			if( m_bClosed )
				return 0;
			int nBefore = m_cItems.Count();
			nStored = m_cItems.Write( pValues, nCount );
			nAdded = m_cItems.Count() - nBefore;
		}
		m_cTokens.Post( nAdded );
		return nStored;
	}

	/** Takes the oldest item out, giving it to pcValue (when not NULL),
	 *  waiting up to nTimeout microseconds for one.
	 * \return false if the time ran out, or it is closed and empty. */
	bool Pop( T* pcValue = NULL, bigtime_t nTimeout = INFINITE_TIMEOUT )
	{
		return Read( pcValue, 1, nTimeout, pcValue == NULL ) == 1;
	}
	/** Pop() without waiting */
	bool TryPop( T* pcValue = NULL ) { return Pop( pcValue, 0 ); }

	/** Takes up to nCount of the oldest items out into pValues, waiting up
	 *  to nTimeout microseconds for the first.  \return how many it took
	 *	(0 if the time ran out, or it is closed and empty). */
	int Read( T* pValues, int nCount, bigtime_t nTimeout = INFINITE_TIMEOUT )
	{
		return Read( pValues, nCount, nTimeout, false );
	}

	/** Gives the oldest item to pcValue, leaving it in.
	 * \return false if it is empty. */
	bool Peek( T* pcValue ) const
	{
		AutoLocker cGuard( &m_cLock );
		return m_cItems.Peek( pcValue );
	}

	/** No more Push()es or Write()s: Pop() and Read() give what is left,
	 *  then return at once.  Wakes the threads waiting in them. */
	void Close()
	{
		{
			AutoLocker cGuard( &m_cLock );
			if( m_bClosed )
				return;
			m_bClosed = true;
		}
		m_cTokens.Post();
	}
	bool IsClosed() const
	{
		AutoLocker cGuard( &m_cLock );
		return m_bClosed;
	}

	/** Drops up to nCount of the oldest items.  \return how many. */
	int Discard( int nCount )
	{
		int nGone;
		{
			AutoLocker cGuard( &m_cLock );
			nGone = m_cItems.Discard( nCount );
		}
		m_cTokens.Drain( nGone );
		return nGone;
	}
	/** Empties it (a closed one stays closed; GetDropped() stays) */
	void Clear()
	{
		int nCount;
		{
			AutoLocker cGuard( &m_cLock );
			nCount = m_cItems.Count();
			m_cItems.Clear();
		}
		m_cTokens.Drain( nCount );
	}
	/** Changes the capacity, keeping the newest items */
	void SetCapacity( int nCapacity )
	{
		int nGone;
		{
			AutoLocker cGuard( &m_cLock );
			int nBefore = m_cItems.Count();
			m_cItems.SetCapacity( nCapacity );
			nGone = nBefore - m_cItems.Count();
		}
		m_cTokens.Drain( nGone );
	}

	/** The items, oldest first, as they were when it looked */
	std::vector<T> Items() const
	{
		AutoLocker cGuard( &m_cLock );
		return m_cItems.Items();
	}

	int GetCapacity() const { AutoLocker cGuard( &m_cLock ); return m_cItems.GetCapacity(); }
	int GetFree() const { AutoLocker cGuard( &m_cLock ); return m_cItems.GetFree(); }
	int GetDropped() const { AutoLocker cGuard( &m_cLock ); return m_cItems.GetDropped(); }
	void ResetDropped() { AutoLocker cGuard( &m_cLock ); m_cItems.ResetDropped(); }
	WhenFull GetWhenFull() const { AutoLocker cGuard( &m_cLock ); return m_cItems.GetWhenFull(); }
	void SetWhenFull( WhenFull eWhenFull ) { AutoLocker cGuard( &m_cLock ); m_cItems.SetWhenFull( eWhenFull ); }
	int Size() const { AutoLocker cGuard( &m_cLock ); return m_cItems.Size(); }
	int Count() const { return Size(); }
	bool IsEmpty() const { return Size() == 0; }
	bool IsFull() const { AutoLocker cGuard( &m_cLock ); return m_cItems.IsFull(); }

	/** Locks it, to do several things as one (Lock() and Unlock() pair up) */
	void Lock() const { m_cLock.Lock(); }
	void Unlock() const { m_cLock.Unlock(); }

private:
	ConcurrentRingBuffer( const ConcurrentRingBuffer& );
	ConcurrentRingBuffer& operator=( const ConcurrentRingBuffer& );

	/* Waits for the first item, then takes up to nCount; with bDrop the
	 * items are dropped rather than copied out (pValues may be NULL) */
	int Read( T* pValues, int nCount, bigtime_t nTimeout, bool bDrop )
	{
		if( nCount <= 0 )
			return 0;
		const bigtime_t nDeadline = concurrent_private::Tokens::Deadline( nTimeout );
		for( ;; )
		{
			{
				AutoLocker cGuard( &m_cLock );
				if( m_bClosed && m_cItems.IsEmpty() )
					return 0;
			}
			if( !m_cTokens.Take( nDeadline ) )
				return 0;
			int nGot;
			{
				AutoLocker cGuard( &m_cLock );
				nGot = bDrop ? m_cItems.Discard( nCount ) : m_cItems.Read( pValues, nCount );
				if( nGot == 0 && m_bClosed )
				{
					m_cTokens.Post();	// wake the next waiter, so it sees it too
					return 0;
				}
			}
			if( nGot > 0 )
			{
				m_cTokens.Drain( nGot - 1 );	// we took one token for them all
				return nGot;
			}
			// a token with no item (Clear() was quicker): wait again
		}
	}

	mutable Locker m_cLock;
	concurrent_private::Tokens m_cTokens;
	RingBuffer<T> m_cItems;
	bool m_bClosed;
};

} // namespace os

#endif // __F_UTIL_CONCURRENT_H__
