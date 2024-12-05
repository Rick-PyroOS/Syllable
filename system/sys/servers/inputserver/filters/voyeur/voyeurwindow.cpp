


#include <os/types.h>
#include <os/kernel.h>

#include <gui/window.h>
#include <gui/textview.h>
#include <gui/rect.h>

#include "voyeurwindow.h"


using namespace os;

//----------------------------------------------------------------------------
// NAME:
// DESC:
// NOTE:
// SEE ALSO:
//----------------------------------------------------------------------------

VoyeurWindow::VoyeurWindow( const Rect& cFrame )
	: Window(cFrame, "InputServer Voyeur", "InputServer Voyeur", 0, ALL_DESKTOPS)
{
	CALLED();
	
	m_pcView = new TextView(GetBounds(), "Voyeur View", "", CF_FOLLOW_ALL, WID_FULL_UPDATE_ON_RESIZE | WID_WILL_DRAW);
	m_pcView->SetMultiLine(true);
	m_pcView->SetReadOnly(true);
	AddChild(m_pcView);
}

//----------------------------------------------------------------------------
// NAME:
// DESC:
// NOTE:
// SEE ALSO:
//----------------------------------------------------------------------------

VoyeurWindow::~VoyeurWindow()
{
	CALLED();
	
	if(m_pcView)
		delete m_pcView;
}

//----------------------------------------------------------------------------
// NAME:
// DESC:
// NOTE:
// SEE ALSO:
//----------------------------------------------------------------------------

void VoyeurWindow::Insert( const char* zBuffer )
{
	CALLED();
	
	m_pcView->Insert(zBuffer);
}

