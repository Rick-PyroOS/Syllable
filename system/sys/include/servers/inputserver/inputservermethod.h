/*
 *  The Syllable input server
 *	inputservermethod.h - Copyright (C) 2008 Dee Sharpe
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

#ifndef __INPUT_SERVER_METHOD_H__
#define __INPUT_SERVER_METHOD_H__

#include <os/types.h>
#include <os/kernel.h>

#include <gui/bitmap.h>
#include <gui/menu.h>

#include <storage/path.h>

#include <util/messenger.h>
#include <util/message.h>
#include <util/string.h>

#include <servers/inputserver/inputserverfilter.h>

namespace os
{

class InputServer;

class InputServerMethod : public InputServerFilter
{
public:
	InputServerMethod( const char *zName, Bitmap *pcIcon );
	~InputServerMethod();
	
	status_t			EnqueueMessage( Message *pcMessage );
	virtual status_t	Activate( bool bActivate );	
	
	bool				operator!( void ) const;
	bool				operator==( const InputServerMethod& cMeth ) const;
	bool				operator!=( const InputServerMethod& cMeth ) const;
	
private:
	friend class InputServer;
};

typedef InputServerMethod* instantiate_input_method();

}
#endif /* __INPUT_SERVER_METHOD_H__ */





