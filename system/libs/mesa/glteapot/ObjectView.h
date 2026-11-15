/*
 * Copyright 2008 Haiku Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 *
 */

/*
	Copyright 1999, Be Incorporated.   All Rights Reserved.
	This file may be used under the terms of the Be Sample Code License.
*/

/*
 * Syllable (Pyro): an os::GLView.  Haiku's renders in a thread of its own;
 * here the window's thread renders, from a timer, so the view's GL calls
 * and os::GLView::SwapBuffers() (which locks the window) never meet in two
 * threads.  An object's menu (colour, solidity) is the view's, opened
 * without waiting for it.
 */

#ifndef OBJECT_VIEW_H
#define OBJECT_VIEW_H

#include <vector>

#include <GL/glu.h>
#include <gui/GLView.h>
#include <gui/menu.h>

enum {
	kMsgFPS = 1000,
	kMsgLimitFps,
	kMsgAddModel,
	kMsgGouraud,
	kMsgZBuffer,
	kMsgCulling,
	kMsgTextured,
	kMsgFog,
	kMsgLighting,
	kMsgLights,
	kMsgFilled,
	kMsgPerspective,
	kMsgObjectColor,
	kMsgObjectSolidity
};

enum lights {
	lightNone = 0,
	lightWhite,
	lightYellow,
	lightRed,
	lightBlue,
	lightGreen
};

#define HISTSIZE 10

class GLObject;

struct TrackingInfo {
	float		lastX;
	float		lastY;
	float		lastDx;
	float		lastDy;
	bool		isTracking;
	GLObject	*pickedObject;
	uint32		buttons;
};

class ObjectView : public os::GLView {
	public:
						ObjectView(const os::Rect& rect, const char* name,
							uint32 resizingMode);
						~ObjectView();

		virtual	void	MouseDown(const os::Point& point, uint32 buttons);
		virtual	void	MouseUp(const os::Point& point, uint32 buttons,
							os::Message* data);
		virtual	void	MouseMove(const os::Point& point, int transit,
							uint32 buttons, os::Message* data);

		virtual	void	HandleMessage(os::Message* msg);
		virtual	void	AttachedToWindow();
		virtual	void	DetachedFromWindow();
		virtual	void	FrameSized(const os::Point& delta);
		virtual	void	TimerTick(int id);
				bool	SpinIt();
				bool	LimitFps() { return fLimitFps; };
				int		ObjectAtPoint(const os::Point &point);
		virtual	void	DrawFrame(bool noPause);
				void	EnforceState();
				bool	RepositionView();
				bool	HasObjects() const { return !fObjects.empty(); }

				// draw again soon (the timer)
				void	RequestDraw();

	private:
				void	OpenObjectMenu(GLObject* object, const os::Point& point);
				void	Resized();

		std::vector<GLObject*>	fObjects;
		bigtime_t		fLastFrame;
		int32			fHistEntries,fOldestEntry;
		bool			fFps, fLimitFps, fLastGouraud, fGouraud;
		bool			fLastZbuf, fZbuf, fLastCulling, fCulling;
		bool			fLastLighting, fLighting, fLastFilled, fFilled;
		bool			fLastPersp, fPersp, fLastTextured, fTextured;
		bool			fLastFog, fFog, fForceRedraw;
		float			fLastYXRatio, fYxRatio, fFpsHistory[HISTSIZE];
		float			fObjectDistance, fLastObjectDistance;
		TrackingInfo	fTrackingInfo;
		bool			fTimerArmed;
		bool			fNoPause;
		os::Menu*		fObjectMenu;
		GLObject*		fMenuObject;
};

#endif // OBJECT_VIEW_H
