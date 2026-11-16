/*  Syllable Dock
 *  Copyright (C) 2005 Arno Klenke
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
#include <util/settings.h>
#include "DockMenu.h"
#include <storage/registrar.h>
#include <cassert>
#include <string>
#include <vector>
#include "resources/Dock.h"

DockMenu::DockMenu( os::Handler* pcHandler, os::Rect cFrame, const char* pzName, os::MenuLayout_e eLayout )
	 	: os::Menu( cFrame, pzName, eLayout )
{
	
	
	/* Get the registrar manager */
	os::RegistrarManager* pcManager = NULL;
	try
	{
		pcManager = os::RegistrarManager::Get();
	} catch( ... )
	{
	}
	
	if( pcManager == NULL )
		return;
	
	/* Get application list */
	os::RegistrarAppList cList = pcManager->GetAppList();
	
	/* Add entries */
	for( int i = 0; i < cList.GetCount(); i++ )
		AddEntry( pcManager, cList.GetName( i ), cList.GetCategory( i ), cList.GetPath( i ), 
			os::String( os::Path( cList.GetPath( i ) ).GetDir().GetLeaf() ).Lower() == "preferences",
			os::Path( cList.GetPath( i ) ).GetDir() );
	
	pcManager->Put();
}

DockMenu::~DockMenu()
{
}

/* The FindItem() method of the Menu class does not return submenus */
static os::MenuItem* find_submenu_item( os::Menu* pcMenu, const os::String& zName )
{
	for( int i = 0; i < pcMenu->GetItemCount(); i++ )
	{
		os::MenuItem* pcItem = pcMenu->GetItemAt( i );
		if( pcItem && pcItem->GetLabel() == zName && ( pcItem->GetSubMenu() != NULL ) )
			return( pcItem );
	}
	return( NULL );
}

os::MenuItem* DockMenu::FindMenuItem( os::String zName )
{
	return( find_submenu_item( this, zName ) );
}

/* The Dock's own folder icon, from its resources */
static os::Image* folder_icon()
{
	os::File cSelf( open_image_file( get_image_id() ) );
	os::Resources cCol( &cSelf );
	os::ResStream* pcStream = cCol.GetResourceStream( "folder.png" );
	os::BitmapImage* pcImg = new os::BitmapImage();
	pcImg->Load( pcStream );
	pcImg->SetSize( os::Point( 24, 24 ) );
	delete( pcStream );
	return( pcImg );
}

void DockMenu::AddEntry( os::RegistrarManager* pcManager, os::String zApplicationName, os::String zCategory, os::Path cPath, bool bUseDirIcon, os::Path cDir )
{
	os::String zTemp;
	os::Image* pcItemIcon = NULL;
	os::Image* pcCategoryIcon = NULL;
	
	//printf( "Add %s %i %s\n", zApplicationName.c_str(), bUseDirIcon, cDir.GetPath().c_str() );
	/* Get the icons */
	if( pcManager )
	{
		os::String zTemp;
		if( pcManager->GetTypeAndIcon( cPath, os::Point( 24, 24 ), &zTemp, &pcItemIcon ) < 0 )
			pcItemIcon = NULL;
		if( bUseDirIcon )
			if( pcManager->GetTypeAndIcon( cDir, os::Point( 24, 24 ), &zTemp, &pcCategoryIcon ) < 0 )
				pcCategoryIcon = NULL;
	}
	
	os::Menu* pcAddMenu = this;
	/* A category can name folders in it: "Demos/Mesa" is the Mesa folder in
	   Other */
	std::vector<os::String> cFolders;
	std::string zFull = zCategory.const_str();
	size_t nSlash = zFull.find( '/' );
	if( nSlash != std::string::npos )
	{
		std::string zRest = zFull.substr( nSlash + 1 );
		zCategory = os::String( zFull.substr( 0, nSlash ) );
		size_t nStart = 0;
		while( nStart <= zRest.size() )
		{
			size_t nEnd = zRest.find( '/', nStart );
			if( nEnd == std::string::npos )
				nEnd = zRest.size();
			if( nEnd > nStart )
				cFolders.push_back( os::String( zRest.substr( nStart, nEnd - nStart ) ) );
			nStart = nEnd + 1;
		}
	}
	if( zCategory != "" )
	{
		os::String zCategoryIcon = zCategory;	// We need the unlocalized name, so we can get the right icon.
		/* Localizable Category */
		if( zCategory == "Preferences" )
			zCategory = MSG_MENU_CATEGORY_PREFERENCES;
		if( zCategory == "Other" )
			zCategory = MSG_MENU_CATEGORY_OTHER;
		if( zCategory == "Internet" )
			zCategory = MSG_MENU_CATEGORY_INTERNET;
		if( zCategory == "Media" )
			zCategory = MSG_MENU_CATEGORY_MEDIA;
		if( zCategory == "Office" )
			zCategory = MSG_MENU_CATEGORY_OFFICE;
		if( zCategory == "Development" )
			zCategory = MSG_MENU_CATEGORY_DEVELOPMENT;
		if( zCategory == "Games" )
			zCategory = MSG_MENU_CATEGORY_GAMES;
		if( zCategory == "Emulators" )
			zCategory = MSG_MENU_CATEGORY_EMULATORS;
		if( zCategory == "System Tools" )
			zCategory = MSG_MENU_CATEGORY_SYSTEMTOOLS;
			
		/* Create category if necessary */
		os::MenuItem* pcSubMenuItem = FindMenuItem( zCategory );
		if( !pcSubMenuItem )
		{
			if( pcCategoryIcon == NULL )
			{
				try
				{
					os::String cCategoryIcon = os::String( "/system/icons/Dock/" ) + zCategoryIcon + os::String( ".png" );
					os::StreamableIO* pcStream = new os::File( cCategoryIcon );
					os::BitmapImage* pcImg = new os::BitmapImage();
					pcImg->Load( pcStream );
					pcImg->SetSize( os::Point( 24, 24 ) );
					pcCategoryIcon = pcImg;
					delete( pcStream );
				} catch( ... )
				{
					os::File cSelf( open_image_file( get_image_id() ) );
					os::Resources cCol( &cSelf );		
					os::ResStream* pcStream = cCol.GetResourceStream( "folder.png" );
					os::BitmapImage* pcImg = new os::BitmapImage();
					pcImg->Load( pcStream );
					pcImg->SetSize( os::Point( 24, 24 ) );
					pcCategoryIcon = pcImg;
					delete( pcStream );
				}
			}
			os::Menu* pcMenu = new os::Menu( os::Rect(), zCategory, os::ITEMS_IN_COLUMN );
			AddItem( new os::MenuItem( pcMenu, NULL, "", pcCategoryIcon ) );
			assert( ( pcSubMenuItem = FindMenuItem( zCategory ) ) != NULL );
		}
		if( pcSubMenuItem->GetSubMenu() )
			pcAddMenu = pcSubMenuItem->GetSubMenu();
	}
	/* The folders in the category, made if necessary */
	for( uint i = 0; i < cFolders.size(); i++ )
	{
		os::MenuItem* pcFolderItem = find_submenu_item( pcAddMenu, cFolders[i] );
		if( !pcFolderItem )
		{
			os::Menu* pcMenu = new os::Menu( os::Rect(), cFolders[i], os::ITEMS_IN_COLUMN );
			pcFolderItem = new os::MenuItem( pcMenu, NULL, "", folder_icon() );
			pcAddMenu->AddItem( pcFolderItem );
		}
		pcAddMenu = pcFolderItem->GetSubMenu();
	}
	/* Create the menu item */
	os::Message *pcMsg = new os::Message( 10002 );
	pcMsg->AddString( "file/path", cPath.GetPath() );
	
	pcAddMenu->AddItem( new os::MenuItem( zApplicationName, pcMsg, "", pcItemIcon ) );
}








