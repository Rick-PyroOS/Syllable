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

#ifndef _TABBASE_H_
#define _TABBASE_H_

#include <gui/view.h>
#include <storage/file.h>
#include "../syll_gui/iconview.h"

class TabBase : public os::View
{
public:

	TabBase();
	~TabBase();
	
	virtual void SetTarget( os::Handler* handler );

	virtual os::String GetName();
	virtual os::String GetTitle();

	virtual bool Close();

	virtual void SetPath( const os::String path, bool notify = true );
	virtual os::String GetPath();
	virtual void ReRead();
	virtual os::String GetFile();
	virtual void MakeHistory();

	virtual os::String Up();
	virtual os::String Forward();
	virtual os::String Back();
	virtual void SetDirChangeMsg(os::Message*);
	virtual void SetFileChangeMsg(os::Message*);
	virtual void SetViewMode(IconView::view_type type);
	virtual IconView::view_type GetViewMode();
	virtual void SelectAll();
	virtual void InvertSelection();
	virtual void SetShowHiddenFiles(bool);
	virtual bool GetShowHiddenFiles();
	virtual void ClearLabel();
	virtual void AddLabel(os::Color32_s);
	virtual void SetZoom(float vZoom);
	virtual float GetZoom();
	
	
	virtual int32_t GetSelectedCount();
	virtual int32_t GetAllCount();
	virtual int32_t GetSize(); //as bytes
private:
	class _Private;
	_Private* my;
};

#endif




















