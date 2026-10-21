/*  Syllable Dock
 *  Copyright (C) 2003 Arno Klenke
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
 
#include <gui/menu.h>
#include <gui/image.h>
#include <storage/directory.h>
#include <storage/fsnode.h>
#include <storage/file.h>
#include <util/string.h>
#include <util/resources.h>
#include "bookmarks.h"
#include "messages.h"
#include <storage/registrar.h>


BookmarksMenu::BookmarksMenu( os::Handler* pcHandler, os::Path cPath, os::Rect cFrame, const char* pzName, os::MenuLayout_e eLayout )
	 	: os::Menu( cFrame, pzName, eLayout )
{
	m_cPath = cPath;
	m_pcHandler = pcHandler;
	/* Create a nodemonitor for this directory */
	m_pcMonitor = new os::NodeMonitor( cPath.GetPath(), NWATCH_ALL, pcHandler );
	Reload();
}

void BookmarksMenu::Reload()
{
	/* Clear */
	os::MenuItem* pcItem;
	while( ( pcItem = RemoveItem( 0 ) ) != NULL )
		delete( pcItem );
	
	/* Check if the directory is valid */
	os::FSNode* pcNode = NULL;
	try
	{
		pcNode = new os::FSNode( m_cPath.GetPath() );
	} catch( ... ) {
		return;
	}
	if( !pcNode->IsValid() )
	{
		delete( pcNode );
		return;
	}
	delete( pcNode );
	
	/* Iterate through the directory */
	os::Directory cDir( m_cPath.GetPath() );
	cDir.Rewind();
	os::String zFile;
	
	/* Get the registrar manager */
	os::RegistrarManager* pcManager = NULL;
	try
	{
		pcManager = os::RegistrarManager::Get();
	} catch( ... )
	{
	}
	
	os::Message* pcAddMsg = new os::Message( ID_ADD_BOOKMARK );
	pcAddMsg->AddString( "path", m_cPath.GetPath() );
	AddItem( new os::MenuItem( "Add this page", pcAddMsg, "" ) );
	os::Message* pcManageMsg = new os::Message( ID_MANAGE_BOOKMARKS );
	pcManageMsg->AddString( "path", m_cPath.GetPath() );
	AddItem( new os::MenuItem( "Manage...", pcManageMsg, "" ) );
	AddItem( new os::MenuSeparator() );
	
	while( cDir.GetNextEntry( &zFile ) == 1 )
	{
		if( zFile == os::String( "Launcher1.cfg" ) ||
			zFile == os::String( "." ) ||
			zFile == os::String( ".." ) )
			continue;
		os::Path cFilePath = m_cPath;
		
		cFilePath.Append( zFile.c_str() );
		
		os::Image* pcItemIcon = NULL;
		
		/* Get icon */
		if( pcManager )
		{
			os::String zTemp;
			if( pcManager->GetTypeAndIcon( cFilePath, os::Point( 16, 16 ), &zTemp, &pcItemIcon ) < 0 )
				continue;
		}
		
		/* Validate entry */
		try {
			pcNode = new os::FSNode( cFilePath );
		} catch( ... ) {
			continue;
		}
		if( !pcNode->IsValid() )
		{
			delete( pcNode );
			continue;
		}
			
		if( pcNode->IsFile() )
		{
			/* Create an menu item */
			os::Message *pcMsg = new os::Message( ID_GO_BOOKMARK );
			pcMsg->AddString( "file/path", cFilePath.GetPath() );
			
			AddItem( new os::MenuItem( zFile.c_str(), pcMsg, "", pcItemIcon ) );
		}
		if( pcNode->IsDir() )
		{
			/* Create another DirMenu */
			BookmarksMenu* pcMenu = new BookmarksMenu( m_pcHandler, cFilePath, os::Rect(), zFile.c_str(), os::ITEMS_IN_COLUMN ) ;
			AddItem( new os::MenuItem( pcMenu, NULL, "", pcItemIcon ) );
		}
		delete( pcNode );
	}
	
	if( pcManager )
		pcManager->Put();
}

BookmarksMenu::~BookmarksMenu()
{
	delete( m_pcMonitor );
}

