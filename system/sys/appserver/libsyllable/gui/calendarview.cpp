#include <gui/calendarview.h>
#include <util/application.h>

const static os::String DaysOfTheWeek[7] = {"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};
const static int days_in_month[13] = { 0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };

using namespace os;


class CalendarView::Private
{
public:
	Private()
	{
		m_pcParent = NULL;
	}

	os::DateTime m_cDate;
	os::TableView* m_pcView;
	os::DateView* m_pcDateView;
	os::Messenger* m_pcParent;
	bool m_bShowDate;	
};


class CalendarView::DayView : public os::View
{
public:	
	DayView(const os::String&);

	//LibSyllable functions
	public:	
		void Paint(const os::Rect&);
		void MouseMove( const os::Point& cNewPos, int nCode, uint32 nButtons, os::Message* pcData );
		void FrameSized( const os::Point & cDelta );		
		void MouseDown( const os::Point& cPosition, uint32 nButtons );		

	public:
		void SetWeekend(bool week);
		void SetCurrentDay(bool current);
		void SetMessenger(const os::Messenger&);
		
		void SetString(const os::String& c);	
		os::String GetString();
	
	
		bool IsWeekend();
		bool IsCurrentDay();
		bool IsSelected();
		bool IsMouseOver();
	

	private:
		bool m_bIsWeekend;
		bool bIsSelected;
		bool m_bIsCurrentDay;
		bool m_bMouseOver;
		os::String m_cDayString;
		os::Messenger cParent;	
};

class CalendarView::DOWView : public os::View
{
	public:
		DOWView(const os::String&);
		void FrameSized( const os::Point & cDelta );
		void Paint(const os::Rect& c);		
	private:
		os::String m_cWeekString;
};	


CalendarView::DOWView::DOWView(const os::String& cName) : os::View(os::Rect(),cName)
{
	GetFont()->SetSize(9.0f);
}

void CalendarView::DOWView::Paint(const os::Rect& c)
{
	SetFgColor(os::get_default_color(os::COL_NORMAL));
	FillRect(GetBounds(),GetFgColor());	
	SetFgColor(0,0,0,255);
	SetDrawingMode(os::DM_OVER);	
	DrawText(GetBounds(),GetName(),os::DTF_ALIGN_CENTER);
	
}

void CalendarView::DOWView::FrameSized( const os::Point & cDelta )
{
	View::FrameSized(cDelta);
	Flush();
	Invalidate();
}


CalendarView::DayView::DayView(const os::String& cName) : os::View(os::Rect(0,0,1,1),cName,os::CF_FOLLOW_ALL)
{
	GetFont()->SetSize(9.0f);	
	m_cDayString = cName;
	m_bIsWeekend = m_bMouseOver = m_bIsCurrentDay = false;
}

void CalendarView::DayView::Paint(const os::Rect&)
{
	
	SetDrawingMode( os::DM_COPY );

	float offset = 4.0f;
	os::Rect cFrame = GetBounds();

	// Draw selection if mouse is inside the button
	if( m_bMouseOver )
	{
		os::Rect cFillRect = GetBounds();
		cFillRect.bottom = 3;
		cFillRect = GetBounds();
		cFillRect.Resize( 1, 4, -1 , -4 );
		FillRect( cFillRect, os::get_default_color( os::COL_ICON_SELECTED ) );
	
		SetFgColor( os::get_default_color( os::COL_ICON_SELECTED ) );
		DrawLine( os::Point( 4, 1 ), os::Point( GetBounds().right - 4, 1 ) );
		DrawLine( os::Point( 2, 2 ), os::Point( GetBounds().right - 2, 2 ) );
		DrawLine( os::Point( 2, 3 ), os::Point( GetBounds().right - 2, 3 ) );

		cFillRect.top = GetBounds().bottom - 3;
		cFillRect.bottom = GetBounds().bottom;
		FillRect( cFillRect, os::get_default_color( os::COL_NORMAL ) );

		SetFgColor( os::get_default_color( os::COL_ICON_SELECTED ) );
		DrawLine( os::Point( 4, cFillRect.bottom - 1 ), os::Point( GetBounds().right - 4, cFillRect.bottom - 1 ) );
		DrawLine( os::Point( 2, cFillRect.bottom - 2 ), os::Point( GetBounds().right - 2, cFillRect.bottom - 2 ) );
		DrawLine( os::Point( 2, cFillRect.bottom - 3 ), os::Point( GetBounds().right - 2, cFillRect.bottom - 3 ) );
	}
	else if (m_bIsCurrentDay)
	{
		os::Rect cFillRect = GetBounds();
		cFillRect.bottom = 3;
		cFillRect = GetBounds();
		cFillRect.Resize( 1, 4, -1 , -4 );
		FillRect( cFillRect, os::get_default_color( os::COL_FOCUS ) );
	
		SetFgColor( os::get_default_color( os::COL_FOCUS ) );
		DrawLine( os::Point( 4, 1 ), os::Point( GetBounds().right - 4, 1 ) );
		DrawLine( os::Point( 2, 2 ), os::Point( GetBounds().right - 2, 2 ) );
		DrawLine( os::Point( 2, 3 ), os::Point( GetBounds().right - 2, 3 ) );

		cFillRect.top = GetBounds().bottom - 3;
		cFillRect.bottom = GetBounds().bottom;
		FillRect( cFillRect, os::get_default_color( os::COL_NORMAL ) );

		SetFgColor( os::get_default_color( os::COL_FOCUS ) );
		DrawLine( os::Point( 4, cFillRect.bottom - 1 ), os::Point( GetBounds().right - 4, cFillRect.bottom - 1 ) );
		DrawLine( os::Point( 2, cFillRect.bottom - 2 ), os::Point( GetBounds().right - 2, cFillRect.bottom - 2 ) );
		DrawLine( os::Point( 2, cFillRect.bottom - 3 ), os::Point( GetBounds().right - 2, cFillRect.bottom - 3 ) );
	}
	else
		FillRect( cFrame, os::get_default_color( os::COL_NORMAL ) );	

	if (!m_cDayString.empty())
		DrawRoundedFrame();


	if (m_bIsWeekend)
		SetFgColor(0,0,255,255);
	else
		SetFgColor(0,0,0,255);
		
	
	SetDrawingMode(os::DM_OVER);
	DrawText(GetBounds(),m_cDayString,os::DTF_ALIGN_CENTER);
}

void CalendarView::DayView::SetWeekend(bool week)
{
	m_bIsWeekend = week;
}

void CalendarView::DayView::SetCurrentDay(bool current)
{
	m_bIsCurrentDay = current;
}

void CalendarView::DayView::SetMessenger(const os::Messenger& c)
{
	cParent = c;
}


void CalendarView::DayView::MouseDown( const os::Point& cPosition, uint32 nButtons )
{
	if (nButtons == 0x01)
	{
		cParent.SendMessage(new os::Message(M_CALENDAR_DATE_INVOKED));
	}
}

void CalendarView::DayView::MouseMove( const os::Point& cNewPos, int nCode, uint32 nButtons, os::Message* pcData )
{
	if (m_cDayString.empty())
		return;
		
	if( nCode == os::MOUSE_EXITED ) 
	{
		m_bMouseOver = false;
	}
	else if (nCode == os::MOUSE_ENTERED)
	{
		m_bMouseOver = true;
	}
	
	Flush();
	Invalidate();
}

void CalendarView::DayView::FrameSized( const os::Point & cDelta )
{
	View::FrameSized(cDelta);
	Flush();
	Invalidate();
}

void CalendarView::DayView::SetString(const os::String& c)
{
	m_cDayString = c;
	Flush();
	Invalidate();
}

os::String CalendarView::DayView::GetString()
{
	return (m_cDayString);
}




CalendarView::CalendarView(const os::Rect& cRect,const os::String& cName,os::Messenger* parent,const os::DateTime& date, bool bShowDateView,uint32 nResizeMask, uint32 nFlags) : os::LayoutView(cRect,cName,NULL,nResizeMask,nFlags)
{
	m = new Private();
	m->m_cDate = date;
	
	if (parent)
		m->m_pcParent = parent;
	else
		m->m_pcParent = new os::Messenger(os::Application::GetInstance());
	m->m_bShowDate = bShowDateView;
	
	
	_Layout();
	Update();
}


void CalendarView::AllAttached()
{
	for (int i=0; i<7; i++)
	{
		for (int j=1; j<7; j++)
		{
			os::TableCell* cell = m->m_pcView->GetCell(i,j);
			DayView* view = dynamic_cast<DayView*> (cell->GetView());
			os::Messenger cMessenger(this);
			view->SetMessenger(cMessenger);
		}
	}
	if (m->m_pcDateView)
		m->m_pcDateView->SetMessageTarget(new os::Messenger(this));
}


void CalendarView::SetDate(const os::DateTime& cDate)
{
	m->m_cDate = cDate;
	Update();
}

os::DateTime CalendarView::GetDate() const
{
	return m->m_cDate;
}

void CalendarView::Update()
{
	os::DateTime dow = os::DateTime(m->m_cDate.GetYear(),m->m_cDate.GetMonth(),1);	
	int day_of_week = dow.GetDayOfWeek();
	int start = 1;
	
	for (int i=0; i<7; i++)
	{
		for (int j=1; j<7; j++)
		{
			os::TableCell* cell = m->m_pcView->GetCell(i,j);
			DayView* view = dynamic_cast<DayView*> (cell->GetView());
			view->SetString("");
			view->SetWeekend(false);
			view->SetCurrentDay(false);
		}
	}
	
	
	for (int j=1; j<7; j++)
	{
		for (int i=0; i<7; i++)
		{
			if (start <= days_in_month[dow.GetMonth()])
			{
			
				if (j == 1)
				{
					if (i >= day_of_week)
					{
						goto update;
					}			
				}
				else
				{	
					update:
						os::TableCell* cell = m->m_pcView->GetCell(i,j);
						DayView* view = dynamic_cast<DayView*> (cell->GetView());
						
						view->SetString(os::String().Format("%d",start++));
						if (i == 0 || i == 6)
								view->SetWeekend(true);

						if (atoi(view->GetString().c_str()) == os::DateTime::Now().GetDay() && (os::DateTime::Now().GetMonth() == m->m_cDate.GetMonth()) && (os::DateTime::Now().GetYear() == m->m_cDate.GetYear()) )
								view->SetCurrentDay(true);				
				}
			}
		}
	}
}

	
void CalendarView::Paint(const os::Rect& cRect)
{
	os::LayoutView::Paint(cRect);
	DrawRoundedFrame();
}


void CalendarView::HandleMessage(os::Message* pcMessage)
{
	switch (pcMessage->GetCode())
	{
		case M_MONTH_CHANGED:
		{
			os::DateTime c = os::DateTime(m->m_cDate.GetYear(),m->m_pcDateView->GetMonth(),1);
			SetDate(c);
			break;
		}
		
		case M_YEAR_CHANGED:
		{
			os::DateTime c = os::DateTime(m->m_pcDateView->GetYear(),m->m_cDate.GetMonth(),1);
			SetDate(c);			
			break;
		}
		
		case M_CALENDAR_DATE_INVOKED:
		{
			int32 day;
			os::DateTime c;
			
			if (pcMessage->FindInt32("day",&day) == 0)
			{
				c = os::DateTime(m->m_pcDateView->GetYear(),m->m_pcDateView->GetMonth(),day);
			}
			
			
			os::Message cMsg(M_CALENDAR_DATE_INVOKED);
			cMsg.AddObject("date",c);
			m->m_pcParent->SendMessage(&cMsg);
			break;
		}
	}
}

void CalendarView::_Layout()
{
	m->m_pcView = new os::TableView(os::Rect(),"table","",7,7,os::CF_FOLLOW_ALL);
	
	for (int i=0; i<7; i++)
	{
		m->m_pcView->SetChild(new DOWView(DaysOfTheWeek[i]),i,0);
	}
	
	for (int i=0; i<7; i++)
	{
		for (int j= 1; j<7; j++)
		{
			DayView* view = new DayView("");
			m->m_pcView->SetChild(view,i,j);
		} 
	}
	
	os::VLayoutNode* root = new os::VLayoutNode("calendar_view_root");
	root->SetHAlignment(os::ALIGN_CENTER);
	root->SetVAlignment(os::ALIGN_CENTER);
	root->AddChild(m->m_pcDateView = new DateView(m->m_cDate),0.02f);
	root->AddChild(new os::VLayoutSpacer("",5,5));
	root->AddChild(m->m_pcView);
	SetRoot(root);
}
	























