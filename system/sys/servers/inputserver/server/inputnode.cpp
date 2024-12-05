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


#include "inputnode.h"

using namespace os;

InputNode::InputNode( String& cPath, uint32 nFlags, const Messenger& cTarget, InputDeviceRef *pcDevRef )
{
	CALLED();
	
	m_pcMonitor = new NodeMonitor(cPath, nFlags, cTarget);
	m_pcPath = new Path(cPath);
	m_pcDevRef = pcDevRef;
}

InputNode::InputNode()
{
	CALLED();
	
	Clear();
}

InputNode::~InputNode()
{
	CALLED();
	
	m_pcDevRef = NULL;
	delete m_pcPath;
	delete m_pcMonitor;
}

void InputNode::SetPath( String& cPath )
{
	CALLED();
	
	m_pcPath->SetTo(cPath);
}

String InputNode::GetPath( void ) const
{
	CALLED();
	
	return( m_pcPath->GetPath() );
}

bool InputNode::HasPath( void )
{
	CALLED();
	
	if(m_pcPath)
		return true;
	else
		return false;
}

InputDeviceRef *InputNode::GetDevRef( void )
{
	CALLED();
	
	return m_pcDevRef;
}

void InputNode::Clear( void )
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

bool InputNode::operator!( void ) const
{
	CALLED();
	
	return (this == NULL);
}

bool InputNode::operator==( const InputNode& cNode ) const
{
	CALLED();
	
	return (*this == cNode);
}

bool InputNode::operator!=( const InputNode& cNode ) const
{
	CALLED();
	
	return (*this != cNode);
}

