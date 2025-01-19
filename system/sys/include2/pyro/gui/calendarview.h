#ifndef CALENDAR_VIEW_H
#define CALENDAR_VIEW_H

#include <gui/view.h>
#include <gui/tableview.h>
#include <gui/stringview.h>
#include <util/datetime.h>
#include <util/handler.h>
#include <util/message.h>
#include <gui/control.h>
#include <gui/layoutview.h>
#include <gui/dateview.h>
#include <gui/font.h>
#include <gui/guidefines.h>

namespace os
{
class CalendarView : public os::LayoutView
{
public:
	CalendarView(const os::Rect&,const os::String& cName,os::Messenger* parent=NULL,const os::DateTime& cDate=os::DateTime::Now(), bool bShowDateView=false,uint32 nResizeMask = os::CF_FOLLOW_ALL, uint32 nFlags  = os::WID_WILL_DRAW | os::WID_CLEAR_BACKGROUND | os::WID_FULL_UPDATE_ON_RESIZE );
	
	void Update();
	
	
	void SetDate(const os::DateTime& cDate);	
	os::DateTime GetDate() const;
	
	
	//Will be moved into libsyllable
	void _DrawRoundedFrame( View* view );
	
	
	//LibSyllable functions
	void Paint(const os::Rect&);
	void HandleMessage(os::Message* pcMessage);
	void AllAttached();	

protected:
	class DayView;
	class DOWView;
	

private:
	void _Layout();

private:
	class Private;
	Private* m;
};
}

#endif

