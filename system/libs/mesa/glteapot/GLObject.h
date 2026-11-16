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
 * Syllable (Pyro): the teapot comes from teapot_data.h, not the application's
 * resources, and the object's menu (colour, solidity) is ObjectView's.
 */

#ifndef GL_OBJECT_H
#define GL_OBJECT_H

#include "util.h"
#include "Quaternion.h"

struct point {
	float x,y,z;
	float nx,ny,nz;
	float tu,tv;
};

struct tri {
	int p1,p2,p3;
};

struct quadStrip {
	int numpts;
	int *pts;
};


// the colours of an object, as its menu lists them
enum {
	kColorWhite = 1,
	kColorYellow,
	kColorBlue,
	kColorRed,
	kColorGreen
};


class GLObject {
	public:
						GLObject();
		virtual			~GLObject();
		virtual void	Draw(bool forID, float IDcolor[]);

		virtual bool	SpinIt();
		void			Spin(float rx, float ry);
		void 			RotateWorldSpace(float rx, float ry);
		virtual void	DoDrawing(bool forID) {};
		int				Solidity() const;
		void			SetSolidity(int solidity);
		int				Color() const;
		void			SetColor(int color);

		float			x, y, z;
		Quaternion		fRotation;

	protected:

		float			spinX, spinY;
		int				solidity;
		int				color;
		bool			changed;
};

class TriangleObject : public GLObject {
	public:
							TriangleObject();
		virtual				~TriangleObject();
		bool				InitCheck() const;
		virtual void		DoDrawing(bool forID);

	private:
		bool					fStatus;
		BufferArray<point>		fPoints;
		BufferArray<tri>		fTriangles;
		BufferArray<quadStrip>	fQs;
};

#endif // GL_OBJECT_H
