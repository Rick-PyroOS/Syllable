/*
	Copyright 1999, Be Incorporated.   All Rights Reserved.
	This file may be used under the terms of the Be Sample Code License.
*/

/*
 * Syllable (Pyro): an os::Window with a menu bar over the ObjectView.
 */

#ifndef _TEAPOT_WINDOW_
#define _TEAPOT_WINDOW_


#include <gui/window.h>
#include <gui/menu.h>

#include "ObjectView.h"


class TeapotWindow : public os::Window {
	public:
						TeapotWindow(const os::Rect& r, const char* name);

		virtual	bool	OkToQuit();
		virtual	void	HandleMessage(os::Message* msg);

	private:
				void	AddLightMenu(os::Menu* menu, const char* name,
							int lightNum, int color);

		ObjectView*		fObjectView;
};


#endif
