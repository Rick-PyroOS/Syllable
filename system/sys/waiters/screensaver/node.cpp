/*
 *  The Syllable input server
 *	inputnode.h - Copyright (C) 2008 Dee Sharpe
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


#include "node.h"

using namespace os;

Node::Node( String& cPath, uint32 nFlags, const Messenger& cTarget )
{
	CALLED();
	
	m_pcMonitor = new NodeMonitor(cPath, nFlags, cTarget);
	m_pcPath = new Path(cPath);
}

Node::Node()
{
	CALLED();
	
	Clear();
}

Node::~Node()
{
	CALLED();
	
	m_pcDevRef = NULL;
	delete m_pcPath;
	delete m_pcMonitor;
}

void Node::SetPath( String& cPath )
{
	CALLED();
	
	m_pcPath->SetTo(cPath);
}

String Node::GetPath( void ) const
{
	CALLED();
	
	return( m_pcPath->GetPath() );
}

bool Node::HasPath( void )
{
	CALLED();
	
	if(m_pcPath)
		return true;
	else
		return false;
}

void Node::Clear( void )
{
	CALLED();
	
	if(m_pcPath) {
		delete m_pcPath;
	}
	
	if(m_pcMonitor) {
		delete m_pcMonitor;
	}
	
	m_pcMonitor = new NodeMonitor();
	m_pcPath = new Path();
	m_pcDevRef = NULL;
}

bool Node::operator!( void ) const
{
	CALLED();
	
	return (this == NULL);
}

bool Node::operator==( const Node& cNode ) const
{
	CALLED();
	
	return (*this == cNode);
}

bool Node::operator!=( const Node& cNode ) const
{
	CALLED();
	
	return (*this != cNode);
}

