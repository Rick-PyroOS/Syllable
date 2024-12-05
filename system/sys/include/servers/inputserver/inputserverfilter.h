/*
 *  The Syllable input server
 *	inputserverfilter.h - Copyright (C) 2008 Dee Sharpe
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

#ifndef __INPUT_SERVER_FILTER_H__
#define __INPUT_SERVER_FILTER_H__

#include <list>

#include <os/types.h>
#include <os/kernel.h>

#include <gui/region.h>
#include <gui/menu.h>
#include <gui/bitmap.h>

#include <storage/path.h>

#include <util/message.h>
#include <util/messagefilter.h>
#include <util/string.h>

#include <servers/inputserver/protocol.h>

namespace os
{


class InputServerFilter
{
public:
	InputServerFilter( void );
	virtual ~InputServerFilter();
	
	virtual	MessageFilterResult	Filter( Message* pcMessage, MessageList tOutList );
	virtual status_t			InitCheck( void );
	virtual status_t			Activate( bool bActivate );	
	
	status_t					GetScreenRegion( Region *cRegion ) const;
	
	virtual status_t			SetName( const char *zName );
	virtual String    			GetName( void ) const;
	virtual status_t			SetIcon( Bitmap *pcIcon );
	virtual Bitmap*				GetIcon( void );
	virtual status_t			SetMenu( Menu *pcMenu, Messenger *pcTarget );
	virtual Menu*				GetMenu( void );
		
	virtual String				GetPath( void ) const;
	virtual void				SetPath( String& cPath );
		
	bool						operator!( void ) const;
	bool						operator==( const InputServerFilter& cFlt ) const;
	bool						operator!=( const InputServerFilter& cFlt ) const;

private:
	friend class InputServer;
	int32	GetLibHandle( void )				{	CALLED();	return( m_nLibHandle );		}
	void	SetLibHandle( int32 nLibHandle )	{	CALLED();	m_nLibHandle = nLibHandle;	}
	
	Menu	*m_pcMenu;
	Bitmap	*m_pcIcon;
	char	*m_zName;
	Path	*m_pcPath;
	int32	m_nLibHandle;
	
};

typedef InputServerFilter* instantiate_input_filter();

}
#endif /* __INPUT_SERVER_FILTER_H__ */

