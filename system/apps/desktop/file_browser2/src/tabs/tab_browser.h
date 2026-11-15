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

#ifndef _TABBROWSER_H_
#define _TABBROWSER_H_

#include "tab_base.h"

class TabBrowser : public TabBase
{
public:

	TabBrowser();
	~TabBrowser();
	
	
	virtual void SetDirChangeMsg(os::Message*);
	virtual void SetFileChangeMsg(os::Message*);
	
	virtual void SetTarget( os::Handler* handler );

	virtual os::String GetName();
	virtual os::String GetTitle();
	virtual os::String GetFile();
	
	virtual void SetPath( const os::String path, bool notify = true );
	virtual os::String GetPath();
	virtual void ReRead();

	virtual void MakeHistory();
	
	virtual os::String Up();
	virtual os::String Forward();
	virtual os::String Back();
	
	virtual void SetViewMode(IconView::view_type type);
	virtual IconView::view_type GetViewMode();

	virtual void SelectAll();
	virtual void InvertSelection();
	virtual void SetShowHiddenFiles(bool);
	virtual bool GetShowHiddenFiles();
	virtual void ClearLabel();
	virtual void AddLabel(os::Color32_s s);
	virtual int32_t GetSelectedCount();
	virtual void SetZoom(float vZoom);
	virtual float GetZoom();
private:
	class _Private;
	_Private* my;
};

#endif

















