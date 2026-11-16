// EFileBrowser (C)opyright 2008 Jonas Jarvoll
//
// This is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
// See the GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

#ifndef __ADDRESS_FIELD_H_
#define __ADDRESS_FIELD_H_

#include <gui/view.h>
#include <util/string.h>
#include <gui/rect.h>
#include <util/invoker.h>
#include <util/message.h>

class AddressField : public os::View, public os::Invoker
{
public:
	AddressField( os::String path, os::Message* msg );
	~AddressField();

	virtual os::Point GetPreferredSize( bool bLargest ) const;
	virtual void SetFrame( const os::Rect& cRect, bool notifyServer = true );
	void AttachedToWindow();
	void HandleMessage( os::Message* pcMessage );

	status_t SetTarget( const os::Handler* pcHandler, const os::Looper* pcLooper = NULL);

	void SetPath( os::String path );
	os::String GetPath();
private:
	class _Private;
	_Private* m;
};

#endif
