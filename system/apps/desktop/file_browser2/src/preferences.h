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

#ifndef __PREFERENCES_H__
#define __PREFERENCES_H__

#include <util/settings.h>
#include <util/string.h>
#include <util/variant.h>

#include "syll_gui/iconview.h"
#include "syll_gui/imagebutton.h"


class Preferences : public os::Settings
{
public:
	static Preferences* GetInstance();

	Preferences();
	~Preferences();

	os::Rect GetWindowFrame();
	void SetWindowFrame( const os::Rect frame );

	void SetSplitRatio( float value );
	float GetSplitRatio();

	void SetStartUpPath( const os::String path );
	os::String GetStartUpPath();

	void SetDefaultView( IconView::view_type style );
	IconView::view_type GetDefaultView();

	void SetUseEmblems( const bool active );
	bool GetUseEmblems();

	os::Point GetEmblemPosition();
	void SetEmblemPosition( const os::Point pos );

	bool GetShowSideBar();
	void SetShowSideBar(bool);
	
	bool GetShowStatusBar();
	void SetShowStatusBar(bool);
	
	bool GetShowToolBar();
	void SetShowToolBar(bool);
	
	void SetToolBarType(os::ImageButton::label_position e);
	os::ImageButton::label_position GetToolBarType();
	
	void SetShowHiddenFiles(bool);
	bool GetShowHiddenFiles();
	
	void SetShowPane(bool);
	bool GetShowPane();
	
	void SetZoom(float);
	float GetZoom();
	
private:
	static Preferences* m_theInstance;
};

#endif











