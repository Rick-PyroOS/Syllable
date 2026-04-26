#include "gui/dateview.h"

#include <gui/point.h>
#include <gui/image.h>
#include <gui/bitmap.h>
#include <storage/memfile.h>
#include <util/application.h>
#include <gui/control.h>
#include <gui/guidefines.h>

using namespace os;

//Static months of year
const static os::String MonthsOfYear[13] = {"","January","February","March","April","May","June","July","August","September","October","November","December"};

//size of arrows
os::IPoint ARROW_SIZE = os::IPoint(4,7);

//left arrow
uint8 BMID_ARROW_LEFT[] =
{
	0xff, 0xff, 0xff, 0x00,
	0xff, 0xff, 0x00, 0x00,
	0xff, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0xff, 0x00, 0x00, 0x00,
	0xff, 0xff, 0x00, 0x00,
	0xff, 0xff, 0xff, 0x00
};

//right arrow
uint8 BMID_ARROW_RIGHT[] =
{		
	0x00, 0xff, 0xff, 0xff,
	0x00, 0x00, 0xff, 0xff,
	0x00, 0x00, 0x00, 0xff,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0xff,
	0x00, 0x00, 0xff, 0xff,
	0x00, 0xff, 0xff, 0xff
};



/*Year view*/
class DateView::YearView : public os::HLayoutNode
{	
public:
		YearView(int);
		void SetTarget(const os::Handler*);	
		
		void SetYear(int);
		int GetYear() const;
		void Increment();
		void Decrement();
private:
		int m_nYear;
	
		os::StringView* m_pcStringView;
		os::ImageButton* m_pcBackButton;
		os::ImageButton* m_pcNextButton;
};


/*Month View*/
class DateView::MonthView : public os::HLayoutNode
{
public:
	MonthView(int);
	void SetTarget(const os::Handler*);
	
	void SetMonth(int);
	int GetMonth() const;
	void Increment();
	void Decrement();
private:
	int m_nMonth;
	
	os::StringView* m_pcStringView;
	os::ImageButton* m_pcBackButton;
	os::ImageButton* m_pcNextButton;
};


class DateView::Private
{
public:
	Private()
	{
		m_pcParent = NULL;
		m_cDate = os::DateTime::Now();
	}
	
	os::Messenger* m_pcParent;
	os::DateTime m_cDate;
	YearView* m_pcYearView;
	MonthView* m_pcMonthView;
	os::HLayoutNode* m_pcRoot;
};



DateView::YearView::YearView(int nYear) : os::HLayoutNode("year_view")
{
	m_nYear = nYear;
	
	m_pcBackButton = new os::ImageButton(os::Rect(),"back_button","back_button",new os::Message(DateView::M_YEAR_PREV),new os::BitmapImage(BMID_ARROW_LEFT,ARROW_SIZE,os::CS_CMAP8));
	m_pcNextButton = new os::ImageButton(os::Rect(0,0,4,7),"next_button","next_button",new os::Message(DateView::M_YEAR_NEXT),new os::BitmapImage(BMID_ARROW_RIGHT,ARROW_SIZE,os::CS_CMAP8));
	m_pcStringView = new os::StringView(os::Rect(0,0,1,1),"display_string",os::String().Format("%d",m_nYear));
	
	AddChild(m_pcBackButton);
	AddChild(new os::HLayoutSpacer("",2,2));
	AddChild(m_pcStringView);
	AddChild(new os::HLayoutSpacer("",2,2));
	AddChild(m_pcNextButton);
	LimitMaxSize(GetPreferredSize(false));	
	ExtendMinSize(GetPreferredSize(false));
	ExtendMaxSize(GetPreferredSize(false));
}

int DateView::YearView::GetYear() const
{
	return m_nYear;
}

void DateView::YearView::SetYear(int nYear)
{
	m_nYear = nYear;
	m_pcStringView->SetString(os::String().Format("%d",m_nYear));
}


void DateView::YearView::SetTarget(const os::Handler* handler)
{
	m_pcBackButton->SetTarget(handler);
	m_pcNextButton->SetTarget(handler);
}

void DateView::YearView::Increment()
{
	m_pcStringView->SetString(os::String().Format("%d",++m_nYear));
	
}

void DateView::YearView::Decrement()
{
	m_pcStringView->SetString(os::String().Format("%d",--m_nYear));
}




DateView::MonthView::MonthView(int nMonth) : os::HLayoutNode("month_view")
{
	m_nMonth = nMonth;
	
	m_pcBackButton = new os::ImageButton(os::Rect(),"back_button","",new os::Message(M_MONTH_PREV),new os::BitmapImage(BMID_ARROW_LEFT,ARROW_SIZE,os::CS_CMAP8));
	m_pcNextButton = new os::ImageButton(os::Rect(0,0,4,7),"next_button","",new os::Message(M_MONTH_NEXT),new os::BitmapImage(BMID_ARROW_RIGHT,ARROW_SIZE,os::CS_CMAP8));
	m_pcStringView = new os::StringView(os::Rect(0,0,1,1),"display_string",MonthsOfYear[m_nMonth],os::DTF_CENTER);
	
	AddChild(m_pcBackButton);
	AddChild(new os::HLayoutSpacer("",2,2));
	AddChild(m_pcStringView);
	AddChild(new os::HLayoutSpacer("",2,2));
	AddChild(m_pcNextButton);
	LimitMaxSize(os::Point(120,GetPreferredSize(false).y));
	ExtendMaxSize(os::Point(120,GetPreferredSize(false).y));
	ExtendMinSize(os::Point(120,GetPreferredSize(false).y));
}


void DateView::MonthView::SetMonth(int n)
{
	m_nMonth = n;
	m_pcStringView->SetString(MonthsOfYear[m_nMonth]);
}

int DateView::MonthView::GetMonth() const
{
	return m_nMonth;
}

void DateView::MonthView::SetTarget(const os::Handler* handler)
{
	m_pcBackButton->SetTarget(handler);
	m_pcNextButton->SetTarget(handler);
}

void DateView::MonthView::Increment()
{
	if (m_nMonth >= 12)
	{
		m_nMonth = 1;
	}
	else
		m_nMonth++;
		
	m_pcStringView->SetString(MonthsOfYear[m_nMonth]);
	
}

void DateView::MonthView::Decrement()
{
	if (m_nMonth >= 12)
	{
		m_nMonth = 1;
	}
	else if (m_nMonth <= 1)
		m_nMonth = 12;
	else
		m_nMonth--;
		
	m_pcStringView->SetString(MonthsOfYear[m_nMonth]);	
}



DateView::DateView(const os::DateTime& date,os::Messenger* pcTarget) : os::LayoutView(os::Rect(0,0,1,1),"",NULL,os::CF_FOLLOW_NONE)
{
	m = new Private;
	m->m_cDate = date;
	
	pcTarget == NULL ? m->m_pcParent = new os::Messenger(os::Application::GetInstance()) : m->m_pcParent = pcTarget;
	
	_Layout();
}

DateView::~DateView()
{
	delete m;
}


void DateView::AllAttached()
{
	m->m_pcMonthView->SetTarget(this);
	m->m_pcYearView->SetTarget(this);
}



void DateView::_Layout()
{
	m->m_pcRoot = new os::HLayoutNode("dateview_root");
	m->m_pcRoot->AddChild(m->m_pcMonthView = new MonthView(m->m_cDate.GetMonth()));	
	m->m_pcRoot->AddChild(new os::HLayoutSpacer("",10,10));
	m->m_pcRoot->AddChild(m->m_pcYearView = new YearView(m->m_cDate.GetYear()));

	SetRoot(m->m_pcRoot);	
}

void DateView::HandleMessage(os::Message* pcMessage)
{
	switch (pcMessage->GetCode())
	{
		case M_YEAR_NEXT:
		{
			m->m_pcYearView->Increment();
			
			os::Message cMsg(M_YEAR_CHANGED);
			cMsg.AddObject("date",os::DateTime(m->m_pcYearView->GetYear(),m->m_pcMonthView->GetMonth(),1));
			m->m_pcParent->SendMessage(&cMsg);
			break;
		}
		
		case M_YEAR_PREV:
		{
			m->m_pcYearView->Decrement();
			
			os::Message cMsg(M_YEAR_CHANGED);
			cMsg.AddObject("date",os::DateTime(m->m_pcYearView->GetYear(),m->m_pcMonthView->GetMonth(),1));
			m->m_pcParent->SendMessage(&cMsg);
			break;
		}
		
		case M_MONTH_NEXT:
		{
			m->m_pcMonthView->Increment();
			os::Message cMsg(M_MONTH_CHANGED);
			cMsg.AddObject("date",os::DateTime(m->m_pcYearView->GetYear(),m->m_pcMonthView->GetMonth(),1));
			m->m_pcParent->SendMessage(&cMsg);
			break;
		}
		
		case M_MONTH_PREV:
		{
			m->m_pcMonthView->Decrement();
			os::Message cMsg(M_MONTH_CHANGED);
			cMsg.AddObject("date",os::DateTime(m->m_pcYearView->GetYear(),m->m_pcMonthView->GetMonth(),1));
			m->m_pcParent->SendMessage(&cMsg);
			break;
		}
	}
}

int DateView::GetMonth() const
{
	return m->m_pcMonthView->GetMonth();
}

int DateView::GetYear() const
{
	return m->m_pcYearView->GetYear();
}

void DateView::SetMessageTarget(os::Messenger* pcTarget)
{
	pcTarget == NULL ? m->m_pcParent = new os::Messenger(os::Application::GetInstance()) : m->m_pcParent = pcTarget;	
}




















