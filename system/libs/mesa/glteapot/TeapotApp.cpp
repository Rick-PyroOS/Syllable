/*
	Copyright 1999, Be Incorporated.   All Rights Reserved.
	This file may be used under the terms of the Be Sample Code License.
*/

/*
 * Syllable (Pyro): an os::Application with the one window, centred.
 */

#include <util/application.h>

#include "TeapotWindow.h"

using namespace os;


int
main(int argc, char** argv)
{
	Application* app = new Application("application/x-vnd.syllable-GLTeapot");

	TeapotWindow* window = new TeapotWindow(Rect(0, 0, 295, 290), "GLTeapot");
	window->CenterInScreen();
	window->Show();
	window->MakeFocus();

	app->Run();
	return 0;
}
