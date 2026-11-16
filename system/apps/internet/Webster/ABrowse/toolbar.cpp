/*  libsyllable.so - the highlevel API library for Syllable
 *  Copyright (C) 2002 - 2003 Rick Caudill
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
#include "toolbar.h"
#include <gui/imagebutton.h>

using namespace os;

/** Initialize the ToolBar.
 * \par Description:
 *	The toolbar constructor.  There is one problem with the toolbar class.  It will not tell
    you if the item that is added is to the ToolBar is or is not inside the ToolBar.  
 * \param cRect - The size and position of the ToolBar.
 * \param zName - The name of the ToolBar.
 * \param bDrawLines - Determines whether or not the ToolBar will draw the lines on the outside of the ToolBar.
 * \param nResizeMask - Determines what way the ToolBar will follow the rest of the window.  Default is CF_FOLLOW_LEFT|CF_FOLLOW_TOP.
 * \param nFlags - Deteremines what flags will be sent to the ToolBar.
 * \author	Rick Caudill (cau0730@cup.edu)
 *****************************************************************************/
ToolBar::ToolBar (const Rect& cRect, const char* zName, bool bShowText, bool bDrawLines,uint32 nPos, uint32 nResizeMask, uint32 /*nFlags*/ ) : LayoutView(cRect, zName,NULL, nResizeMask)
{
    nPosition = nPos;
    bDraw = bDrawLines;
    m_bShowText = bShowText;
    m_pcNode = new os::HLayoutNode( "h_toolbar" );
    m_pcNode->SetBorders( os::Rect( 0, 4, 5, 4 ) );
    
} /*end ToolBar Constructor*/

ToolBar::~ToolBar()
{
    /*destructor*/
}/*end destructor*/

/*Paint*/
void ToolBar::Paint(const Rect& cRect)
{
    SetEraseColor(get_default_color(COL_NORMAL));
    EraseRect(cRect);
    Draw();
} //end Paint()


static void SetButtonImageFromResource( os::ImageButton* pcButton, os::String zResource )
{
	os::File cSelf( open_image_file( get_image_id() ) );
	os::Resources cCol( &cSelf );		
	os::ResStream *pcStream = cCol.GetResourceStream( zResource );
	pcButton->SetImage( pcStream );
	delete( pcStream );
}

os::ImageButton* ToolBar::AddButton( os::String cResource, os::String zText, os::Message* pcMsg )
{
	os::ImageButton* pcButton = new os::ImageButton(os::Rect(), zText, zText,pcMsg,NULL, os::ImageButton::IB_TEXT_BOTTOM,cResource == "breaker.png" ? false : true,m_bShowText, true);
    SetButtonImageFromResource( pcButton, cResource );
    m_pcNode->AddChild( pcButton, 0.0f );
    m_pcNode->AddChild( new os::HLayoutSpacer( "", 3.0f, 3.0f ) );
    return( pcButton );
}

void ToolBar::Finish()
{
	SetRoot( m_pcNode );
}

void ToolBar::EnableButton( int nCode, bool bEnable )
{
	os::View* pcView;
	int nIndex = 0;
	while( ( pcView = GetChildAt( nIndex ) ) != NULL )
	{
		os::ImageButton* pcButton = dynamic_cast<os::ImageButton*>(pcView);
		if( pcButton && pcButton->GetMessage() && pcButton->GetMessage()->GetCode() == nCode )
		{
			pcButton->SetEnable( bEnable );
		}	
		nIndex++;
	}
}

//Draw
void ToolBar::Draw()
{
    if (bDraw == true)
    {
        if(nPosition == Horizontal)
        {
            SetFgColor(10,10,10);
            DrawLine(Point(0.0f,0.0f), Point(this->GetBounds().Width(), 0)  );
            SetFgColor(get_default_color(COL_SHINE));
            DrawLine(Point(0.0f,1.0f), Point(this->GetBounds().Width(), 1.0f)  );
            DrawLine(Point(0.0f, this->GetBounds().Height()+0.1f), Point(this->GetBounds().Width(),this->GetBounds().Height() +0.1f));
            SetFgColor(10,10,10);
            DrawLine(Point(0.0f, this->GetBounds().Height()+1.1f), Point(this->GetBounds().Width(),this->GetBounds().Height() +1.1f));
        }

        if (nPosition == Vertical)
        {
            SetFgColor(10,10,10);
            DrawLine(Point(0.0f,0.0f), Point(0,this->GetBounds().Height())  );
            SetFgColor(get_default_color(COL_SHINE));
            DrawLine(Point(1.0f,0.0f), Point(1.0f,this->GetBounds().Height())  );
            DrawLine(Point(this->GetBounds().Width(),0.0f), Point(this->GetBounds().Width(),this->GetBounds().Height()));
            SetFgColor(0,0,0);
            DrawLine(Point(this->GetBounds().Width()-0.1f,0.0f), Point(this->GetBounds().Width()-0.1f,this->GetBounds().Height() +1.1f));
        }
    }
} //end Draw()

os::HLayoutNode* ToolBar::GetNode()
{
	return( m_pcNode );
}

/*reserved*/
void ToolBar::_reserved1()
{}
void ToolBar::_reserved2()
{}
void ToolBar::_reserved3()
{}
void ToolBar::_reserved4()
{}
void ToolBar::_reserved5()
{}
void ToolBar::_reserved6()
{}
void ToolBar::_reserved7()
{}
void ToolBar::_reserved8()
{}
void ToolBar::_reserved9()
{}
void ToolBar::_reserved10()
{}



















