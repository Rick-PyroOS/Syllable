

#ifndef _SPLASH__H
#define _SPLASH__H

#include <gui/window.h>
#include <gui/view.h>
#include <gui/image.h>
#include <util/string.h>
#include <gui/progressbar.h>
#include <gui/font.h>


namespace os
{
class Splash : public Window
{
public:
	Splash(os::BitmapImage* pcImage=NULL,const os::String& cText="", bool bEnableProgress=true, float vProgress=0.0f);
	virtual bool OkToQuit();
	
	void SetText(const os::String& cText);
	void SetImage(os::BitmapImage* pcImage);
	void SetTextColor(const Color32_s& sColor);
	void SetBgColor(const os::Color32_s& sColor);
	void SetFlags(uint nFlags);
	void SetEnableProgressBar(bool bEnable);
	void SetProgress(float vProgress);
	void SetEnable(bool bEnable);
	void SetFont(os::Font*);
	
	void Go();
	void Quit(os::Window*);
private:
	class Private;
	Private* m;
};
}
#endif











