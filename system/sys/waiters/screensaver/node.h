/*
 *  The Pyro libsaver
 *	node.h - Copyright (C) 2009 Dee Sharpe
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

#ifndef __NODE_H__
#define __NODE_H__

#include <os/types.h>

#include <storage/path.h>
#include <storage/nodemonitor.h>

namespace os
{

class Node
{
public:
	Node( String& cPath, uint32 nFlags, const Messenger& cTarget );
	Node();
	~Node();
	
	void	SetPath( String& cPath );
	String	GetPath( void ) const;
	bool	HasPath( void );
	void	Clear( void );
	
	bool operator!( void ) const;
	bool operator==( const Node& cNode ) const;
	bool operator!=( const Node& cNode ) const;
	
private:
	NodeMonitor			*m_pcMonitor;
	Path				*m_pcPath;
};

}	/* namespace */

#endif /* __NODE_H__ */

