#ifndef DATE_VIEW_H
#define DATE_VIEW_H

#include <gui/view.h>
#include <gui/layoutview.h>
#include <util/datetime.h>
#include <gui/imagebutton.h>
#include <gui/stringview.h>



namespace os
{
class DateView : public os::LayoutView
{
public:
	DateView(const os::DateTime&,os::Messenger* pcTarget=NULL);
	~DateView();
public:
	void AllAttached();
	void HandleMessage(os::Message*);

	int GetMonth() const;
	int GetYear() const;
	void SetMessageTarget(os::Messenger*);
private:
	enum
	{
		M_MONTH_NEXT,
		M_MONTH_PREV,
		M_YEAR_NEXT,
		M_YEAR_PREV
	};	
private:
	class YearView;
	class MonthView;
private:	
	class Private;
	Private* m;
	
private:	
	void _Layout();

private:

};
}

#endif

















