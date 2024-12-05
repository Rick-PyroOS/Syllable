



#ifndef __OFFLINE_WINDOW_H__
#define __OFFLINE_WINDOW_H__

#include <os/types.h>
#include <os/kernel.h>

#include <gui/window.h>
#include <gui/textview.h>

#include <util/message.h>

#include <servers/inputserver/protocol.h>

namespace os
{


class OfflineWindow : public Window
{
public:
	OfflineWindow();
	virtual ~OfflineWindow();
	
	status_t HandleInputEventMessage( Message *pcMessage, MessageList tOutList );
		
private:
	TextView	*m_pcView;
};

} // namespace os

#endif	//  __OFFLINE_WINDOW_H__

