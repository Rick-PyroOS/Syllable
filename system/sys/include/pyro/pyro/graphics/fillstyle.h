// Graphics (C)opyright 2008 Jonas Jarvoll
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

#ifndef __GRAPHICS_FILLSTYLE_H_
#define __GRAPHICS_FILLSTYLE_H_

#include <cairo/cairo.h>
#include <gui/point.h>
#include <graphics/colour.h>

namespace graphics
{

///////////////////////////////////////////////////////////////////////////////
/*																			 */
/* F I L L S T Y L E   C L A S S  											 */
/*																			 */
///////////////////////////////////////////////////////////////////////////////
class FillStyle
{
public:
	enum eFillStyleType { FillNone, FillSolid, FillLinear, FillRadial };

	FillStyle();
	FillStyle( enum eFillStyleType type );
	~FillStyle();

	void AddColourStop( const double nOffset, const Colour& cColour );
	void AddColourStop( const double nOffset, const os::String& cColourName );

	uint ColourStopCount() const;
	void GetColourStop( uint nIndex, double& nOffset, Colour& cColour ) const;

	FillStyle& operator=( const FillStyle& cFillStyle );

	eFillStyleType GetType();

	void GetColour( Colour& cColour );
	void GetLinearStartStop( os::Point& cStart, os::Point& cEnd );
	void GetRadialStartStop( os::Point& cCenter1, double& nRadius1, os::Point& cCenter2, double& nRadius2 );

	cairo_pattern_t* GetCairoPattern() const;
protected:	
	class _Private;
	_Private* m;
};

///////////////////////////////////////////////////////////////////////////////
/*																			 */
/* F I L L S T Y L E S O L I D   C L A S S  								 */
/*																			 */
///////////////////////////////////////////////////////////////////////////////
class FillStyleSolid : public FillStyle
{
public:
	FillStyleSolid( const Colour& nPenColour );
	FillStyleSolid( const os::String cColourName );
	
	void SetColour( const Colour& cPenColour );
	void SetColour( const os::String cColourName );
};

///////////////////////////////////////////////////////////////////////////////
/*																			 */
/* F I L L S T Y L E L I N E A R   C L A S S  								 */
/*																			 */
///////////////////////////////////////////////////////////////////////////////
class FillStyleLinear : public FillStyle
{
public:
	FillStyleLinear( const os::Point& cStart, const os::Point& cStop );
	
	
};

///////////////////////////////////////////////////////////////////////////////
/*																			 */
/* F I L L S T Y L E R A D I A L   C L A S S  								 */
/*																			 */
///////////////////////////////////////////////////////////////////////////////
class FillStyleRadial : public FillStyle
{
public:
	FillStyleRadial( const os::Point& cCenter1, const double& nRadius1,
					 const os::Point& cCenter2, const double& nRadius2 );
	
};

} // End of namespace

#endif

