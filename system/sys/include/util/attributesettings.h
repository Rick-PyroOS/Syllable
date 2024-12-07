/*  Attribute Settings - Manager for storing settings in files attributes.
 *  Copyright (C)2009 Dee Sharpe <demetrioussharpe@netscape.net>
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of version 2 of the GNU Library
 *  General Public License as published by the Free Software
 *  Foundation.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public
 *  License along with this library; if not, write to the Free
 *  Software Foundation, Inc., 59 Temple Place - Suite 330, Boston,
 *  MA 02111-1307, USA
 */


#ifndef	__F_UTIL_ATTRIBUTESETTINGS_H__
#define	__F_UTIL_ATTRIBUTESETTINGS_H__


#include <os/kernel.h>
#include <os/types.h>

#include <storage/fsnode.h>

#include <util/string.h>
#include <util/locker.h>

#include <set>

namespace os
{

typedef union AttrData_s {
	int32 i32;
	int64 i64;
	float f;
	double d;
	char z[1024];
} AttrData_s;

class AttributeSettings
{
public:
	AttributeSettings( const String& cPath );
	~AttributeSettings();
	
	AttributeSettings&	operator=( const AttributeSettings& cSource );
	
	String GetPath() const;
	FSNode GetNode() const;
	
	status_t		AddSetting( const char* pzName, AttrData_s& sData, fsattr_type nType );
	status_t		RemoveSetting( const String& cName );
	status_t		ChangeSetting( const char* pzName, AttrData_s& sData, fsattr_type nType );
	AttrData_s&		GetSetting( const String& cName );
	size_t			CountSettings() const;
	
	bool			IsEmpty() const;
	
	status_t		LoadSettings();
	status_t		StoreSettings();
	status_t		EraseSettings();
	
private:
	class Private;
	Private* m;
};


} // end of namespace

#endif //	__F_UTIL_ATTRIBUTESETTINGS_H__


