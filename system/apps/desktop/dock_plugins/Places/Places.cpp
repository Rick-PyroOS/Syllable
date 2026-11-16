/*
    A 'Places' utility for Dock

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/


//TODO: Monitor for drive changes
//TODO: Monitor registrar for 'recent' file changes
//TODO: Add settings dialog
//TODO: Drag/Drop
                                                                                                                   
#include <vector>
#include <gui/button.h>
#include <gui/imagebutton.h>
#include <gui/layoutview.h>
#include <gui/frameview.h>
#include <gui/stringview.h>
#include <util/resources.h>
#include <util/settings.h>
#include <gui/requesters.h>
#include <gui/dropdownmenu.h>
#include <gui/menu.h>
#include <util/datetime.h>
#include <gui/desktop.h>
#include <gui/image.h>
#include <gui/checkbox.h>
#include <storage/file.h>
#include <atheos/time.h>
#include <storage/streamableio.h>
#include <atheos/filesystem.h>
#include <storage/volumes.h>
#include <iostream>
#include <appserver/dockplugin.h>
#include <gui/treeview.h>
#include <storage/directory.h>
#include <storage/nodemonitor.h>
#include <gui/filerequester.h>
#include <gui/fontrequester.h>
#include <gui/spinner.h>
#include <util/event.h>
#include <storage/registrar.h>
#include <utility>
#include <vector>
#include <unistd.h>

#include "popupmenu.h"
#include "progressbar.h"
#include "fonthandle.h"

using namespace os;
using namespace os_priv2;
using namespace std;                                                                                                                                                                                                     



typedef std::vector<pair<os::String,os::String> >::iterator defaultPlacesIterator;
typedef std::map<os::String,std::pair<String,String> >::iterator customPlacesIterator;
typedef std::map<os::String,std::pair<String,String> >::const_iterator const_customPlacesIterator;
typedef std::vector<os::String>::iterator recentIterator;
os::Path m_cPath;                                                                                                                           

os::BitmapImage* LoadImage(const os::Path& cPath,const os::String& cName)
{
	/* Load default icons */
	os::File* pcFile = new os::File( cPath );
	os::Resources cCol( pcFile );
	os::ResStream* pcStream = cCol.GetResourceStream( cName);
	os::BitmapImage* m_pcIcon = new os::BitmapImage( pcStream );
	delete pcFile;
	return m_pcIcon;
}
enum
{
	M_PREFS=60000,
	M_PREFS_APPLY,
	M_PREFS_DEFAULT,
	M_PREFS_UNDO,
	M_PREFS_ADD,
	M_CUSTOM_PLACES_INVOKED,
	M_CUSTOM_PLACES_MOVE_UP,
	M_CUSTOM_PLACES_MOVE_DOWN,
	M_PREFS_CUSTOM_PLACE_APPLY,
	M_PREFS_CUSTOM_PLACE_CANCEL,
	M_PREFS_CUSTOM_PLACE_ICON_REQUESTED,
	M_PREFS_CUSTOM_PLACE_ICON_HIT,
	M_PREFS_REMOVE,
	M_PREFS_BROWSER_CHANGED,
	M_PREFS_BROWSE,
	M_PREFS_BROWSE_HIT,
	M_PREFS_FONT_REQUESTED,
	M_PREFS_ICON_SIZE,
	M_CHECK_DRIVES_TIMER,
	M_CHECKED,
	M_MENU_HIT
};

struct Prefs
{
	bool m_bShowDrives;
	bool m_bShowHome;
	bool m_bShowDocuments;
	bool m_bShowDesktop;
	bool m_bShowPictures;
	bool m_bShowDownloads;
	bool m_bShowApplications;
	bool m_bShowPreferences;
	bool m_bShowTrash;
	bool m_bShowRecent;
	bool m_bNotifyIfNotExist;
	os::String m_cFileBrowser;
	int m_nIconSize;
	FontHandle* m_pcFontHandle;
	
	std::vector<pair<os::String,os::String> > m_cDefaultPlaces;
	std::vector<pair<os::String,os::String> > m_cHomePlaces;
	std::map<os::String,pair<os::String,os::String> > m_customPlaces;
	std::vector<os::String> m_recentFiles;
	
};

class PrefsWindow : public os::Window
{
	virtual void Load();
	virtual void Validate();
};

class Drive
{
public:
	Drive(os::BitmapImage* icon,int iconSize, double free=0);
	
	void SetSize(const os::Point&);
	os::Point GetSize() const;
	void SetIcon(os::BitmapImage* icon);
	void SetProgress(float v);
	os::Color32 GetProgressColor(float v);
public:
	os::BitmapImage* m_pcImage;
	os::BitmapImage* m_pcIcon;
	os_priv::ProgressBar* m_pcProgress;
	int m_nIconSize;
};

Drive::Drive(os::BitmapImage* icon,int iconSize, double free)
{
	m_pcIcon = icon;
	m_nIconSize = iconSize;
	
	m_pcImage = new BitmapImage( Bitmap::SHARE_FRAMEBUFFER | Bitmap::ACCEPT_VIEWS );
	
	os::Point cSize = os::Point(iconSize+10,iconSize+10);
	m_pcImage->ResizeCanvas(cSize);
	m_pcImage->SetSize(cSize);
	
	View* pcView = m_pcImage->GetView();
	Rect cRect( pcView->GetBounds() );
	
	m_pcIcon->Draw(os::Point(0,0),pcView);
	
	cRect.Resize((iconSize),4,0,-4);
	m_pcProgress = new os_priv::ProgressBar(cRect,"progress",os::VERTICAL);
	

	pcView->AddChild(m_pcProgress);
	m_pcProgress->SetProgress(1.0f - free);
	m_pcProgress->SetProgressColor(GetProgressColor(m_pcProgress->GetProgress()));
	m_pcProgress->SetLabel(os::String().Format("%.1f free",free*100));
	m_pcProgress->Paint(cRect);

	m_pcImage->Sync();
	
}

os::Point Drive::GetSize() const
{
	float y = m_nIconSize;
	float x = m_nIconSize+25;
	return os::Point(y,x);
}

void Drive::SetSize(const os::Point& s)
{
	m_pcImage->ResizeCanvas(s);
	m_pcImage->Sync();
}

void Drive::SetProgress(float v)
{
	m_pcProgress->SetProgress(1.0f - v);
	
	delete m_pcImage;
	m_pcImage = new BitmapImage( Bitmap::SHARE_FRAMEBUFFER | Bitmap::ACCEPT_VIEWS );
	
	os::Point cSize = os::Point(m_nIconSize+10,m_nIconSize+10);
	m_pcImage->ResizeCanvas(cSize);
	m_pcImage->SetSize(cSize);
	
	View* pcView = m_pcImage->GetView();
	Rect cRect( pcView->GetBounds() );
	
	m_pcIcon->Draw(os::Point(0,0),pcView);

	cRect.Resize((m_nIconSize),4,0,-4);	
	
	m_pcProgress->SetProgressColor(GetProgressColor(m_pcProgress->GetProgress()));

	m_pcProgress->Paint(cRect);
	m_pcImage->Sync();
}

os::Color32 Drive::GetProgressColor(float v)
{
	if (v > 0.75 && v < 0.9)
	{
		return (os::Color32(255,244,0));
	}
	else if (v >= 0.9)
	{
		return(os::Color32(255,0,0));
	}
	else
	{
		return(get_default_color(COL_FOCUS));
	}		
}

class DriveMenuItem : public os::MenuItem
{
public:
	DriveMenuItem(const os::String&,os::Message*,int,float);
public:
	Drive* m_pcDrive;
};

DriveMenuItem::DriveMenuItem(const os::String& name,os::Message* msg,int iconSize,float free) : MenuItem(name,msg,"")
{
	File cFile("/system/icons/disk.png");
	os::BitmapImage* c = new BitmapImage(&cFile);	
	c->SetSize(os::Point(iconSize,iconSize));
	m_pcDrive = new Drive(c,iconSize,free);
	SetImage(m_pcDrive->m_pcImage);
}


class PrefsWin : public os::Window
{
public:
	PrefsWin(os::Handler* parent);
	virtual bool Validate();
};

class PlacesPrefs : public os::Window
{
	public:
		PlacesPrefs(os::View*,os::Bitmap*);
		
		void SetPrefs(const Prefs& p);
		Prefs GetPrefs() const;
		
		void HandleMessage(os::Message*);
		bool OkToQuit();
		bool CheckBrowserPath(const os::String& path);
		void MakeFocus(bool focus=true);
	private:
		void _Layout();
		void RecalculatePrefs();
	private:
		Prefs m_cPrefs;
		os::LayoutView* v;
		os::CheckBox* m_pcApplicationsCheck;
		os::CheckBox* m_pcPreferencesCheck;
		os::CheckBox* m_pcTrashCheck;		
		
		os::CheckBox* m_pcHomeCheck;
		os::CheckBox* m_pcDesktopCheck;
		os::CheckBox* m_pcDocumentsCheck;
		os::CheckBox* m_pcDownloadsCheck;
		os::CheckBox* m_pcPicturesCheck;

		os::CheckBox* m_pcDrivesCheck;
		os::CheckBox* m_pcRecentCheck;
		
		os::CheckBox* m_pcNotifyCheck;
		
		os::TextView* m_pcBrowserTextView;
		os::StringView* m_pcBrowserInfo;
		os::ImageButton* m_pcBrowseButton;
		os::FileRequester* m_pcBrowseRequester;

		os::StringView* m_pcFontInfo;
		os::ImageButton* m_pcFontSelection;
		os::FontRequester* m_pcFontRequester;
		os::Spinner* m_pcIconSize;
			
		os::TreeView* m_pcCustomPlacesListView;
		os::Button* m_pcAddCustomPlaceButton;
		os::Button* m_pcRemoveCustomPlaceButton;
		
		os::Button* m_pcApplyButton;
		os::Button* m_pcUndoButton;
		os::Button* m_pcDefaultButton;

		os::ImageButton* m_pcUpButton;
		os::ImageButton* m_pcDownButton;
		
		View* m_pcParent;

		private:	
		class CustomPlacesWin : public os::Window
		{
			public:
				CustomPlacesWin(os::Handler*);
				void HandleMessage(os::Message*);
				void Clear();
				void SetPrefs(const os::String& title,const os::String& path,const os::String& icon);
				void MakeFocus( bool bFocus = true );
			private:
				void _Layout();
				void _SetIcon(const os::String&);
			private:
				os::LayoutView* m_pcView;
				os::TextView* m_pcPath;
				os::TextView* m_pcTitle;
				os::ImageButton* m_pcIcon;
				os::Button* m_pcOk;
				os::Button* m_pcCancel;
				os::Handler* m_pcParent;
				os::String m_cIcon;
				os::FileRequester* m_pcIconRequester;
				os::ImageButton* m_pcBrowseButton;
		};
		
		CustomPlacesWin* m_pcCustomPlacesWin;
};


PlacesPrefs::CustomPlacesWin::CustomPlacesWin(os::Handler* parent) : os::Window(os::Rect(0,0,300,100),"custom_places","Custom Place:",WND_NO_ZOOM_BUT|WND_NO_DEPTH_BUT| WND_MODAL | WND_NO_CLOSE_BUT | WND_NOT_RESIZABLE)
{
	m_pcParent = parent;
	m_pcIconRequester = new os::FileRequester(os::FileRequester::LOAD_REQ,new os::Messenger(this),"/system/bin",os::FileRequester::NODE_FILE,false,new os::Message(M_PREFS_CUSTOM_PLACE_ICON_HIT),NULL,true);
	
	_Layout();
}

void PlacesPrefs::CustomPlacesWin::MakeFocus( bool bFocus)
{
	Window::MakeFocus(bFocus);
	
	if (bFocus)
	{
		SetFocusChild(m_pcTitle);
		SetDefaultButton(m_pcOk);
	}
}

void PlacesPrefs::CustomPlacesWin::_Layout()
{
	m_pcView = new os::LayoutView(GetBounds(),"custom_places_win");
	
	os::VLayoutNode* root = new os::VLayoutNode("root");
	root->SetBorders(os::Rect(5,5,5,5));
	os::HLayoutNode* main = new os::HLayoutNode("main");
	
	os::VLayoutNode* n = new os::VLayoutNode("icon_node");
	n->AddChild(m_pcIcon = new os::ImageButton(os::Rect(),"","",new os::Message(M_PREFS_CUSTOM_PLACE_ICON_REQUESTED),NULL,os::ImageButton::IB_TEXT_BOTTOM,true,true,false));
	m_pcIcon->SetTabOrder();
	n->LimitMaxSize(os::Point(48,48));
	main->AddChild(n);
	main->AddChild(new os::HLayoutSpacer("",5,5));
	
	n = new os::VLayoutNode("text_node");
	n->AddChild(new os::VLayoutSpacer("",10,10));
	
	os::HLayoutNode* t = new os::HLayoutNode("");
	t->AddChild(new os::StringView(os::Rect(),"title_string","Title:"));
	t->AddChild(new os::HLayoutSpacer("",5,5));
	t->AddChild(m_pcTitle = new os::TextView(os::Rect(),"title_text",""));
	m_pcTitle->SetTabOrder(NEXT_TAB_ORDER);
	t->AddChild(new os::ImageButton(os::Rect(),"dummy_but","",NULL,NULL));
	n->AddChild(t);
	
	t = new os::HLayoutNode("");
	t->AddChild(new os::StringView(os::Rect(),"path_string","Path:"));
	t->AddChild(new os::HLayoutSpacer("",5,5));
	t->AddChild(m_pcPath = new os::TextView(os::Rect(),"path_text",""));
	m_pcPath->SetTabOrder(NEXT_TAB_ORDER);
	os::BitmapImage* i = LoadImage(m_cPath,"icon24x24.png");
	i->SetSize(os::Point(24,24));
	m_pcBrowseButton = new os::ImageButton(os::Rect(),"browse_button","",new os::Message(M_PREFS_BROWSE),i);
	m_pcBrowseButton->SetTabOrder();
	t->AddChild(m_pcBrowseButton);	
	n->AddChild(t);

	
	n->SameWidth("path_string","title_string",NULL);
	n->SameHeight("path_string","title_string",NULL);
	n->SameWidth("dummy_but","browse_button",NULL);

	main->AddChild(n);
	root->AddChild(main);
			
	t = new os::HLayoutNode("");
	t->AddChild(m_pcOk = new os::Button(os::Rect(),"okay_but","OK",new os::Message(	M_PREFS_CUSTOM_PLACE_APPLY)));
	m_pcOk->SetTabOrder();

	t->AddChild(m_pcCancel = new os::Button(os::Rect(),"cancel_but","Cancel",new os::Message(M_PREFS_CUSTOM_PLACE_CANCEL)));
	m_pcCancel->SetTabOrder();
	t->SameWidth("okay_but","cancel_but",NULL);
	t->SameHeight("okay_but","cancel_but",NULL);
	root->AddChild(t);

	m_pcView->SetRoot(root);

	AddChild(m_pcView);
}

void PlacesPrefs::CustomPlacesWin::HandleMessage(os::Message* pcMessage)
{
	switch (pcMessage->GetCode())
	{
		case M_PREFS_CUSTOM_PLACE_ICON_HIT:
		{
			os::String icon;
			if (pcMessage->FindString("file/path",&icon)==0)
			{
				m_cIcon = icon;
				_SetIcon(m_cIcon);
			}			
			break;
		}
		case M_PREFS_CUSTOM_PLACE_ICON_REQUESTED:
		{
			m_pcIconRequester->Show();
			m_pcIconRequester->MakeFocus();		
			break;
		}
		case M_PREFS_CUSTOM_PLACE_APPLY:
		{
			pcMessage->AddString("title",m_pcTitle->GetValue().AsString());
			pcMessage->AddString("path",m_pcPath->GetValue().AsString());
			pcMessage->AddString("icon",m_cIcon);
			m_pcParent->HandleMessage(pcMessage);
			break;
		}
		default:
		{
			m_pcParent->HandleMessage(pcMessage);
			break;
		}
	}	
}

void PlacesPrefs::CustomPlacesWin::Clear()
{
	m_cIcon = "";
	m_pcTitle->Clear(false);
	m_pcPath->Clear(false);
	_SetIcon(m_cIcon);
}

void PlacesPrefs::CustomPlacesWin::SetPrefs(const os::String& title,const os::String& path,const os::String& icon)
{
	m_pcTitle->SetValue(title);
	m_pcPath->SetValue(path);
	m_cIcon = icon;
	_SetIcon(m_cIcon);
}

void PlacesPrefs::CustomPlacesWin::_SetIcon(const os::String& icon)
{
	if (icon != "")
	{
		os::String type;
		os::String mime;
		os::Message cMsg;
		os::RegistrarManager* pcManager = os::RegistrarManager::Get();
		pcManager->GetTypeAndIcon( icon, os::Point(24,24), &type, NULL,&cMsg);
		cMsg.FindString("mimetype",&mime);

		if (mime.find("image/",0)==0)
		{
			try
			{
				os::File file(icon);
				os::BitmapImage* image = new os::BitmapImage(&file);
				m_pcIcon->SetImage(image);
				return;
			}
			catch(...)
			{
				m_cIcon = "/system/icons/file.png";
				os::File file(m_cIcon);
				os::BitmapImage* image = new os::BitmapImage(&file);
				m_pcIcon->SetImage(image);
				return;
			}
		}
	}
	
	m_cIcon = "/system/icons/file.png";
	os::File file(m_cIcon);
	os::BitmapImage* image = new os::BitmapImage(&file);
	m_pcIcon->SetImage(image);	
}

PlacesPrefs::PlacesPrefs(os::View* parent,os::Bitmap* icon) : Window(os::Rect(0,0,500,500),"places_prefs","Places Preferences",WND_NO_ZOOM_BUT|WND_NO_DEPTH_BUT| WND_MODAL | WND_NO_CLOSE_BUT)
{
	m_pcParent = parent;
	m_pcBrowseRequester = new os::FileRequester(os::FileRequester::LOAD_REQ,new os::Messenger(this),"/system/bin",os::FileRequester::NODE_FILE,false,new os::Message(M_PREFS_BROWSE_HIT),NULL,true);
	m_pcFontRequester = new os::FontRequester(new os::Messenger(this),false);
	m_pcCustomPlacesWin = new CustomPlacesWin(this);
	m_pcCustomPlacesWin->CenterInScreen();
	SetIcon(icon);
	v = new os::LayoutView(GetBounds(),"places_prefs_layout");
	_Layout();
	
	AddChild(v);
}

bool PlacesPrefs::OkToQuit()
{
	return false;
}

void PlacesPrefs::SetPrefs(const Prefs& p)
{
	m_cPrefs = p;
	
	m_pcApplicationsCheck->SetValue(p.m_bShowApplications);
	m_pcDrivesCheck->SetValue(p.m_bShowDrives);
	m_pcHomeCheck->SetValue(p.m_bShowHome);
	m_pcDesktopCheck->SetValue(p.m_bShowDesktop);
	m_pcDocumentsCheck->SetValue(p.m_bShowDocuments);
	m_pcDownloadsCheck->SetValue(p.m_bShowDownloads);
	m_pcPicturesCheck->SetValue(p.m_bShowPictures);	
	
	m_pcPreferencesCheck->SetValue(p.m_bShowPreferences);
	m_pcTrashCheck->SetValue(p.m_bShowTrash);
	m_pcRecentCheck->SetValue(p.m_bShowRecent);
	m_pcBrowserTextView->SetValue(p.m_cFileBrowser);
	m_pcNotifyCheck->SetValue(p.m_bNotifyIfNotExist);
	m_pcIconSize->SetValue(p.m_nIconSize);
	
	os::Font* f = m_pcFontInfo->GetFont();
	
	os::String c = os::String().Format("%s %s, %.1f",f->GetFamily().c_str(),f->GetStyle().c_str(),f->GetSize());
	m_pcFontInfo->SetString(c);

	for (const_customPlacesIterator i = p.m_customPlaces.begin(); i!=p.m_customPlaces.end(); i++)
	{
			os::String title = i->first;
			os::String path = i->second.first;
			os::String icon = i->second.second;
			os::TreeViewStringNode* n = new os::TreeViewStringNode();
			n->AppendString("");
			n->AppendString(title);
			n->AppendString(path);
			n->SetCookie(icon);
			try
			{
				os::File cIconFile( icon );
				os::BitmapImage* pcIcon = new os::BitmapImage();
				if( pcIcon->Load( &cIconFile ) == 0 )
				{
					pcIcon->SetSize( os::Point( m_cPrefs.m_nIconSize,m_cPrefs.m_nIconSize ) );
					n->SetIcon( pcIcon );
				}
			} catch( ... )
			{
			}
			
			m_pcCustomPlacesListView->InsertNode(n,false);
	}
	m_pcCustomPlacesListView->RefreshLayout();
}

Prefs PlacesPrefs::GetPrefs() const
{
	return m_cPrefs;
}

void PlacesPrefs::_Layout()
{
	os::FrameView* frame = new os::FrameView(GetBounds(),"frame","Preferences");
	
	os::VLayoutNode* root = new os::VLayoutNode("root");
	root->SetBorders(os::Rect( 5, 0, 5, 0 ));
	
	os::HLayoutNode* n = new os::HLayoutNode("applications_node");
	n->AddChild(m_pcApplicationsCheck = new os::CheckBox(os::Rect(),"applications_check","Show Applications",new os::Message(M_CHECKED)));
	m_pcApplicationsCheck->SetTabOrder();
	root->AddChild(n);

	n = new os::HLayoutNode("prefs_node");
	n->AddChild(m_pcPreferencesCheck = new os::CheckBox(os::Rect(),"preferences_check","Show Preferences",new os::Message(M_CHECKED)));
	m_pcPreferencesCheck->SetTabOrder();
	root->AddChild(n);
	
	n = new os::HLayoutNode("trash_node");
	n->AddChild(m_pcTrashCheck = new os::CheckBox(os::Rect(),"trash_check","Show Trash",new os::Message(M_CHECKED)));
	m_pcTrashCheck->SetTabOrder();
	root->AddChild(n);
	
	n = new os::HLayoutNode("home_node");
	n->AddChild(m_pcHomeCheck = new os::CheckBox(os::Rect(),"home_check","Show Home",new os::Message(M_CHECKED)));
	m_pcHomeCheck->SetTabOrder();
	root->AddChild(n);

	n = new os::HLayoutNode("desktop_node");
	n->AddChild(m_pcDesktopCheck = new os::CheckBox(os::Rect(),"desktop_check","Show Desktop",new os::Message(M_CHECKED)));
	m_pcDesktopCheck->SetTabOrder();
	root->AddChild(n);
		
	n = new os::HLayoutNode("documents_node");
	n->AddChild(m_pcDocumentsCheck = new os::CheckBox(os::Rect(),"documents_check","Show Documents",new os::Message(M_CHECKED)));
	m_pcDocumentsCheck->SetTabOrder();
	root->AddChild(n);
	
	n = new os::HLayoutNode("downloads_node");
	n->AddChild(m_pcDownloadsCheck = new os::CheckBox(os::Rect(),"downloads_check","Show Downloads",new os::Message(M_CHECKED)));
	m_pcDownloadsCheck->SetTabOrder();
	root->AddChild(n);
	
	n = new os::HLayoutNode("pictures_node");
	n->AddChild(m_pcPicturesCheck = new os::CheckBox(os::Rect(),"pictures_check","Show Pictures",new os::Message(M_CHECKED)));
	m_pcPicturesCheck->SetTabOrder();
	root->AddChild(n);
	
	n = new os::HLayoutNode("drives_node");
	n->AddChild(m_pcDrivesCheck = new os::CheckBox(os::Rect(),"drives_check","Show Drives",new os::Message(M_CHECKED)));
	m_pcDrivesCheck->SetTabOrder();
	root->AddChild(n);
	
	n = new os::HLayoutNode("recent_node");
	n->AddChild(m_pcRecentCheck = new os::CheckBox(os::Rect(),"recent_check","Show Recent Files",new os::Message(M_CHECKED)));
	m_pcRecentCheck->SetTabOrder();
	root->AddChild(n);
	
	n = new os::HLayoutNode("notify_node");
	n->AddChild(m_pcNotifyCheck = new os::CheckBox(os::Rect(),"notify_check","Notify If Directory Does Not Exist",new os::Message(M_CHECKED)));
	m_pcNotifyCheck->SetTabOrder();
	root->AddChild(n);

	n = new os::HLayoutNode("browser_main_node",0.0f);

	os::HLayoutNode* browser_node = new os::HLayoutNode("browser_node");
	os::StringView* browser_label = new os::StringView(os::Rect(),"browser_label_string","FileBrowser:");
	browser_node->AddChild(browser_label,0.08);

	
	m_pcBrowserTextView = new os::TextView(os::Rect(),"browser_text","");
	m_pcBrowserTextView->SetEventMask(os::TextView::EI_CONTENT_CHANGED|os::TextView::EI_FOCUS_LOST | os::TextView::EI_ENTER_PRESSED);
	m_pcBrowserTextView->SetMessage(new os::Message(M_PREFS_BROWSER_CHANGED));
	m_pcBrowserTextView->SetTabOrder(NEXT_TAB_ORDER);
	browser_node->AddChild(m_pcBrowserTextView,0.967f);
	
	os::BitmapImage* i = new os::BitmapImage(new os::File("/system/icons/folder.png"));
	i->SetSize(os::Point(24,24));
	m_pcBrowseButton = new os::ImageButton(os::Rect(),"browse_button","",new os::Message(M_PREFS_BROWSE),i);
	m_pcBrowseButton->SetTabOrder();
	browser_node->AddChild(m_pcBrowseButton,0.025);
	
	
	os::HLayoutNode* browser_info_node = new os::HLayoutNode("browser_info_node");
	browser_info_node->AddChild(new os::HLayoutSpacer(""));
	browser_info_node->AddChild(m_pcBrowserInfo= new os::StringView(os::Rect(),"browser_info","Please enter full path",DTF_ALIGN_RIGHT));
	browser_info_node->AddChild(new os::HLayoutSpacer(""));
	
	

	os::VLayoutNode* b_node = new os::VLayoutNode("");
	b_node->AddChild(browser_node);
	b_node->AddChild(browser_info_node);
	n->AddChild(b_node);
	root->AddChild(n);
	
	n = new os::HLayoutNode("icon_node");
	n->AddChild(new os::StringView(os::Rect(),"icon_size_string","Select Icon Size:"));
	n->AddChild(new os::HLayoutSpacer("",5,5));
	n->AddChild(m_pcIconSize = new  os::Spinner(os::Rect(),"icon_size_spinner",0,new os::Message(M_PREFS_ICON_SIZE)));
	m_pcIconSize->SetTabOrder();
	m_pcIconSize->SetStep(8);
	m_pcIconSize->SetMinMax(8,48);
	m_pcIconSize->SetScale(1);
	root->AddChild(n);
	
	n = new os::HLayoutNode("font_node");
	n->AddChild(new os::StringView(os::Rect(),"font_info_string","Font:"));
	n->AddChild(new os::HLayoutSpacer("",5,5));
	n->AddChild(m_pcFontInfo = new os::StringView(os::Rect(),"font_info",""),8.967f);
	
	os::BitmapImage* f = LoadImage(m_cPath,"font.png");
	f->SetSize(os::Point(24,24));
	n->AddChild(m_pcFontSelection = new os::ImageButton(os::Rect(),"font_selection","",new os::Message(M_PREFS_FONT_REQUESTED),f),0.025f);
	m_pcFontSelection->SetTabOrder();
	
	root->AddChild(n);
	root->SameWidth("icon_size_string","font_info_string","browser_label_string",NULL);
	root->SameWidth("browser_node","icon_node","font_node",NULL);
	frame->SetRoot(root);
	
	os::FrameView* customFrameView = new os::FrameView(os::Rect(),"custom_frame","Custom Places");
	os::VLayoutNode* r = new os::VLayoutNode("r");
	r->SetBorders(os::Rect(5,5,5,5));
	
	n = new os::HLayoutNode("custom_listview_node");

	os::HLayoutNode* b = new os::HLayoutNode("");
	n->AddChild(m_pcCustomPlacesListView = new os::TreeView(os::Rect(),"custom_listview", os::ListView::F_RENDER_BORDER, 0, os::WID_FULL_UPDATE_ON_RESIZE),30);
	m_pcCustomPlacesListView->InsertColumn("Icon",50);
	m_pcCustomPlacesListView->InsertColumn("Title",150);
	m_pcCustomPlacesListView->InsertColumn("Path",150);
	m_pcCustomPlacesListView->SetInvokeMsg( new os::Message( M_CUSTOM_PLACES_INVOKED ) );
	n->AddChild(b);
	
	n->AddChild(new os::HLayoutSpacer("",5,5));

	os::VLayoutNode* move_node = new os::VLayoutNode("move_node");
	os::BitmapImage* up = LoadImage(m_cPath,"up.png");
	up->SetSize(os::Point(24,24));	
	m_pcUpButton = new os::ImageButton(os::Rect(),"up_button","",new os::Message(M_CUSTOM_PLACES_MOVE_UP),up);
	m_pcUpButton->SetTabOrder();
	move_node->AddChild(new os::VLayoutSpacer("",35,35));
	move_node->AddChild(m_pcUpButton);
	
	
	os::BitmapImage* down = LoadImage(m_cPath,"down.png");
	down->SetSize(os::Point(24,24));
	m_pcDownButton = new os::ImageButton(os::Rect(),"down_button","",new os::Message(M_CUSTOM_PLACES_MOVE_DOWN),down);
	
	m_pcDownButton->SetTabOrder();
	move_node->AddChild(m_pcDownButton);
	move_node->AddChild(new os::VLayoutSpacer(""));
	move_node->SameWidth("down_button","up_button",NULL);
	move_node->SameHeight("down_button","up_button",NULL);
	
	n->AddChild(move_node);
	r->AddChild(n);
	n = new os::HLayoutNode("custom_buttons");
	n->AddChild(new os::HLayoutSpacer( "" ));
	n->AddChild(m_pcAddCustomPlaceButton = new os::Button(os::Rect(),"custom_button_add","Add",new os::Message(M_PREFS_ADD)));
	n->AddChild(m_pcRemoveCustomPlaceButton = new os::Button(os::Rect(),"custom_button_remove","Remove",new os::Message(M_PREFS_REMOVE)));
	n->SameWidth("custom_button_add","custom_button_remove",NULL);
	n->SameHeight("custom_button_add","custom_button_remove",NULL);	
	r->AddChild(n);
	
	customFrameView->SetRoot(r);
	
	
	n = new os::HLayoutNode("buttons_node");
	n->AddChild(m_pcApplyButton = new os::Button(os::Rect(),"apply_button","Apply",new os::Message(M_PREFS_APPLY)));
	m_pcApplyButton->SetTabOrder();
	n->AddChild(m_pcUndoButton = new os::Button(os::Rect(),"undo_button","Undo",NULL));
	m_pcUndoButton->SetTabOrder();
	n->AddChild(m_pcDefaultButton= new os::Button(os::Rect(),"default_button","Default",NULL));
	m_pcDefaultButton->SetTabOrder();
	n->SameWidth("apply_button","undo_button","default_button",NULL);
	n->SameHeight("apply_button","undo_button","default_button",NULL);
	
	r = new os::VLayoutNode("main_node");
	r->SetBorders(os::Rect(5,5,5,5));
	r->AddChild(frame,0.3);
	r->AddChild(new os::VLayoutSpacer("",5,5));
	r->AddChild(customFrameView);
	r->AddChild(new os::VLayoutSpacer("",5,5));
	r->AddChild(n);
	
	v->SetRoot(r);
}

void PlacesPrefs::RecalculatePrefs()
{
	m_cPrefs.m_bShowApplications = m_pcApplicationsCheck->GetValue().AsBool();
	m_cPrefs.m_bShowDrives = m_pcDrivesCheck->GetValue().AsBool();
	m_cPrefs.m_bShowHome = m_pcHomeCheck->GetValue().AsBool();
	m_cPrefs.m_bShowDesktop = m_pcDesktopCheck->GetValue().AsBool();
	m_cPrefs.m_bShowDocuments = m_pcDocumentsCheck->GetValue().AsBool();
	m_cPrefs.m_bShowDownloads = m_pcDownloadsCheck->GetValue().AsBool();
	m_cPrefs.m_bShowPictures = m_pcPicturesCheck->GetValue().AsBool();	
	
	m_cPrefs.m_bShowPreferences = m_pcPreferencesCheck->GetValue().AsBool();
	m_cPrefs.m_bShowTrash = m_pcTrashCheck->GetValue().AsBool();
	m_cPrefs.m_bShowRecent = m_pcRecentCheck->GetValue().AsBool();
	m_cPrefs.m_cFileBrowser = m_pcBrowserTextView->GetValue().AsString();
	m_cPrefs.m_bNotifyIfNotExist = m_pcNotifyCheck->GetValue().AsBool();
	m_cPrefs.m_nIconSize = m_pcIconSize->GetValue();
	
		
	m_cPrefs.m_cDefaultPlaces.clear();
	m_cPrefs.m_cHomePlaces.clear();
	
	m_cPrefs.m_customPlaces.clear();
	for (int i=0; i<m_pcCustomPlacesListView->GetRowCount(); i++)
	{
		os::TreeViewStringNode* node = (os::TreeViewStringNode*)m_pcCustomPlacesListView->GetRow(i);
		os::String path = node->GetString(2);
		os::String title = node->GetString(1);
		os::String icon = node->GetCookie().AsString();
		m_cPrefs.m_customPlaces[title] = std::make_pair(path,icon);		
	}
	
	m_cPrefs.m_recentFiles.clear();	
}

void PlacesPrefs::HandleMessage(os::Message* pcMessage)
{
	switch (pcMessage->GetCode())
	{
		case M_CHECKED:
		{
			void* source;		
			if (pcMessage->FindPointer("source",&source) == 0)
			{
				os::CheckBox* box = dynamic_cast<os::CheckBox*>(static_cast<os::Invoker*>(source));
				RecalculatePrefs();
			}
			break;
		}


		case M_PREFS_ADD:
		{
			m_pcCustomPlacesWin->Clear();
			m_pcCustomPlacesWin->Show();
			m_pcCustomPlacesWin->MakeFocus();
			break;
		}
		
		case M_PREFS_REMOVE:
		{
			int sel = m_pcCustomPlacesListView->GetFirstSelected();
			if (sel != -1)
			{
				m_pcCustomPlacesListView->RemoveRow(sel);
			}
			break;
		}
		
		case M_PREFS_CUSTOM_PLACE_APPLY:
		{
			m_pcCustomPlacesWin->Hide();
			os::String path,icon,title;
			
			pcMessage->FindString("path",&path);
			pcMessage->FindString("icon",&icon);
			pcMessage->FindString("title",&title);
			
			os::TreeViewStringNode* n = new os::TreeViewStringNode();
			n->AppendString("");
			n->AppendString(title);
			n->AppendString(path);
			n->SetCookie(icon);
			try
			{
				os::File cIconFile( icon );
				os::BitmapImage* pcIcon = new os::BitmapImage();
				if( pcIcon->Load( &cIconFile ) == 0 )
				{
					pcIcon->SetSize( os::Point( m_cPrefs.m_nIconSize,m_cPrefs.m_nIconSize ) );
					n->SetIcon( pcIcon );
				}
			} catch( ... )
			{
			}
			
			m_pcCustomPlacesListView->InsertNode(n);
			break;
		}
		case M_PREFS_CUSTOM_PLACE_CANCEL:
		{
			m_pcCustomPlacesWin->Hide();
			m_pcCustomPlacesWin->Clear();
			break;
		}

		case M_CUSTOM_PLACES_INVOKED:
		{
			os::TreeViewStringNode* n = static_cast<os::TreeViewStringNode*>( m_pcCustomPlacesListView->GetRow( m_pcCustomPlacesListView->GetFirstSelected()));
			m_pcCustomPlacesWin->Clear();
			
			os::String icon = n->GetCookie().AsString();
			os::String path = n->GetString(2);
			os::String title = n->GetString(1);

			
			m_pcCustomPlacesWin->SetPrefs(title,path,icon);
			m_pcCustomPlacesWin->Show();
			m_pcCustomPlacesWin->MakeFocus();
			break;
		}
		
		case M_PREFS_BROWSE:
		{
			m_pcBrowseRequester->Show();
			m_pcBrowseRequester->MakeFocus();
			break;
		}
		case M_PREFS_BROWSE_HIT:
		{
			os::String path;
			if (pcMessage->FindString("file/path",&path) == 0)
			{
				m_pcBrowserTextView->SetValue(path);
				RecalculatePrefs();
			}
			break;
		}
		case M_PREFS_BROWSER_CHANGED:
		{
			os::String path = m_pcBrowserTextView->GetValue().AsString();
			
			if ((path != m_cPrefs.m_cFileBrowser) || !CheckBrowserPath(path))
			{
				if (!CheckBrowserPath(path))
				{
					m_pcBrowserInfo->SetFgColor(os::Color32_s(180,0,0));
				}
				else
				{
					m_pcBrowserInfo->SetFgColor(os::Color32_s(0,180,0));
					RecalculatePrefs();
				}
			}
			else
			{
				m_pcBrowserInfo->SetFgColor(os::Color32_s(0,0,180));
				RecalculatePrefs();
			}
			m_pcBrowserInfo->Invalidate();
			break;
		}
		case M_PREFS_FONT_REQUESTED:
		{
			m_pcFontRequester->Show();
			break;
		}
		
		case M_FONT_REQUESTED:
		{
			os::Font* pcFont = new Font();
			pcMessage->FindObject("font",*pcFont);  //get the font from the requester and then do anything you want with it
			os::String c = os::String().Format("%s %s %0.1f",pcFont->GetFamily().c_str(),pcFont->GetStyle().c_str(),pcFont->GetSize());
			m_pcFontInfo->SetString(c);
			m_pcParent->SetFont(pcFont);
        	pcFont->Release();		
		}
		
		case M_PREFS_APPLY:
		{
			if (CheckBrowserPath(m_pcBrowserTextView->GetValue().AsString()))
			{
				RecalculatePrefs();
				os::Message* c = new os::Message(pcMessage->GetCode());
				m_pcParent->GetLooper()->PostMessage(c,m_pcParent);
				delete c;
			}
			break;
		}
		
		default:
		{
			break;
		}
	}
}

bool PlacesPrefs::CheckBrowserPath(const os::String& path)
{
	try
	{
		File f (path);
		return true;
	}
	catch(...)
	{
		return false;
	}
	return false;
}

void PlacesPrefs::MakeFocus(bool focus)
{
	Window::MakeFocus(focus);
	if (focus)
	{
		SetFocusChild(m_pcApplicationsCheck);
		SetDefaultButton(m_pcApplyButton);
	}
}

class DockPlaces : public View
{
	public:
		DockPlaces(os::Path,os::Looper*);
		~DockPlaces();
		
		void LoadSettings();
		void SaveSettings();
		
		Point			GetPreferredSize( bool bLargest ) const;	
        
		virtual void	AttachedToWindow();
		virtual void	DetachedFromWindow();
		virtual void	MouseMove( const os::Point& cNewPos, int nCode, uint32 nButtons, os::Message* pcData );
		//virtual void	MouseUp( const os::Point & cPosition, uint32 nButton, os::Message * pcData );
		virtual void	MouseDown( const os::Point& cPosition, uint32 nButtons );
		virtual void	HandleMessage(Message* pcMessage);    
		virtual void 	TimerTick(int nID);
		virtual void	FontChanged(os::Font*);
		
		void AddPlacesMenu();
		void AddHomeItemsMenu();
		void AddDriveItemsMenu();
		void AddRecentMenu();
		void AddCustomItems();
		os::Image* GetImage(const os::String& path);
		bool CheckPath(const os::String& path);
		void CheckDrives();
		void CreateMenu();
	private:
		PopupMenu* m_pcPopup;
		os::Looper* m_pcParent;
		
		os::BitmapImage* m_pcIcon;
		Menu*      pcContextMenu;
		os::Path m_cPath;
		os::Message m_cMsg;
		os::String m_cDroppedPath;
		
		Prefs m_cPrefs;
		os::Volumes* m_pcVolumes;


		PlacesPrefs* m_pcPrefsWin;
};

//*************************************************************************************
DockPlaces::DockPlaces(os::Path cPath,os::Looper* parent) : View(Rect(0,0,0,0),"DockPlacesView")
{
	m_pcPrefsWin = NULL;
	m_pcParent = parent;
	m_cMsg = os::Message(M_MENU_HIT);
	

	/* Load default icons */
	os::File* pcFile = new os::File( cPath );
	os::Resources cCol( pcFile );
	os::ResStream* pcStream = cCol.GetResourceStream( "icon24x24.png" );
	m_pcIcon = new os::BitmapImage( pcStream );
	m_pcIcon->SetSize(os::Point(24,24));
	delete pcFile;
	
	
	m_pcPopup = new PopupMenu(Rect(0,0,1,1),"places_popup","Places",NULL,m_pcIcon);
	m_pcVolumes = new os::Volumes();
	m_cDroppedPath = "";
	pcContextMenu = NULL;
}



DockPlaces::~DockPlaces( )
{

}

void DockPlaces::AttachedToWindow()
{
	LoadSettings();
	CreateMenu();
	
	m_pcPopup->SetFrame(Rect(0,0,m_pcPopup->GetPreferredSize(false).x, m_pcPopup->GetPreferredSize(false).y));
	m_pcPopup->SetDrawArrow(false);
	AddChild(m_pcPopup);
}

void DockPlaces::CreateMenu()
{
	if (pcContextMenu != NULL)
	{
		delete pcContextMenu;
		pcContextMenu = NULL;
		m_pcPopup->SetMenu(NULL);
	}
	pcContextMenu = new Menu(Rect(0,0,10,10),"",ITEMS_IN_COLUMN);

	os::String h = getenv("HOME");	
	if (m_cPrefs.m_bShowApplications)
	{
		m_cPrefs.m_cDefaultPlaces.push_back(std::pair<os::String,os::String>("Applications","/boot/Applications"));
	}

	if (m_cPrefs.m_bShowPreferences)
	{
		m_cPrefs.m_cDefaultPlaces.push_back(std::pair<os::String,os::String>("Preferences","/boot/Applications/Preferences"));
	}
	if (m_cPrefs.m_bShowTrash)
	{
		os::String t = os::String().Format("%s/Trash",h.c_str());
		m_cPrefs.m_cDefaultPlaces.push_back(std::pair<os::String,os::String>("Trash",t));
	}
	
	if (m_cPrefs.m_bShowHome)
	{
		m_cPrefs.m_cHomePlaces.push_back(std::pair<os::String,os::String>("Home",h));
	}
	if (m_cPrefs.m_bShowDesktop)
	{
		os::String t = os::String().Format("%s/Desktop",h.c_str());
		m_cPrefs.m_cHomePlaces.push_back(std::pair<os::String,os::String>("Desktop",t));
	}
	if (m_cPrefs.m_bShowDocuments)
	{
		os::String t = os::String().Format("%s/Documents",h.c_str());
		m_cPrefs.m_cHomePlaces.push_back(std::pair<os::String,os::String>("Documents",t));
	}
	
	if (m_cPrefs.m_bShowDownloads)
	{
		os::String t = os::String().Format("%s/Downloads",h.c_str());
		m_cPrefs.m_cHomePlaces.push_back(std::pair<os::String,os::String>("Downloads",t));
	}
	
	if (m_cPrefs.m_bShowPictures)
	{
		os::String t = os::String().Format("%s/Pictures",h.c_str());
		m_cPrefs.m_cHomePlaces.push_back(std::pair<os::String,os::String>("Pictures",t));
	}

	AddPlacesMenu();
	AddHomeItemsMenu();
	AddDriveItemsMenu();
	AddCustomItems();
	AddRecentMenu();
	if (pcContextMenu->GetItemCount() > 0)
	{
		pcContextMenu->AddItem(new MenuSeparator());
	}
	
	os::BitmapImage* image = (os::BitmapImage*)GetImage("/system/icons/settings.png");
	image->SetSize(os::Point(m_cPrefs.m_nIconSize,m_cPrefs.m_nIconSize));
	
	pcContextMenu->AddItem("Preferences...",new Message(M_PREFS),"",image);
	pcContextMenu->SetTargetForItems(this);
	
	pcContextMenu->InvalidateLayout();
	m_pcPopup->SetMenu(pcContextMenu);
}

void DockPlaces::DetachedFromWindow()
{
	//SaveSettings();
}

void DockPlaces::LoadSettings()
{
	/* Load settings */
	os::String zPath = getenv( "HOME" );
	zPath += "/Settings/Places/Settings";
	int custom = 0;
	
	try
	{
		os::Settings* pcSettings = new os::Settings(new File(zPath));
		pcSettings->Load();

		m_cPrefs.m_bShowDrives = pcSettings->GetBool("drives",true);
		m_cPrefs.m_bShowHome = pcSettings->GetBool("home",true);
		m_cPrefs.m_bShowDesktop = pcSettings->GetBool("desktop",true);
		m_cPrefs.m_bShowDocuments = pcSettings->GetBool("documents",true);
		m_cPrefs.m_bShowDownloads = pcSettings->GetBool("downloads",true);
				
		m_cPrefs.m_bShowPictures = pcSettings->GetBool("pictures",true);
		
		m_cPrefs.m_bShowApplications = pcSettings->GetBool("apps",true);
		m_cPrefs.m_bShowPreferences = pcSettings->GetBool("prefs",true);
		m_cPrefs.m_bShowTrash = pcSettings->GetBool("trash",true);
		m_cPrefs.m_bShowRecent = pcSettings->GetBool("recent",true);
		m_cPrefs.m_cFileBrowser = pcSettings->GetString("filebrowser","/system/bin/FileBrowser");
		m_cPrefs.m_bNotifyIfNotExist = pcSettings->GetBool("notify",true);
		
		m_cPrefs.m_nIconSize = pcSettings->GetInt32("icon_size",16);
		
//		FontHandle* pcFontHandle;
		/*size_t nFontHandleSize;
		if (pcSettings->FindData("fonthandle",T_RAW,(const void**)&m_cPrefs.m_pcFontHandle, &nFontHandleSize) != 0)
		{
			m_cPrefs.m_pcFontHandle = GetDefaultFontHandle();
		}
		else
		{
			SetFont(new os::Font(GetFontProperties(m_cPrefs.m_pcFontHandle)));
		}*/	
		if (pcSettings->FindInt32("custom",&custom) == 0)
		{
			os::String path;
			os::String icon;
			os::String title;
			
			for (int i=0; i<custom; i++)
			{
				os::String c = os::String().Format("custom_%d",i);
				os::String pathString = os::String().Format("%s_path",c.c_str());
				os::String iconString = os::String().Format("%s_icon",c.c_str());
				os::String titleString = os::String().Format("%s_title",c.c_str());
				
				path = pcSettings->GetString(pathString.c_str(),"");
				title = pcSettings->GetString(titleString.c_str(),os::String().Format("Custom #%d",i+1).c_str());
				icon = pcSettings->GetString(iconString.c_str(),"");
				
				if (path != "")
				{
					m_cPrefs.m_customPlaces[title] = std::make_pair(path,icon);
				}
			}
		}
		delete( pcSettings );
	}
	catch(...)
	{
	}
}

void DockPlaces::SaveSettings()
{
	/* Save settings */
	os::String zPath = getenv( "HOME" );
	zPath += "/Settings/Places";

	try
	{
		mkdir( zPath.c_str(), S_IRWXU | S_IRWXG | S_IRWXO );
		zPath += "/Settings";
		os::File cFile( zPath, O_RDWR | O_CREAT );	
		os::Settings* pcSettings = new os::Settings(new File(zPath));

		pcSettings->SetBool("drives",m_cPrefs.m_bShowDrives);

		pcSettings->SetBool("apps",m_cPrefs.m_bShowApplications);
		pcSettings->SetBool("prefs",m_cPrefs.m_bShowPreferences);
		pcSettings->SetBool("trash",m_cPrefs.m_bShowTrash);
		pcSettings->SetBool("recent",m_cPrefs.m_bShowRecent);
		pcSettings->SetString("filebrowser",m_cPrefs.m_cFileBrowser);
		pcSettings->SetInt32("custom",m_cPrefs.m_customPlaces.size());
		
		pcSettings->SetBool("home",m_cPrefs.m_bShowHome);
		pcSettings->SetBool("desktop",m_cPrefs.m_bShowDesktop);
		pcSettings->SetBool("documents",m_cPrefs.m_bShowDocuments);
		pcSettings->SetBool("downloads",m_cPrefs.m_bShowDownloads);
		pcSettings->SetBool("pictures",m_cPrefs.m_bShowPictures);
		pcSettings->SetBool("notify",m_cPrefs.m_bNotifyIfNotExist);
		pcSettings->SetInt32("icon_size",m_cPrefs.m_nIconSize);
		
		//pcSettings->SetData("fonthandle",T_RAW, (void*)m_cPrefs.m_pcFontHandle,sizeof(FontHandle));
		int custom = 0;
		for (customPlacesIterator i = m_cPrefs.m_customPlaces.begin(); i!=m_cPrefs.m_customPlaces.end(); i++)
		{
			os::String title = i->first;
			os::String path = i->second.first;
			os::String icon = i->second.second;
			
			os::String c = os::String().Format("custom_%d",custom);
			os::String pathString = os::String().Format("%s_path",c.c_str());
			os::String iconString = os::String().Format("%s_icon",c.c_str());
			os::String titleString = os::String().Format("%s_title",c.c_str());

			pcSettings->SetString(titleString.c_str(),title);
			pcSettings->SetString(pathString.c_str(),path);
			pcSettings->SetString(iconString.c_str(),icon);
			custom++;
		}
			
		pcSettings->Save();
		delete( pcSettings );
	}
	catch(...)
	{
	}
}

void DockPlaces::AddPlacesMenu()
{
	for (defaultPlacesIterator i=m_cPrefs.m_cDefaultPlaces.begin(); i!=m_cPrefs.m_cDefaultPlaces.end(); i++)
	{
		os::Message* pcMsg = new os::Message(M_MENU_HIT);
		pcMsg->AddString("_path",i->second);
		
		os::Image* pcImage = GetImage(i->second);
		pcImage->SetSize(os::Point(m_cPrefs.m_nIconSize,m_cPrefs.m_nIconSize));
		os::MenuItem* pcItem = new os::MenuItem(i->first,pcMsg,"",pcImage);
		pcContextMenu->AddItem(pcItem);
	}
}

void DockPlaces::AddHomeItemsMenu()
{
	if (m_cPrefs.m_cHomePlaces.size() > 0)
	{
		pcContextMenu->AddItem(new MenuSeparator());
	}
	
	for (defaultPlacesIterator i=m_cPrefs.m_cHomePlaces.begin(); i!=m_cPrefs.m_cHomePlaces.end(); i++)
	{
		os::Message* pcMsg = new os::Message(M_MENU_HIT);
		pcMsg->AddString("_path",i->second);
		
		os::Image* pcImage = GetImage(i->second);
		pcImage->SetSize(os::Point(m_cPrefs.m_nIconSize,m_cPrefs.m_nIconSize));
		os::MenuItem* pcItem = new os::MenuItem(i->first,pcMsg,"",pcImage);
		pcContextMenu->AddItem(pcItem);
	}	
}

void DockPlaces::AddDriveItemsMenu()
{
	if (m_cPrefs.m_bShowDrives)
	{
		m_pcParent->AddTimer( this,M_CHECK_DRIVES_TIMER, 6000000,false);
		pcContextMenu->AddItem(new MenuSeparator());
		os::BitmapImage* pcImage = (os::BitmapImage*)GetImage("disk.png");
		
		os::String cMountPoint;
		for( uint i = 0; i < m_pcVolumes->GetMountPointCount(); i++ )
		{
			if( m_pcVolumes->GetMountPoint( i, &cMountPoint ) == 0 ) 
			{
				fs_info sInfo;
				if( m_pcVolumes->GetFSInfo( i, &sInfo ) == 0 && ( sInfo.fi_flags & FS_IS_BLOCKBASED ) )
				{					
					os::String zVolumeName = sInfo.fi_volume_name;
					if( ( zVolumeName == "" ) || ( zVolumeName == "/" ) )
					{
						zVolumeName = "Unknown";
					}
					os::Message* pcMsg = new os::Message(M_MENU_HIT);
					pcMsg->AddString("_path",cMountPoint);
	
					double free = ( double )sInfo.fi_free_blocks / ( ( double )sInfo.fi_total_blocks );

					pcImage->SetSize(os::Point(m_cPrefs.m_nIconSize,m_cPrefs.m_nIconSize));

					DriveMenuItem* pcItem = new DriveMenuItem(sInfo.fi_volume_name,pcMsg,m_cPrefs.m_nIconSize,free);
					pcContextMenu->AddItem(pcItem);
				}
			}
		}
	}
}
					
void DockPlaces::AddRecentMenu()
{
	if (m_cPrefs.m_bShowRecent && m_cPrefs.m_recentFiles.size() >0)
	{
		pcContextMenu->AddItem(new MenuSeparator());
		
		os::Menu* recent = new Menu(Rect(0,0,10,10),"Recent Files",ITEMS_IN_COLUMN);
		recent->SetEnable(false);
		pcContextMenu->AddItem(recent);
	}
}

void DockPlaces::AddCustomItems()
{
	if (m_cPrefs.m_customPlaces.size() > 0)
	{
		pcContextMenu->AddItem(new MenuSeparator());
	}

	for (customPlacesIterator i = m_cPrefs.m_customPlaces.begin(); i!=m_cPrefs.m_customPlaces.end(); i++)
	{
		os::String title = i->first;
		os::String path = i->second.first;
		os::String icon = i->second.second;

		os::Message* pcMsg = new os::Message(M_MENU_HIT);
		pcMsg->AddString("_path",path);
		os::Image* pcImage = GetImage(icon);
		pcImage->SetSize(os::Point(m_cPrefs.m_nIconSize,m_cPrefs.m_nIconSize));
		os::MenuItem* pcItem = new os::MenuItem(title,pcMsg,"",pcImage);
		pcContextMenu->AddItem(pcItem);
	}
}

os::Image* DockPlaces::GetImage(const os::String& path)
{
	printf("path is: %s \n", path.c_str());
	os::BitmapImage* c = NULL;
	os::String h = getenv("HOME");
	
	if (path == "/boot/Applications")
	{
		File cFile( "/boot/system/icons/applications.png" );
		c = new BitmapImage( &cFile );
	}
	else if (path == h)
	{
		File cFile( "/boot/system/icons/home.png" );
		c = new BitmapImage( &cFile );
	}
	else if (path == "/boot/Applications/Preferences")
	{
		File cFile( "/boot/system/icons/settings.png" );
		c = new BitmapImage( &cFile );	
	}
	else if (path == os::String().Format("%s/Trash",h.c_str()))
	{
		File cFile( "/boot/system/icons/trash.png" );
		c = new BitmapImage( &cFile );			
	}
	else if (path == "disk.png")
	{
		File cFile("/system/icons/disk.png");
		c = new BitmapImage(&cFile);
	}
	else
	{
		try 
		{
			File cFile (path);
			c = new BitmapImage(&cFile);
		}
		catch(...)
		{
			os::Bitmap* b = m_pcIcon->LockBitmap();
			c = new os::BitmapImage(b->LockRaster(),os::IPoint(24,24),CS_RGBA32);
			b->UnlockRaster();
			m_pcIcon->UnlockBitmap();
		}
	}
	return c;
}


bool DockPlaces::CheckPath(const os::String& cPath)
{
	try
	{
		Directory d(cPath);
		return true;
	}
	catch(...)
	{
		return false;
	}
}

void DockPlaces::TimerTick(int nID)
{
	//CheckDrives();
}

void DockPlaces::CheckDrives()
{
	os::String cMountPoint;
	for( uint i = 0; i < m_pcVolumes->GetMountPointCount(); i++ )
	{
		if( m_pcVolumes->GetMountPoint( i, &cMountPoint ) == 0 ) 
		{
			fs_info sInfo;
			
			if( m_pcVolumes->GetFSInfo( i, &sInfo ) == 0 && ( sInfo.fi_flags & FS_IS_BLOCKBASED ) )
			{
				DriveMenuItem* item = dynamic_cast<DriveMenuItem*>(pcContextMenu->FindItem(sInfo.fi_volume_name));
				
				if (item)
				{
					double free = ( double )sInfo.fi_free_blocks / ( ( double )sInfo.fi_total_blocks );
					
	
					free = static_cast<float>(rand())/static_cast<float>(RAND_MAX);
					item->m_pcDrive->SetProgress(free);
					//
					//
					//item->SetImage(item->m_pcDrive->m_pcImage,true);
					//item->Draw();
					//pcContextMenu->InvalidateLayout();
				}
			}
		}
	}
}

Point DockPlaces::GetPreferredSize( bool bLargest ) const
{
	return os::Point(m_pcPopup->GetBounds().Width()+5, m_pcPopup->GetBounds().Height());
}

void DockPlaces::HandleMessage(Message* pcMessage)
{
	switch (pcMessage->GetCode())
	{
		case M_PREFS_APPLY:
		{
			m_cPrefs = m_pcPrefsWin->GetPrefs();
			CreateMenu();
			m_pcPrefsWin->Hide();
			//SaveSettings();
			break;
		}		
		case M_PREFS:
		{
			if (m_pcPrefsWin == NULL){
				m_pcPrefsWin = new PlacesPrefs(this,m_pcIcon->LockBitmap());
				m_pcIcon->UnlockBitmap();
				m_pcPrefsWin->CenterInScreen();
				m_pcPrefsWin->Show();
				m_pcPrefsWin->MakeFocus();
				m_pcPrefsWin->SetPrefs(m_cPrefs);
			}
			else
			{
				if (!m_pcPrefsWin->IsVisible())
				{
					m_pcPrefsWin->Show();
				}
				m_pcPrefsWin->MakeFocus();
			}
			break;
		}
		case M_MENU_HIT:
		{
			os::String path;
			os::String recent;
			if (pcMessage->FindString("_path",&path) == 0)
			{
				if (m_cPrefs.m_bNotifyIfNotExist && !CheckPath(path))
				{
					
					os::String c = os::String().Format("%s: does not exist",path.c_str());

					//TODO: RGC - NOTIFICATION SYSTEM
					//We need to fix the alert, however we need a notification system: new Alert("Title","Text", Alert::ALERT_WARNING,0,"OK",NULL					
					os::Alert* alert = new os::Alert("Path does not exist",c,os::Alert::ALERT_WARNING,0,"OK",NULL);
					alert->Go();
				}
				else if (fork() == 0)
				{
					execlp(m_cPrefs.m_cFileBrowser.c_str(),m_cPrefs.m_cFileBrowser.c_str(),path.c_str(),NULL);
				}
			}
			else if (pcMessage->FindString("_recent",&recent) == 0)
			{
			}
			break;
		}
		default:
		{
			View::HandleMessage(pcMessage);
			break;
		}
	}
}

void DockPlaces::MouseMove( const os::Point& cNewPos, int nCode, uint32 nButtons, os::Message* pcData )
{
	os::String path;
	
	if (pcData != NULL && pcData->FindString("file/path",&path) == 0)
	{
		m_cDroppedPath = path;
		return;
	}
	View::MouseMove(cNewPos,nCode,nButtons,pcData);
}

/*void DockPlaces::MouseUp( const os::Point & cPosition, uint32 nButton, os::Message * pcData )
{
	os::String path;
	
/*	if (pcData == NULL)
	{
	}
	
	if (pcData->FindString("file/path",&path) == 0)
	{
		printf("%s\n",path.c_str());
	}
	
	View::MouseUp( cPosition, nButton, pcData );

}*/

void DockPlaces::MouseDown( const os::Point& cPosition, uint32 nButtons )
{
	m_pcPopup->MouseDown( cPosition, 1 );
}

void DockPlaces::FontChanged(os::Font* font)
{
	m_pcPopup->SetFont(font);
}

//*************************************************************************************
class PlacesPlugin : public DockPlugin
{
public:
	PlacesPlugin()
	{
		m_pcView = NULL;
	}
	~PlacesPlugin()
	{
	}
	status_t Initialize()
	{
		m_cPath = GetPath();
		m_pcView = new DockPlaces(GetPath(),GetApp());
		AddView( m_pcView );
		return( 0 );
	}
	void Delete()
	{
		RemoveView( m_pcView );
	}
	os::String GetIdentifier()
	{
		return( "Places");
	}
private:
	DockPlaces* m_pcView;
};

 
extern "C"
{
DockPlugin* init_dock_plugin()
{
	return(new PlacesPlugin());
}
}




