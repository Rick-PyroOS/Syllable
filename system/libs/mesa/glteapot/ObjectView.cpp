/*
 * Copyright 2008 Haiku Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 *
 * Authors:
 *		Alexandre Deckner
 *
 */

/*
 * Original Be Sample source modified to use a quaternion for the object's orientation
 */

/*
	Copyright 1999, Be Incorporated.   All Rights Reserved.
	This file may be used under the terms of the Be Sample Code License.
*/

/*
 * Syllable (Pyro): see ObjectView.h.  Each entry point (the timer, the mouse,
 * a resize, a menu) takes the GL lock once around its GL calls; frames are
 * shown with SwapBuffers() after UnlockGL().
 */

#include "ObjectView.h"

#include <math.h>

#include <atheos/time.h>
#include <gui/window.h>
#include <gui/checkmenu.h>
#include <util/message.h>

#include "FPS.h"
#include "GLObject.h"

using namespace os;

float displayScale = 1.0;
float depthOfView = 30.0;
float zRatio = 10.0;

float white[3] = {1.0, 1.0, 1.0};
float dimWhite[3] = {0.25, 0.25, 0.25};
float black[3] = {0.0, 0.0, 0.0};
float foggy[3] = {0.4, 0.4, 0.4};
float blue[3] = {0.0, 0.0, 1.0};
float dimBlue[3] = {0.0, 0.0, 0.5};
float yellow[3] = {1.0, 1.0, 0.0};
float dimYellow[3] = {0.5, 0.5, 0.0};
float green[3] = {0.0, 1.0, 0.0};
float dimGreen[3] = {0.0, 0.5, 0.0};
float red[3] = {1.0, 0.0, 0.0};

float* bgColor = black;

struct light {
	float *ambient;
	float *diffuse;
	float *specular;
};


light lights[] = {
	{NULL, NULL, NULL},
	{dimWhite, white, white},
	{dimWhite, yellow, yellow},
	{dimWhite, red, red},
	{dimWhite, blue, blue},
	{dimWhite, green, green}
};

static const int kDrawTimer = 1;
// "Limit FPS": about 60 frames a second (Haiku waits for the screen's retrace)
static const bigtime_t kLimitedFrameTime = 16667;
// unlimited: a short pause, so the window's messages are handled between frames
static const bigtime_t kFrameTime = 1000;

static const char* kColorNames[] = { "White", "Yellow", "Blue", "Red", "Green" };
static const char* kSolidityNames[] = { "Solid", "Translucent", "Transparent" };


static GLParams
gl_params()
{
	GLParams params;
	params.flags |= SYLGL_DOUBLE_BUFFER;
	params.depth_bits = 16;
	return params;
}


ObjectView::ObjectView(const Rect& rect, const char *name, uint32 resizingMode)
	: GLView(rect, name, gl_params(), resizingMode, WID_WILL_DRAW),
	fLastFrame(0),
	fHistEntries(0),
	fOldestEntry(0),
	fFps(true),
	fLimitFps(true),
	fLastGouraud(true),
	fGouraud(true),
	fLastZbuf(true),
	fZbuf(true),
	fLastCulling(true),
	fCulling(true),
	fLastLighting(true),
	fLighting(true),
	fLastFilled(true),
	fFilled(true),
	fLastPersp(false),
	fPersp(false),
	fLastTextured(false),
	fTextured(false),
	fLastFog(false),
	fFog(false),
	fForceRedraw(false),
	fLastYXRatio(1),
	fYxRatio(1),
	fTimerArmed(false),
	fNoPause(false),
	fObjectMenu(NULL),
	fMenuObject(NULL)
{
	fTrackingInfo.isTracking = false;
	fTrackingInfo.pickedObject = NULL;
	fTrackingInfo.buttons = 0;
	fTrackingInfo.lastX = 0.0f;
	fTrackingInfo.lastY = 0.0f;
	fTrackingInfo.lastDx = 0.0f;
	fTrackingInfo.lastDy = 0.0f;

	fLastObjectDistance = fObjectDistance = depthOfView / 8;

	TriangleObject *Tri = new TriangleObject();
	if (Tri->InitCheck())
		fObjects.push_back(Tri);
	else
		delete Tri;
}


ObjectView::~ObjectView()
{
	for (size_t i = 0; i < fObjects.size(); i++)
		delete fObjects[i];
	delete fObjectMenu;
}


void
ObjectView::AttachedToWindow()
{
	float position[] = {0.0, 3.0, 3.0, 0.0};
	float position1[] = {-3.0, -3.0, 3.0, 0.0};
	float position2[] = {3.0, 0.0, 0.0, 0.0};
	float local_view[] = {0.0, 0.0};
	Rect bounds = GetBounds();

	GLView::AttachedToWindow();

	LockGL();

	glEnable(GL_DITHER);
	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);
	glDepthFunc(GL_LESS);

	glShadeModel(GL_SMOOTH);

	glLightfv(GL_LIGHT0, GL_POSITION, position);
	glLightfv(GL_LIGHT0 + 1, GL_POSITION, position1);
	glLightfv(GL_LIGHT0 + 2, GL_POSITION, position2);
	glLightModelfv(GL_LIGHT_MODEL_LOCAL_VIEWER, local_view);

	glEnable(GL_LIGHT0);
	glLightfv(GL_LIGHT0, GL_SPECULAR, lights[lightWhite].specular);
	glLightfv(GL_LIGHT0, GL_DIFFUSE,lights[lightWhite].diffuse);
	glLightfv(GL_LIGHT0, GL_AMBIENT,lights[lightWhite].ambient);
	glEnable(GL_LIGHT1);
	glLightfv(GL_LIGHT1, GL_SPECULAR, lights[lightBlue].specular);
	glLightfv(GL_LIGHT1, GL_DIFFUSE,lights[lightBlue].diffuse);
	glLightfv(GL_LIGHT1, GL_AMBIENT,lights[lightBlue].ambient);

	glFrontFace(GL_CW);
	glEnable(GL_LIGHTING);
	glEnable(GL_AUTO_NORMAL);
	glEnable(GL_NORMALIZE);

	glMaterialf(GL_FRONT, GL_SHININESS, 0.6 * 128.0);

	glClearColor(bgColor[0], bgColor[1], bgColor[2], 1.0);
	glColor3f(1.0, 1.0, 1.0);

	glViewport(0, 0, (GLint)bounds.Width() + 1,
				(GLint)bounds.Height() + 1);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();

	float scale = displayScale;
	glOrtho(-scale, scale, -scale, scale, -scale * depthOfView,
			scale * depthOfView);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	UnlockGL();

	// the window may have been made at another size than the view's first
	Resized();

	// the object menu: colours, then solidity
	fObjectMenu = new Menu(Rect(0, 0, 1, 1), "Object", ITEMS_IN_COLUMN);
	for (int i = 0; i < 5; i++) {
		Message* msg = new Message(kMsgObjectColor);
		msg->AddInt32("color", kColorWhite + i);
		fObjectMenu->AddItem(new CheckMenu(kColorNames[i], msg));
	}
	fObjectMenu->AddItem(new MenuSeparator());
	for (int i = 0; i < 3; i++) {
		Message* msg = new Message(kMsgObjectSolidity);
		msg->AddInt32("solidity", i);
		fObjectMenu->AddItem(new CheckMenu(kSolidityNames[i], msg));
	}
	fObjectMenu->SetTargetForItems(this);

	fForceRedraw = true;
	RequestDraw();
}


void
ObjectView::DetachedFromWindow()
{
	if (fTimerArmed && GetLooper() != NULL)
		GetLooper()->RemoveTimer(this, kDrawTimer);
	fTimerArmed = false;
	GLView::DetachedFromWindow();
}


void
ObjectView::RequestDraw()
{
	if (fTimerArmed || GetLooper() == NULL)
		return;
	GetLooper()->AddTimer(this, kDrawTimer, kFrameTime, true);
	fTimerArmed = true;
}


void
ObjectView::TimerTick(int id)
{
	if (id != kDrawTimer) {
		GLView::TimerTick(id);
		return;
	}
	fTimerArmed = false;

	LockGL();
	bool spinning = SpinIt();
	if (spinning)
		DrawFrame(fNoPause);
	UnlockGL();

	if (spinning) {
		SwapBuffers();
		fNoPause = true;
		// the next frame: as the original's thread, which draws while
		// anything moves and waits for an event when nothing does
		GetLooper()->AddTimer(this, kDrawTimer,
			fLimitFps ? kLimitedFrameTime : kFrameTime, true);
		fTimerArmed = true;
	} else
		fNoPause = false;
}


void
ObjectView::HandleMessage(Message* msg)
{
	bool toggleItem = false;

	switch (msg->GetCode()) {
		case kMsgFPS:
			fFps = (fFps) ? false : true;
			fForceRedraw = true;
			RequestDraw();
			break;
		case kMsgAddModel:
		{
			TriangleObject *Tri = new TriangleObject();
			if (Tri->InitCheck())
				fObjects.push_back(Tri);
			else
				delete Tri;
			fForceRedraw = true;
			RequestDraw();
			break;
		}
		case kMsgLights:
		{
			int32 lightNum = 0, color = 0, index = -1;
			msg->FindInt32("num", &lightNum);
			msg->FindInt32("color", &color);

			// the light's menu shows one colour checked (Haiku marks its item
			// and unmarks the others)
			Menu* menu = NULL;
			if (msg->FindPointer("menu", reinterpret_cast<void**>(&menu)) == 0
				&& msg->FindInt32("index", &index) == 0 && menu != NULL) {
				for (int i = 0; i < menu->GetItemCount(); i++) {
					CheckMenu* item = dynamic_cast<CheckMenu*>(menu->GetItemAt(i));
					if (item != NULL)
						item->SetChecked(i == index);
				}
			}

			LockGL();
			if (color != lightNone) {
				glEnable(GL_LIGHT0 + lightNum - 1);
				glLightfv(GL_LIGHT0 + lightNum - 1, GL_SPECULAR,
					lights[color].specular);
				glLightfv(GL_LIGHT0 + lightNum - 1, GL_DIFFUSE,
					lights[color].diffuse);
				glLightfv(GL_LIGHT0 + lightNum - 1, GL_AMBIENT,
					lights[color].ambient);
			} else {
				glDisable(GL_LIGHT0 + lightNum - 1);
			}
			UnlockGL();
			fForceRedraw = true;
			RequestDraw();
			break;
		}
		case kMsgGouraud:
			fGouraud = !fGouraud;
			toggleItem = true;
			break;
		case kMsgZBuffer:
			fZbuf = !fZbuf;
			toggleItem = true;
			break;
		case kMsgCulling:
			fCulling = !fCulling;
			toggleItem = true;
			break;
		case kMsgLighting:
			fLighting = !fLighting;
			toggleItem = true;
			break;
		case kMsgFilled:
			fFilled = !fFilled;
			toggleItem = true;
			break;
		case kMsgPerspective:
			fPersp = !fPersp;
			toggleItem = true;
			break;
		case kMsgFog:
			fFog = !fFog;
			toggleItem = true;
			break;
		case kMsgLimitFps:
			fLimitFps = !fLimitFps;
			toggleItem = true;
			break;
		case kMsgObjectColor:
		case kMsgObjectSolidity:
		{
			// the object the menu was opened for, if it is still there
			for (size_t i = 0; i < fObjects.size(); i++) {
				if (fObjects[i] != fMenuObject)
					continue;
				int32 value = 0;
				if (msg->GetCode() == kMsgObjectColor) {
					if (msg->FindInt32("color", &value) == 0)
						fMenuObject->SetColor(value);
				} else if (msg->FindInt32("solidity", &value) == 0)
					fMenuObject->SetSolidity(value);
				RequestDraw();
			}
			fMenuObject = NULL;
			break;
		}
		default:
			GLView::HandleMessage(msg);
			return;
	}

	// the menu items are os::CheckMenus: they check and uncheck themselves
	if (toggleItem)
		RequestDraw();
}


int
ObjectView::ObjectAtPoint(const Point &point)
{
	LockGL();
	glShadeModel(GL_FLAT);
	glDisable(GL_LIGHTING);
	glDisable(GL_FOG);
	glClearColor(black[0], black[1], black[2], 1.0);
	glClear(GL_COLOR_BUFFER_BIT | (fZbuf ? GL_DEPTH_BUFFER_BIT : 0));

	float idColor[3];
	idColor[1] = idColor[2] = 0;
	for (size_t i = 0; i < fObjects.size(); i++) {
		// to take into account 16 bits colorspaces,
		// only use the 5 highest bits of the red channel
		idColor[0] = (255 - (i << 3)) / 255.0;
		fObjects[i]->Draw(true, idColor);
	}
	// OSMesa's buffer is the one buffer there is: read it as it is
	uint8 pixel[256];
	glReadPixels((GLint)point.x, (GLint)(GetBounds().bottom - point.y), 1, 1,
		GL_RGB, GL_UNSIGNED_BYTE, pixel);
	int objNum = pixel[0];
	objNum = (255 - objNum) >> 3;

	EnforceState();
	UnlockGL();

	// the next frame paints over the objects' ID colours
	fForceRedraw = true;
	RequestDraw();

	return objNum;
}


static GLObject*
object_at(std::vector<GLObject*>& objects, int index)
{
	if (index < 0 || index >= (int)objects.size())
		return NULL;
	return objects[index];
}


void
ObjectView::MouseDown(const Point& point, uint32 buttons)
{
	MakeFocus(true);

	GLObject* object = object_at(fObjects, ObjectAtPoint(point));
	if (object == NULL)
		return;

	// as Haiku's: the left button turns an object, the right one moves it,
	// the middle one opens its menu
	if (buttons == MBI_LEFT || buttons == MBI_RIGHT) {
		fTrackingInfo.pickedObject = object;
		fTrackingInfo.buttons = buttons;
		fTrackingInfo.isTracking = true;
		fTrackingInfo.lastX = point.x;
		fTrackingInfo.lastY = point.y;
		fTrackingInfo.lastDx = 0.0f;
		fTrackingInfo.lastDy = 0.0f;
		fTrackingInfo.pickedObject->Spin(0.0f, 0.0f);
	} else
		OpenObjectMenu(object, point);
}


void
ObjectView::OpenObjectMenu(GLObject* object, const Point& point)
{
	if (fObjectMenu == NULL)
		return;

	int color = object->Color();
	int solidity = object->Solidity();
	for (int i = 0; i < fObjectMenu->GetItemCount(); i++) {
		CheckMenu* item = dynamic_cast<CheckMenu*>(fObjectMenu->GetItemAt(i));
		if (item == NULL)
			continue;
		bool checked = i < 5 ? kColorWhite + i == color : i - 6 == solidity;
		item->SetChecked(checked);
	}
	fMenuObject = object;
	fObjectMenu->Open(ConvertToScreen(point));
}


void
ObjectView::MouseUp(const Point& point, uint32 buttons, Message* data)
{
	if (fTrackingInfo.isTracking) {

		//spin the teapot on release, TODO: use a marching sum and divide by time
		if (fTrackingInfo.buttons == MBI_LEFT
			&& fTrackingInfo.pickedObject != NULL
			&& (fabs(fTrackingInfo.lastDx) > 1.0f
				|| fabs(fTrackingInfo.lastDy) > 1.0f) ) {

			fTrackingInfo.pickedObject->Spin(0.5f * fTrackingInfo.lastDy, 0.5f * fTrackingInfo.lastDx);

			RequestDraw();
		}

		//stop tracking
		fTrackingInfo.isTracking = false;
		fTrackingInfo.buttons = 0;
		fTrackingInfo.pickedObject = NULL;
		fTrackingInfo.lastX = 0.0f;
		fTrackingInfo.lastY = 0.0f;
		fTrackingInfo.lastDx = 0.0f;
		fTrackingInfo.lastDy = 0.0f;
	}
	GLView::MouseUp(point, buttons, data);
}


void
ObjectView::MouseMove(const Point& point, int transit, uint32 buttons,
	Message* data)
{
	if (fTrackingInfo.isTracking && fTrackingInfo.pickedObject != NULL) {

		float dx = point.x - fTrackingInfo.lastX;
		float dy = point.y - fTrackingInfo.lastY;
		fTrackingInfo.lastX = point.x;
		fTrackingInfo.lastY = point.y;

		if (fTrackingInfo.buttons == MBI_LEFT) {

			fTrackingInfo.pickedObject->Spin(0.0f, 0.0f);
			fTrackingInfo.pickedObject->RotateWorldSpace(dx,dy);
			fTrackingInfo.lastDx = dx;
			fTrackingInfo.lastDy = dy;

			RequestDraw();

		} else if (fTrackingInfo.buttons == MBI_RIGHT) {

			float xinc = (dx * 2 * displayScale / GetBounds().Width());
			float yinc = (-dy * 2 * displayScale / GetBounds().Height());
			float zinc = 0;

			if (fPersp) {
				zinc = yinc * (fTrackingInfo.pickedObject->z / displayScale);
				xinc *= -(fTrackingInfo.pickedObject->z * 4 / zRatio);
				yinc *= -(fTrackingInfo.pickedObject->z * 4 / zRatio);
			}

			fTrackingInfo.pickedObject->x += xinc;
			if (GetQualifiers() & QUAL_SHIFT)
				fTrackingInfo.pickedObject->z += zinc;
			else
	  			fTrackingInfo.pickedObject->y += yinc;

			fForceRedraw = true;
			RequestDraw();
		}
	}
	GLView::MouseMove(point, transit, buttons, data);
}


void
ObjectView::FrameSized(const Point& delta)
{
	GLView::FrameSized(delta);
	Resized();
}


void
ObjectView::Resized()
{
	// LockGL() resizes the view's buffer to the view
	LockGL();

	float width = GetBounds().Width();
	float height = GetBounds().Height();
	fYxRatio = height / width;
	glViewport(0, 0, (GLint)width + 1, (GLint)height + 1);

	// To prevent weird buffer contents
	glClear(GL_COLOR_BUFFER_BIT);

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	float scale = displayScale;

	if (fPersp) {
		gluPerspective(60, 1.0 / fYxRatio, 0.15, 120);
	} else {
		if (fYxRatio < 1) {
			glOrtho(-scale / fYxRatio, scale / fYxRatio, -scale, scale, -1.0,
				depthOfView * 4);
		} else {
			glOrtho(-scale, scale, -scale * fYxRatio, scale * fYxRatio, -1.0,
				depthOfView * 4);
		}
	}

	fLastYXRatio = fYxRatio;

	glMatrixMode(GL_MODELVIEW);

	UnlockGL();

	fForceRedraw = true;
	RequestDraw();
}


// with the GL lock held
bool
ObjectView::RepositionView()
{
	if (!(fPersp != fLastPersp) &&
		!(fLastObjectDistance != fObjectDistance) &&
		!(fLastYXRatio != fYxRatio)) {
		return false;
	}

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	float scale = displayScale;

	if (fPersp) {
		gluPerspective(60, 1.0 / fYxRatio, 0.15, 120);
	} else {
		if (fYxRatio < 1) {
			glOrtho(-scale / fYxRatio, scale / fYxRatio, -scale, scale, -1.0,
				depthOfView * 4);
		} else {
			glOrtho(-scale, scale, -scale * fYxRatio, scale * fYxRatio, -1.0,
				depthOfView * 4);
		}
	}

	glMatrixMode(GL_MODELVIEW);

	fLastObjectDistance = fObjectDistance;
	fLastPersp = fPersp;
	fLastYXRatio = fYxRatio;
	return true;
}


// with the GL lock held
void
ObjectView::EnforceState()
{
	glShadeModel(fGouraud ? GL_SMOOTH : GL_FLAT);

	if (fZbuf)
		glEnable(GL_DEPTH_TEST);
	else
		glDisable(GL_DEPTH_TEST);

	if (fCulling)
		glEnable(GL_CULL_FACE);
	else
		glDisable(GL_CULL_FACE);

	if (fLighting)
		glEnable(GL_LIGHTING);
	else
		glDisable(GL_LIGHTING);

	if (fFilled)
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	else
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

	if (fFog) {
		glFogf(GL_FOG_START, 10.0);
		glFogf(GL_FOG_DENSITY, 0.2);
		glFogf(GL_FOG_END, depthOfView);
		glFogfv(GL_FOG_COLOR, foggy);
		glEnable(GL_FOG);
		bgColor = foggy;
		glClearColor(bgColor[0], bgColor[1], bgColor[2], 1.0);
	} else {
		glDisable(GL_FOG);
		bgColor = black;
		glClearColor(bgColor[0], bgColor[1], bgColor[2], 1.0);
	}
}


// with the GL lock held
bool
ObjectView::SpinIt()
{
	bool changed = false;

	if (fGouraud != fLastGouraud) {
		glShadeModel(fGouraud ? GL_SMOOTH : GL_FLAT);
		fLastGouraud = fGouraud;
		changed = true;
	}

	if (fZbuf != fLastZbuf) {
		if (fZbuf)
			glEnable(GL_DEPTH_TEST);
		else
			glDisable(GL_DEPTH_TEST);
		fLastZbuf = fZbuf;
		changed = true;
	}

	if (fCulling != fLastCulling) {
		if (fCulling)
			glEnable(GL_CULL_FACE);
		else
			glDisable(GL_CULL_FACE);
		fLastCulling = fCulling;
		changed = true;
	}

	if (fLighting != fLastLighting) {
		if (fLighting)
			glEnable(GL_LIGHTING);
		else
			glDisable(GL_LIGHTING);
		fLastLighting = fLighting;
		changed = true;
	}

	if (fFilled != fLastFilled) {
		if (fFilled) {
			glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
		} else {
			glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
		}
		fLastFilled = fFilled;
		changed = true;
	}

	if (fFog != fLastFog) {
		if (fFog) {
			glFogf(GL_FOG_START, 1.0);
			glFogf(GL_FOG_DENSITY, 0.2);
			glFogf(GL_FOG_END, depthOfView);
			glFogfv(GL_FOG_COLOR, foggy);
			glEnable(GL_FOG);
			bgColor = foggy;
			glClearColor(bgColor[0], bgColor[1], bgColor[2], 1.0);
		} else {
			glDisable(GL_FOG);
			bgColor = black;
			glClearColor(bgColor[0], bgColor[1], bgColor[2], 1.0);
		}
		fLastFog = fFog;
		changed = true;
	}

	changed = changed || RepositionView();
	changed = changed || fForceRedraw;
	fForceRedraw = false;

	for (size_t i = 0; i < fObjects.size(); i++) {
		bool hack = fObjects[i]->SpinIt();
		changed = changed || hack;
	}

	return changed;
}


// with the GL lock held; the caller shows the frame (SwapBuffers())
void
ObjectView::DrawFrame(bool noPause)
{
	glClear(GL_COLOR_BUFFER_BIT | (fZbuf ? GL_DEPTH_BUFFER_BIT : 0));

	for (size_t i = 0; i < fObjects.size(); i++) {
		GLObject *object = fObjects[i];
		if (object->Solidity() == 0)
			object->Draw(false, NULL);
	}
	EnforceState();
	for (size_t i = 0; i < fObjects.size(); i++) {
		GLObject *object = fObjects[i];
		if (object->Solidity() != 0)
			object->Draw(false, NULL);
	}

	glDisable(GL_BLEND);
	glDepthMask(GL_TRUE);

	if (noPause) {
		bigtime_t now = get_system_time();
		float fps = 1.0 / ((now - fLastFrame) / 1000000.0);
		fLastFrame = now;
		int entry;
		if (fHistEntries < HISTSIZE) {
			entry = (fOldestEntry + fHistEntries) % HISTSIZE;
			fHistEntries++;
		} else {
			entry = fOldestEntry;
			fOldestEntry = (fOldestEntry + 1) % HISTSIZE;
		}

		fFpsHistory[entry] = fps;

		if (fHistEntries > 5) {
			fps = 0;
			for (int i = 0; i < fHistEntries; i++)
				fps += fFpsHistory[(fOldestEntry + i) % HISTSIZE];

			fps /= fHistEntries;

			if (fFps) {
				glPushAttrib(GL_ENABLE_BIT | GL_LIGHTING_BIT);
				glPushMatrix();
				glLoadIdentity();
				glTranslatef(-0.9, -0.9, 0);
				glScalef(0.10, 0.10, 0.10);
				glDisable(GL_LIGHTING);
				glDisable(GL_DEPTH_TEST);
				glDisable(GL_BLEND);
				glColor3f(1.0, 1.0, 0);
				glMatrixMode(GL_PROJECTION);
				glPushMatrix();
				glLoadIdentity();
				glMatrixMode(GL_MODELVIEW);

				FPS::drawCounter(fps);

				glMatrixMode(GL_PROJECTION);
				glPopMatrix();
				glMatrixMode(GL_MODELVIEW);
				glPopMatrix();
				glPopAttrib();
			}
		}
	} else {
		fHistEntries = 0;
		fOldestEntry = 0;
		fLastFrame = get_system_time();
	}
	glFinish();
}
