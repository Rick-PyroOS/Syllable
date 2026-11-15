// EFileBrowser (C)opyright 2006 Jonas Jarvoll
// 				(C)opyright 2010 Rick Caudill
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

#include <storage/file.h>
#include <util/resources.h>
#include <cstdio>
#include <atheos/filesystem.h>
#include <fcntl.h>
#include <unistd.h>

#include "common.h"

using namespace os;
using namespace std;

BitmapImage* LoadImage( const String& zResource )
{
	File cSelf( open_image_file( get_image_id() ) );
	Resources cCol( &cSelf );		
    ResStream* pcStream = cCol.GetResourceStream(zResource);
    BitmapImage* vImage = new BitmapImage(pcStream);
    delete( pcStream );
    return(vImage);
}

os::String GetApplicationPath()
{
	os::String cPath;
	const unsigned BUFFER_SIZE = PATH_MAX+200;
	char buffer[BUFFER_SIZE];
	int nFD;
	
	if( (nFD = open_image_file( IMGFILE_BIN_DIR )) < 0 )
		return "";
		
	if( get_directory_path( nFD,buffer,BUFFER_SIZE ) < 0 )
		return "";
	
	close(nFD);
	buffer[BUFFER_SIZE-1] = 0; /* ensure null-terminated */
	cPath =  os::String(buffer);
	
	return cPath;
}

os::String GetPluginPath()
{
	return GetApplicationPath() + "/plugins";
}
