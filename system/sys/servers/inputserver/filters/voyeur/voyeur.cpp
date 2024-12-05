


#include <os/types.h>
#include <os/kernel.h>
#include <os/string.h>

#include <posix/errno.h>

#include <servers/inputserver/protocol.h>

#include "voyeur.h"
#include "voyeurwindow.h"


//----------------------------------------------------------------------------
// NAME:
// DESC:
// NOTE:
// SEE ALSO:
//----------------------------------------------------------------------------

Voyeur::Voyeur( void )
{
	CALLED();
	
	m_pcWindow = new VoyeurWindow(Rect(100, 125, 700, 500));
	m_pcWindow->Show();
}

//----------------------------------------------------------------------------
// NAME:
// DESC:
// NOTE:
// SEE ALSO:
//----------------------------------------------------------------------------

Voyeur::~Voyeur()
{
	CALLED();
	
	if(m_pcWindow)
		delete m_pcWindow;
}

//----------------------------------------------------------------------------
// NAME:
// DESC:
// NOTE:
// SEE ALSO:
//----------------------------------------------------------------------------

MessageFilterResult Voyeur::Filter( Message* pcMessage, MessageList tOutList )
{
	CALLED();
	
	switch(pcMessage->GetCode()) {
		case M_MOUSE_DOWN: {
			int32 nButton;
			char zBuffer[256];
						
			pcMessage->FindInt32("_button", &nButton);
			
			m_pcWindow->Insert("****** Voyeur ****** : M_MOUSE_DOWN\n");
			snprintf(zBuffer, sizeof(zBuffer), "****** Voyeur ****** : Buttons pressed:  %d\n", nButton);
			m_pcWindow->Insert(zBuffer);
			m_pcWindow->Insert("\n");
		} break;
		case M_MOUSE_UP: {
			int32 nButton;
			char zBuffer[256];
						
			pcMessage->FindInt32("_button", &nButton);
			
			m_pcWindow->Insert("****** Voyeur ****** : M_MOUSE_UP\n");
			snprintf(zBuffer, sizeof(zBuffer), "****** Voyeur ****** : Buttons pressed:  %d\n", nButton);
			m_pcWindow->Insert(zBuffer);
			m_pcWindow->Insert("\n");
		} break;
		case M_MOUSE_MOVED: {
			Point cDeltaMove;
			char zBuffer[256];
						
			pcMessage->FindPoint("delta_move", &cDeltaMove);
			
			m_pcWindow->Insert("****** Voyeur ****** : M_MOUSE_MOVED\n");
			snprintf(zBuffer, sizeof(zBuffer), "****** Voyeur ****** : X:  %f, Y:  %f\n", cDeltaMove.x, cDeltaMove.y);
			m_pcWindow->Insert(zBuffer);
			m_pcWindow->Insert("\n");
		} break;
		case M_WHEEL_MOVED: {
			Point cDeltaMove;
			char zBuffer[256];
						
			pcMessage->FindPoint("delta_move", &cDeltaMove);
			
			m_pcWindow->Insert("****** Voyeur ****** : M_WHEEL_MOVED\n");
			snprintf(zBuffer, sizeof(zBuffer), "****** Voyeur ****** : X:  %f, Y:  %f\n", cDeltaMove.x, cDeltaMove.y);
			m_pcWindow->Insert(zBuffer);
			m_pcWindow->Insert("\n");
		} break;
		case M_KEY_DOWN: {
			int32 nRaw;
			int32 nQual;
			
			const char *pzString;
			const char *pzRawString;
			
			char zBuffer1[256];
			char zBuffer2[256];
			char zBuffer3[256];
			char zBuffer4[256];
						
			pcMessage->FindInt32("_raw_key", &nRaw);
			pcMessage->FindInt32("_qualifiers", &nQual);
			pcMessage->FindString("_string", &pzString);
			pcMessage->FindString("_raw_string", &pzRawString);
			
			m_pcWindow->Insert("****** Voyeur ****** : M_KEY_DOWN\n");
			snprintf(zBuffer1, sizeof(zBuffer1), "****** Voyeur ****** : Raw Key Code:  %d\n", nRaw);
			snprintf(zBuffer2, sizeof(zBuffer2), "****** Voyeur ****** : Qualifier Mask:  %d\n", nQual);
			snprintf(zBuffer3, sizeof(zBuffer3), "****** Voyeur ****** : String:  %s\n", pzString);
			snprintf(zBuffer4, sizeof(zBuffer4), "****** Voyeur ****** : Raw String:  %s\n", pzRawString);
			m_pcWindow->Insert(zBuffer1);
			m_pcWindow->Insert(zBuffer2);
			m_pcWindow->Insert(zBuffer3);
			m_pcWindow->Insert(zBuffer4);
			m_pcWindow->Insert("\n");
		} break;
		case M_KEY_UP: {
			int32 nRaw;
			int32 nQual;
			
			const char *pzString;
			const char *pzRawString;
			
			char zBuffer1[256];
			char zBuffer2[256];
			char zBuffer3[256];
			char zBuffer4[256];
						
			pcMessage->FindInt32("_raw_key", &nRaw);
			pcMessage->FindInt32("_qualifiers", &nQual);
			pcMessage->FindString("_string", &pzString);
			pcMessage->FindString("_raw_string", &pzRawString);
			
			m_pcWindow->Insert("****** Voyeur ****** : M_KEY_UP\n");
			snprintf(zBuffer1, sizeof(zBuffer1), "****** Voyeur ****** : Raw Key Code:  %d\n", nRaw);
			snprintf(zBuffer2, sizeof(zBuffer2), "****** Voyeur ****** : Qualifier Mask:  %d\n", nQual);
			snprintf(zBuffer3, sizeof(zBuffer3), "****** Voyeur ****** : String:  %s\n", pzString);
			snprintf(zBuffer4, sizeof(zBuffer4), "****** Voyeur ****** : Raw String:  %s\n", pzRawString);
			m_pcWindow->Insert(zBuffer1);
			m_pcWindow->Insert(zBuffer2);
			m_pcWindow->Insert(zBuffer3);
			m_pcWindow->Insert(zBuffer4);
			m_pcWindow->Insert("\n");
		} break;
	}
	
	return MF_DISPATCH_MESSAGE;
}

//----------------------------------------------------------------------------
// NAME:
// DESC:
// NOTE:
// SEE ALSO:
//----------------------------------------------------------------------------

status_t Voyeur::InitCheck( void )
{
	CALLED();
	
	return EOK;
}

//----------------------------------------------------------------------------
// NAME:
// DESC:
// NOTE:
// SEE ALSO:
//----------------------------------------------------------------------------

extern "C" InputServerFilter* init_isf_addon()
{
	CALLED();
		
	// this is where it all starts
	// make sure this function is exported!
	
    try {
		InputServerFilter* pcISF = new Voyeur();
		return( pcISF );
    } catch( std::exception&  cExc ) {
		return( NULL );
    }
}

