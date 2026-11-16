#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>

#include <gui/window.h>
#include <gui/view.h>


#include <string>
#include <list>
#include <vector>

#include <util/message.h>
#include <util/messenger.h>
#include "messages.h"

#ifndef _HTMLVIEW_H_
#define _HTMLVIEW_H_


class HTMLPart;

namespace os

{


class HTMLView : public os::View
{
public:
	HTMLView( const Rect& cFrame, const String& cTitle, WebCore::ChromeClient* pcClient, 
	  uint32 nResizeMask = CF_FOLLOW_LEFT | CF_FOLLOW_TOP,
	  uint32 nFlags = WID_WILL_DRAW | WID_CLEAR_BACKGROUND );
	~HTMLView();
	virtual void KeyDown( const char* pzString, const char* pzRawString, uint32 nQualifiers );
	WebCore::Frame* GetWebCoreFrame();
	
	int GetBackListCount();
	int GetForwardListCount();
	os::String GetCurrentURL();
	void OpenURL( const WebCore::ResourceRequest& cRequest);
	void GoBack();
	void GoForward();
	void Reload();
	void Stop();
	bool IsLoading();
private:
	void SetPart( HTMLPart *pcPart );
	class Private;
	Private* m;
	
};



}

#endif


























