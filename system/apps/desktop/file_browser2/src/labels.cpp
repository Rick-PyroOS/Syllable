#include "labels.h"
#include <util/typecodes.h>

using namespace os;

Labels* Labels::m_theInstance = NULL;

Labels* Labels :: GetInstance()
{
	if( m_theInstance == NULL )
	{
		m_theInstance = new Labels();
	}

	return m_theInstance;
}


LabelMenuItem::LabelMenuItem( const String& cLabel, Message* pcMsg, os::Color32_s sColor ) : os::MenuItem(cLabel,pcMsg)
{
	m_sColor = sColor;
	m_Highlighted = false;
	m_Enabled = true;
	m_bIsSelectable = true;	
}

LabelMenuItem::~LabelMenuItem()
{
}

os::Color32_s LabelMenuItem::GetColor()
{
	return m_sColor;
}

void LabelMenuItem::Draw()
{
	Menu *pcMenu = GetSuperMenu();

	if( pcMenu == NULL )
	{
		return;
	}

	const String& cLabel = GetLabel();
	Rect cFrame = GetFrame();

	font_height sHeight;
	pcMenu->GetFontHeight( &sHeight );

	if( m_Highlighted )
	{
		pcMenu->SetFgColor( get_default_color( COL_SEL_MENU_BACKGROUND ) );
	}
	else
	{
		pcMenu->SetFgColor( get_default_color( COL_MENU_BACKGROUND ) );
	}

	pcMenu->FillRect( GetFrame() );

	if( m_Highlighted )
	{
		pcMenu->SetFgColor( get_default_color( COL_SEL_MENU_TEXT ) );
		pcMenu->SetBgColor( get_default_color( COL_SEL_MENU_BACKGROUND ) );
	}
	else
	{
		pcMenu->SetFgColor( get_default_color( COL_MENU_TEXT ) );
		pcMenu->SetBgColor( get_default_color( COL_MENU_BACKGROUND ) );
	}

	float vCharHeight = sHeight.ascender + sHeight.descender + sHeight.line_gap;
	float y = cFrame.top + ( cFrame.Height() + 1.0f ) / 2 - vCharHeight / 2 + sHeight.ascender + sHeight.line_gap * 0.5f;

	pcMenu->DrawString( Point( cFrame.left + 18, y ), cLabel );

	//os::Rect cFrame = GetFrame();
	pcMenu->FillRect(os::Rect(cFrame.left+2,cFrame.top+2,cFrame.left+16,cFrame.bottom-2),m_sColor);
}

//----------------------------------------------------------------------------
// NAME:
// DESC:
// NOTE:
// SEE ALSO:
//----------------------------------------------------------------------------

void LabelMenuItem::SetHighlighted( bool bHighlighted )
{
	m_Highlighted = bHighlighted;
	MenuItem::SetHighlighted( bHighlighted );
}

//----------------------------------------------------------------------------
// NAME:
// DESC:
// NOTE:
// SEE ALSO:
//----------------------------------------------------------------------------

Point LabelMenuItem::GetContentSize() const
{
	Menu *pcMenu = GetSuperMenu();

	if( pcMenu != NULL )
	{
		font_height sHeight;

		pcMenu->GetFontHeight( &sHeight );

		return ( Point( pcMenu->GetStringWidth( GetLabel() ) + 16, sHeight.ascender + sHeight.descender + sHeight.line_gap ) );
	}

	return ( Point( 0.0f, 0.0f ) );
}

//----------------------------------------------------------------------------
// NAME:
// DESC:
// NOTE:
// SEE ALSO:
//----------------------------------------------------------------------------

bool LabelMenuItem::Invoked( Message * pcMessage )
{
	pcMessage->AddBool( "color", m_sColor );
	return true;
}


bool LabelMenuItem::IsSelectable() const
{
	return m_bIsSelectable;
}




class Labels::Private
{
public:
	Private()
	{
	}
	LabelType m_sLabels;
};


Labels::Labels()
{
	m = new Private();
	LoadSettings();
}

void Labels::LoadSettings()
{
	os::Settings* settings = new os::Settings();
	settings->SetFile("labels");
	
	if (settings->Load() == -1)
	{
		m->m_sLabels.insert(std::map<os::String,os::Color32_s>::value_type("Red",os::Color32_s(255,0,0,128)));
		m->m_sLabels.insert(std::map<os::String,os::Color32_s>::value_type("Green",os::Color32_s(0,255,0,128)));
		m->m_sLabels.insert(std::map<os::String,os::Color32_s>::value_type("Blue",os::Color32_s(0,0,255,128)));
		m->m_sLabels.insert(std::map<os::String,os::Color32_s>::value_type("Yellow",os::Color32_s(255,255,0,128)));
		m->m_sLabels.insert(std::map<os::String,os::Color32_s>::value_type("Orange",os::Color32_s(255,192,0,128)));
	}
	else
	{	
		int count;
		settings->FindInt32("count",&count);

		for (int i=0; i<count; i++)
		{
			Label* data;
			size_t size = sizeof(data);
			if (settings->FindData("label",T_RAW,(const void**)&data,&size,i) != -1)
			{
				m->m_sLabels.insert(std::map<os::String,os::Color32_s>::value_type(data->name,data->color));
			}
		}
	}
	
}

void Labels::SaveSettings()
{
	os::Settings* settings = new os::Settings();
	settings->SetFile("labels");

	LabelType::iterator p;
	int i=0;
	for (p = m->m_sLabels.begin(); p != m->m_sLabels.end();++p)
	{
		Label s;
		memcpy(s.name,p->first.c_str(),1024);
		s.color = p->second;
		settings->SetData("label",T_RAW,(void*)&s,sizeof(s),++i);
	}
	settings->SetInt32("count",i);
	settings->Save();

}

LabelType Labels::GetLabels()
{
	return m->m_sLabels;
}

os::Menu* Labels::GetMenu(int message)
{
	os::Menu* menu = new os::Menu(os::Rect(),"Labels",os::ITEMS_IN_COLUMN);
	
	LabelType::iterator p;
	for (p = m->m_sLabels.begin(); p != m->m_sLabels.end();++p)
	{
		menu->AddItem(new LabelMenuItem(p->first,new os::Message(message),p->second));
	}
	return menu;
}





