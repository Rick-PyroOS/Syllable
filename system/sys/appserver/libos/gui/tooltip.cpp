#include <gui/tooltip.h>

using namespace os;

ToolTipView::ToolTipView(const os::Rect& cRect,const os::String& cName, const os::String& cText) : os::View(cRect,cName,CF_FOLLOW_ALL)
{
	m_cText = cText;
	m_sToolTipBgColor = Color32_s(255,255,100,150);
	m_sToolTipFgColor = Color32_s(0,0,0);
}
		
void ToolTipView::Paint(const os::Rect& cRect)
{
	os::Rect cBounds = GetBounds();
	FillRect(cBounds,m_sToolTipBgColor);
	
	SetDrawingMode(DM_OVER);
	SetFgColor(m_sToolTipFgColor);
	DrawText(GetBounds(),m_cText,DTF_DEFAULT);
}
		
void ToolTipView::SetToolTipBgColor(const Color32_s& sBgColor)
{
	m_sToolTipBgColor = sBgColor;
}
		
void ToolTipView::SetToolTipFgColor(const Color32_s& sFgColor)
{
	m_sToolTipFgColor = sFgColor;
}
		
void ToolTipView::SetText(const os::String& cText)
{
	m_cText = cText;
}

os::String ToolTipView::GetText() const
{
	return m_cText;
}

os::Point ToolTipView::GetPreferredSize(bool b) const
{
	return GetTextExtent(m_cText);
}

ToolTip::ToolTip(const os::String& t) : os::Window( os::Rect(0,0,1,1), "tooltip_window", "tooltip_window2",WND_NO_BORDER)
{
	pcView = new ToolTipView(GetBounds(),"tool_tip_string",t);
	AddChild(pcView);
}
	
	
void ToolTip::SetTip(const os::String& t)
{
	pcView->SetText(t);
}
	
os::String ToolTip::GetTip() const
{
	return pcView->GetText();
}
	
void ToolTip::TimerTick(int nID)
{
	if (nID == TIP_TIMER)
	{
		Hide();
	}
}

void ToolTip::ShowTip()
{
	if (!IsVisible() && pcView->GetText() != "")
	{
		ResizeTo(pcView->GetPreferredSize(false));
		Show();
		AddTimer(this,TIP_TIMER,3000000);
	}
}





