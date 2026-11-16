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

#include <storage/registrar.h>
#include <storage/volumes.h>
#include <storage/file.h>
#include <storage/memfile.h>

#include "iconserver.h"
#include "preferences.h"
#include "common.h"
#include "application.h"

using namespace os;

/*
  The IconServer is a Singleton and handles icons for disks, folders 
  and files.
*/

static uint8 g_aCDImage[] = {
#include "images/cd.h"
};

static uint8 g_aDiskImage[] = {
#include "images/disk.h"
};

static uint8 g_aFloppyImage[] = {
#include "images/floppy.h"
};

static uint8 g_aFileImage[] = {
#include "images/file.h"
};

static uint8 g_aFolderImage[] = {
#include "images/folder.h"
};

///////////////////////////////////////////////////////////////////////////////
//
// S I N G L E T O N
//
///////////////////////////////////////////////////////////////////////////////

IconServer* IconServer::m_theInstance = NULL;

IconServer* IconServer :: GetInstance()
{
	if( m_theInstance == NULL )
	{
		m_theInstance = new IconServer();
	}

	return m_theInstance;
}

///////////////////////////////////////////////////////////////////////////////
//
// P R I V A T E
//
///////////////////////////////////////////////////////////////////////////////

class IconServer :: _Private
{
public:
	_Private()
	{	
		try
		{
			_Manager = RegistrarManager::Get();
		} 
		catch( ... ) 
		{ 
			_Manager = NULL; 
		}	
	}

	BitmapImage* GetDriveIcon( String zPath, fs_info* psInfo ) const
	{	
		BitmapImage* pcItemIcon = NULL;
		Volumes cVolumes;
		String cMountPoint;
	
		for( uint i = 0; i < cVolumes.GetMountPointCount(); i++ )
		{
			if( cVolumes.GetMountPoint( i, &cMountPoint ) == 0 )
			{
				if( zPath == cMountPoint )
				{
					fs_info sInfo;
					if( cVolumes.GetFSInfo( i, &sInfo ) == 0 && ( sInfo.fi_flags & FS_IS_BLOCKBASED ) )
					{
						if( psInfo != NULL )
							*psInfo = sInfo;
				
						String zDevice = os::Path( sInfo.fi_device_path ).GetDir().GetLeaf();
						if( zDevice.Length() > 2 )
						{
							os::StreamableIO* pcSource = NULL;
							if( zDevice[0] == 'c' && zDevice[1] == 'd' )
							{
								try {
									pcSource = new os::File( "/system/icons/cdrom.png" );
								} catch( ... ) {
									pcSource = new MemFile( g_aCDImage, sizeof( g_aCDImage ) );
								}
							}
							else if( zDevice[0] == 'f' && zDevice[1] == 'd' )
							{
								try {
									pcSource = new os::File( "/system/icons/floppy.png" );
								} catch( ... ) {
									pcSource = new MemFile( g_aFloppyImage, sizeof( g_aFloppyImage ) );
								}
							}
							else 
							{
								try {
									pcSource = new os::File( "/system/icons/harddisk.png" );
								} catch( ... ) {
									pcSource = new MemFile( g_aDiskImage, sizeof( g_aDiskImage ) );
								}
							}
						
							pcItemIcon = new os::BitmapImage();
									
							pcItemIcon->Load( pcSource );
							delete( pcSource );						
						}
					}
				}
			}
		}
	
		if( pcItemIcon == NULL )
		{
			/* Default icon */
			MemFile* pcSource = new MemFile( g_aDiskImage, sizeof( g_aDiskImage ) );
			pcItemIcon = new os::BitmapImage();
			pcItemIcon->Load( pcSource );
			delete( pcSource );
		}
	
		if( pcItemIcon && !( pcItemIcon->GetSize() == os::Point( 24, 24 ) ) )
			pcItemIcon->SetSize( os::Point( 24, 24 ) );
	
		return( pcItemIcon );
	}	

	RegistrarManager* _Manager;
};


///////////////////////////////////////////////////////////////////////////////
//
// T H E   C L A S S
//
///////////////////////////////////////////////////////////////////////////////

IconServer :: IconServer()
{
	// Create the private class
	_m = new _Private();
}

IconServer :: ~IconServer()
{
	delete _m;
}

Image* IconServer :: GetRootIcon( const os::String path, enum eSize size )
{
	// Look if the path is a mountpoint
	os::Volumes cVolumes;
	os::String cMountPoint;
	for( uint i = 0; i < cVolumes.GetMountPointCount(); i++ )
	{
		if( cVolumes.GetMountPoint( i, &cMountPoint ) == 0 )
		{
			if( path == cMountPoint )
				return _m->GetDriveIcon( path, NULL );
		}
	}
}

Image* IconServer :: GetIcon( const os::String path, enum eSize size )
{
	String zTempType;
	Image* pcIcon;
		
	if( _m->_Manager && _m->_Manager->GetTypeAndIcon( path, os::Point( 24, 24 ),
		&zTempType, &pcIcon ) != 0)
	{
		BitmapImage* pcBitmap = new BitmapImage();
		
		MemFile* pcSource = new MemFile( g_aFolderImage, sizeof( g_aFolderImage ) );
							
		pcBitmap->Load( pcSource );
		delete( pcSource );
		pcIcon = pcBitmap;
	}

	// Add emblems if used
	if( Preferences::GetInstance()->GetUseEmblems() )
	{
		// Check if an emblem has been defined for this file
	}

	return( pcIcon );
}



