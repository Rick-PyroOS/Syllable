/*
	Copyright 1999, Be Incorporated.   All Rights Reserved.
	This file may be used under the terms of the Be Sample Code License.
*/

/*
 * Syllable (Pyro): an os::Window (Haiku's is a BDirectWindow, for direct
 * rendering) with the same menus.  The settings are os::CheckMenus, which
 * check and uncheck themselves; a light's colours are checked one at a time
 * by the ObjectView.  A menu item only shows its shortcut: the window
 * registers them.
 */

#include <stdio.h>

#include <gui/checkmenu.h>
#include <util/shortcutkey.h>
#include <util/application.h>
#include <util/message.h>
#include <atheos/messages.h>

#include "TeapotWindow.h"

using namespace os;


TeapotWindow::TeapotWindow(const Rect& rect, const char* name)
	:
	Window(rect, "glteapot_window", name, 0)
{
	Rect bounds = GetBounds();
	Menu* menuBar = new Menu(Rect(0, 0, bounds.Width(), 0), "main menu",
		ITEMS_IN_ROW, CF_FOLLOW_LEFT | CF_FOLLOW_RIGHT | CF_FOLLOW_TOP);

	Menu* menu;
	Message msg(kMsgAddModel);

	menuBar->AddItem(menu = new Menu(Rect(), "GLTeapot", ITEMS_IN_COLUMN));

	msg.AddInt32("num", 256);
	menu->AddItem(new MenuItem("Add a teapot", new Message(msg), "CTRL+N"));
	menu->AddItem(new MenuSeparator());
	menu->AddItem(new MenuItem("Quit", new Message(M_QUIT),
		"CTRL+Q"));

	menuBar->AddItem(menu = new Menu(Rect(), "Settings", ITEMS_IN_COLUMN));
	menu->AddItem(new CheckMenu("Perspective", new Message(kMsgPerspective),
		false));
	menu->AddItem(new CheckMenu("FPS display", new Message(kMsgFPS), true));
	menu->AddItem(new CheckMenu("Filled polygons", new Message(kMsgFilled),
		true));
	menu->AddItem(new CheckMenu("Lighting", new Message(kMsgLighting), true));
	menu->AddItem(new CheckMenu("Backface culling", new Message(kMsgCulling),
		true));
	menu->AddItem(new CheckMenu("Z-buffered", new Message(kMsgZBuffer), true));
	menu->AddItem(new CheckMenu("Gouraud shading", new Message(kMsgGouraud),
		true));
	menu->AddItem(new CheckMenu("Fog", new Message(kMsgFog), false));
	menu->AddItem(new MenuSeparator());
	// Haiku waits for the screen's retrace; Syllable has no such call
	menu->AddItem(new CheckMenu("Limit to 60 FPS", new Message(kMsgLimitFps),
		true));

	menuBar->AddItem(menu = new Menu(Rect(), "Lights", ITEMS_IN_COLUMN));
	AddLightMenu(menu, "Upper center", 1, lightWhite);
	AddLightMenu(menu, "Lower left", 2, lightBlue);
	AddLightMenu(menu, "Right", 3, lightNone);

	Point menuSize = menuBar->GetPreferredSize(false);
	menuBar->SetFrame(Rect(0, 0, bounds.Width(), menuSize.y - 1));
	AddChild(menuBar);

	bounds.top = menuSize.y;
	fObjectView = new ObjectView(bounds, "objectView", CF_FOLLOW_ALL);
	AddChild(fObjectView);
	SetFocusChild(fObjectView);

	// the GLTeapot and Settings menus are the view's; Quit is the window's
	menuBar->SetTargetForItems(fObjectView);
	Menu* fileMenu = menuBar->GetItemAt(0)->GetSubMenu();
	fileMenu->GetItemAt(fileMenu->GetItemCount() - 1)->SetTarget(this);

	AddShortcut(ShortcutKey("CTRL+N"), new Message(kMsgAddModel));
	AddShortcut(ShortcutKey("CTRL+Q"), new Message(M_QUIT));

	float f = menuSize.y;
	SetSizeLimits(Point(32, 32 + f), Point(1024, 1024 + f));
}


void
TeapotWindow::HandleMessage(Message* msg)
{
	switch (msg->GetCode()) {
		case kMsgAddModel:
			// Ctrl+N: the view's
			fObjectView->HandleMessage(msg);
			break;
		default:
			Window::HandleMessage(msg);
	}
}


void
TeapotWindow::AddLightMenu(Menu* menu, const char* name, int lightNum,
	int color)
{
	static const char* kNames[] = { "Off", NULL, "White", "Yellow", "Blue",
		"Red", "Green" };
	static const int kColors[] = { lightNone, -1, lightWhite, lightYellow,
		lightBlue, lightRed, lightGreen };

	Menu* subMenu = new Menu(Rect(), name, ITEMS_IN_COLUMN);
	for (int i = 0; i < 7; i++) {
		if (kNames[i] == NULL) {
			subMenu->AddItem(new MenuSeparator());
			continue;
		}
		Message* msg = new Message(kMsgLights);
		msg->AddInt32("num", lightNum);
		msg->AddInt32("color", kColors[i]);
		msg->AddPointer("menu", subMenu);
		msg->AddInt32("index", i);
		subMenu->AddItem(new CheckMenu(kNames[i], msg, kColors[i] == color));
	}
	menu->AddItem(subMenu);
}


bool
TeapotWindow::OkToQuit()
{
	Application::GetInstance()->PostMessage(M_QUIT);
	return true;
}
