/*
 *  The Syllable input server
 *	inputservercontainer.h - Copyright (C) 2008 Dee Sharpe
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of version 2 of the GNU Library
 *  General Public License as published by the Free Software
 *  Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef __CONTAINER_H__
#define __CONTAINER_H__

#include <set>

#include <os/kdebug.h>
#include <os/types.h>

#include <util/locker.h>

#include <sys/types.h>

namespace os
{

template<class T> class	Container
{
public:
	Container();
	~Container();
	
	status_t	Insert( T* pcObject );
	status_t	Remove( T* pcObject );
	
	int	GetTabCount( void )	const	{ return( m_nTabCount );	}
	int	GetMaxCount( void )	const	{ return( m_nMaxCount );	}
	int	GetAvgCount( void )	const	{ return( m_nAvgCount );	}
	
	bool HasObj( T* pcObject ) const;
	
	void MakeEmpty( void );
	bool IsEmpty( void ) const		{ return( m_ptSet.empty() );}
	
	T*	GetFirstObj( void );
	T*	Begin( void );
	T*	GetLastObj( void );
	T*	End( void );
	T*	GetNextObj( T* pcObject );
	
private:
	void IncCount( void );
	void DecCount( void );
	
	std::set<T*>	m_ptSet;
	Locker			*m_pcSetLock;
	
	int	m_nTabCount;
	int	m_nMaxCount;
	int	m_nAvgCount;
	
};

template<class T>
void Container<T>::IncCount()
{
	CALLED();
	
    m_nTabCount++;

    m_nAvgCount = (m_nAvgCount + m_nTabCount) / 2;

    if ( m_nTabCount > m_nMaxCount ) {
	m_nMaxCount = m_nTabCount;
    }
}

template<class T>
void Container<T>::DecCount()
{
	CALLED();
	
    m_nTabCount--;

    m_nAvgCount = (m_nAvgCount + m_nTabCount) / 2;
}


template<class T>
Container<T>::Container()
{
	CALLED();
	
	m_pcSetLock = new Locker("container");
	
	MakeEmpty();
}

template<class T>
Container<T>::~Container()
{
	CALLED();
	
	MakeEmpty();
	
	delete m_pcSetLock;
}

template<class T>
status_t Container<T>::Insert( T* pcObject )
{
	CALLED();
	
	AutoLocker _lock_(m_pcSetLock);

	if(!pcObject)
		return -EFAULT;
	else {
		if(m_ptSet.find(pcObject) != m_ptSet.end())
			return -EPERM;
		else {
			m_ptSet.insert(pcObject);
			IncCount();
			return 0;
		}
	}
}

template<class T>
status_t Container<T>::Remove( T* pcObject )
{
	CALLED();
	
	AutoLocker _lock_(m_pcSetLock);

	if(!pcObject)
		return -EFAULT;
	else {
		if(m_ptSet.find(pcObject) == m_ptSet.end())
			return -EPERM;
		else {
			m_ptSet.erase(pcObject);
			DecCount();
			return 0;
		}
	}
}

template<class T>
bool Container<T>::HasObj( T* pcObject ) const
{
	CALLED();
	
	AutoLocker _lock_(m_pcSetLock);

	if(m_ptSet.find(pcObject) == m_ptSet.end())
		return false;
	else
		return true;
}

template<class T>
void Container<T>::MakeEmpty( void )
{
	CALLED();
	
	AutoLocker _lock_(m_pcSetLock);

    m_nTabCount	=	0;
    m_nMaxCount	=	0;
    m_nAvgCount	=	0;
	
    m_ptSet.clear();
}

template<class T>
T*	Container<T>::GetFirstObj( void )
{
	CALLED();
	
	AutoLocker _lock_(m_pcSetLock);

	typename std::set<T*>::iterator it;
	
	it = m_ptSet.begin();
	
	return (*it);
}

template<class T>
T*	Container<T>::Begin( void )
{
	CALLED();
	
	AutoLocker _lock_(m_pcSetLock);

	typename std::set<T*>::iterator it;
	
	it = GetFirstObj();
	
	return (*it);
}

template<class T>
T*	Container<T>::GetLastObj( void )
{
	CALLED();
	
	AutoLocker _lock_(m_pcSetLock);

	typename std::set<T*>::iterator it;
	
	it = --m_ptSet.end();
	
	return (*it);
}

template<class T>
T*	Container<T>::End( void )
{
	CALLED();
	
	AutoLocker _lock_(m_pcSetLock);

	typename std::set<T*>::iterator it;
	
	it = m_ptSet.end();
	
	return (*it);
}

template<class T>
T* Container<T>::GetNextObj( T* pcObject )
{
	CALLED();
	
	AutoLocker _lock_(m_pcSetLock);

	typename std::set<T*>::iterator it;
	
	if((it = m_ptSet.find(pcObject)) == m_ptSet.end())
		return NULL;
	
	it++;
	
	return (*it);
}

}
#endif /* __CONTAINER_H__ */


