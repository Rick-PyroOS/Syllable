



#include <os/types.h>
#include <os/kernel.h>
#include <os/time.h>

#include <gui/window.h>
#include <gui/textview.h>
#include <gui/desktop.h>

#include <posix/errno.h>

#include <util/message.h>
#include <util/string.h>

#include <servers/inputserver/protocol.h>
#include <servers/inputserver/input.h>

#include "offlinewindow.h"

using namespace os;


OfflineWindow::OfflineWindow()
	: Window(Rect(0, 0, 350, 16), "InputServer", "Offline Window", WND_NOT_V_RESIZABLE |
			 WND_NO_CLOSE_BUT | WND_NO_ZOOM_BUT, CURRENT_DESKTOP)
{
	CALLED();
	
	m_pcView = new TextView(GetBounds(), "Offline View", "", CF_FOLLOW_ALL, WID_FULL_UPDATE_ON_RESIZE | WID_WILL_DRAW);
	m_pcView->SetMultiLine(true);
	m_pcView->SetReadOnly(true);
	AddChild(m_pcView);
	
	m_pcView->Set("");
	
	MoveTo(Point(100, Desktop().GetResolution().y * 2 / 3 - GetBounds().Height() / 2));
	Show();
	
	dbprintf("%s: OfflineWindow created\n", __PRETTY_FUNCTION__);
}

OfflineWindow::~OfflineWindow()
{
	CALLED();
	
	if(m_pcView)
		delete m_pcView;
}

status_t OfflineWindow::HandleInputEventMessage( Message *pcMessage, MessageList tOutList )
{
	CALLED();
	
	Message *pcNewMessage;
	const char *zString;
	bool bConfirmed;
	int32 nOpCode;
	int32 nLen;
	int32 nOffset = 0;
	int32 nNextOffset = 0;
	
	if(pcMessage->FindInt32("os:opcode", &nOpCode) != EOK || nOpCode != INPUT_METHOD_CHANGED ||
	   pcMessage->FindBool("os:confirmed", &bConfirmed) != EOK || !bConfirmed ||
	   pcMessage->FindString("os:string", &zString) != EOK)
		return -ENOMSG;
	
	dbprintf("%s: Input method event: %i, %s\n", __PRETTY_FUNCTION__, nOpCode, zString);
	dbprintf("%s: Input method event: confirmed\n", __PRETTY_FUNCTION__);
	
	nLen = strlen(zString);
	
	while(nOffset < nLen) {
		// this should go to the next UTF-8 character
		for(++nNextOffset; (zString[nNextOffset] & 0xC0) == 0x80; ++nNextOffset)
			;
		
		pcNewMessage = new Message(M_KEY_DOWN);
		
		if(pcNewMessage) {
			pcNewMessage->AddInt32("key", 0);
			pcNewMessage->AddInt64("when", get_system_time());
			String cBytes(zString + nOffset, nNextOffset - nOffset);
			pcNewMessage->AddString("bytes", cBytes);
			tOutList.push_front(pcNewMessage);
		}
		
		nOffset = nNextOffset;
	}
}

