

#ifndef _VOYEURWINDOW_H
#define _VOYEURWINDOW_H


#include <os/types.h>

#include <gui/window.h>
#include <gui/textview.h>
#include <gui/rect.h>


using namespace os;

class VoyeurWindow : public Window
{
public:
	VoyeurWindow( const Rect& cFrame );
	~VoyeurWindow();
	
	void Insert( const char* zBuffer );
	
private:
	TextView*	m_pcView;
};


#endif	// _VOYEURWINDOW_H

