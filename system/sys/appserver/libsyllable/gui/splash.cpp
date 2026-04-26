#include <gui/splash.h>

using namespace os;


class SplashView : public View
{
public:
	SplashView(os::BitmapImage*,const os::String& cText, bool bEnableProgress, float vProgress);
	~SplashView();

	virtual void Paint(const Rect& cUpdateRect);
	Point GetPreferredSize(bool) const;
	void SetText(const os::String& cText);
	void SetImage(os::BitmapImage* pcImage);
	void SetTextColor(const os::Color32_s& sColor);
	void SetProgress(float vProgress);
	void SetEnableProgressBar(bool bEnable);
	void SetEnable(bool bEnable);
	void SetFont(os::Font*);
private:
	BitmapImage* m_pcImage;
	BitmapImage* m_pcGrayImage;

	os::String m_cText;

	Color32_s m_sTextColor;
	Color32_s m_sBgColor;

	os::ProgressBar* m_pcProgressBar;
	bool m_bEnableProgress;
	float m_vProgress;
	int m_nTextStart;
	int m_nTextEnd;
	bool m_bEnabled;
};

class Splash::Private
{
public:
	SplashView* m_pcView;
};


SplashView::SplashView(os::BitmapImage* pcImage, const os::String& cText, bool bEnableProgress, float vProgress) : View(Rect(0,0,1,1),"",WID_WILL_DRAW)
{
	m_pcImage = pcImage;

	if (m_pcImage)
	{
		m_pcGrayImage = new BitmapImage( *( dynamic_cast < BitmapImage * >( m_pcImage ) ) );
		m_pcGrayImage->ApplyFilter( Image::F_GRAY );
	}

	m_cText = cText;
	m_bEnableProgress = bEnableProgress;
	m_vProgress = vProgress;
	m_sTextColor = Color32_s(0,0,0,255);
	m_sBgColor = Color32_s(255,255,255,255);
	m_bEnabled = true;
	m_nTextStart = m_bEnableProgress && !cText.empty() ? (int)GetPreferredSize(false).y-40 : 0;
	m_nTextEnd = m_bEnableProgress && !cText.empty() ?  (int)GetPreferredSize(false).y-21 : 0;

	ResizeTo(GetPreferredSize(false));


	if (m_bEnableProgress)
	{
		m_pcProgressBar = new os::ProgressBar(os::Rect(4, GetPreferredSize(false).y-20,GetBounds().Width()-4,GetPreferredSize(false).y-4),"progress");
		m_pcProgressBar->SetProgress(m_vProgress);
		AddChild(m_pcProgressBar);
	}

}


SplashView::~SplashView()
{
	delete m_pcImage;
	delete m_pcGrayImage;
}

void SplashView::Paint(const Rect& cUpdateRect)
{
	if (m_bEnabled && !m_cText.empty())
	{
		SetFgColor(m_sBgColor);
		SetBgColor(m_sTextColor);
		SetEraseColor(m_sBgColor);
	}
	else
	{
		SetFgColor(get_default_color(COL_NORMAL));
	}


	if (m_pcImage)
	{
		SetDrawingMode(DM_BLEND);
		!m_bEnabled ? m_pcGrayImage->Draw(Point(0,0),this) : m_pcImage->Draw(Point(0,0),this);
		SetDrawingMode(DM_COPY);
	}

	if (!m_cText.empty())
	{
		SetFgColor(get_default_color(COL_SHADOW));
		SetDrawingMode(DM_OVER);
		m_bEnabled ? SetFgColor(m_sTextColor) : SetFgColor(get_default_color(COL_SHADOW));
		DrawText(os::Rect(4, m_nTextStart, GetBounds().Width() - 4, m_nTextEnd), m_cText);
	}
}

void SplashView::SetImage(os::BitmapImage* pcImage)
{
	m_pcImage = pcImage;

	if (pcImage)
	{
		m_pcGrayImage = new BitmapImage( *( dynamic_cast < BitmapImage * >( m_pcImage ) ) );
		m_pcGrayImage->ApplyFilter( Image::F_GRAY );
	}

	Flush();
	Sync();
	Invalidate();
}

void SplashView::SetText(const os::String& cText)
{
	m_cText = cText;
	m_nTextStart = m_bEnableProgress && !cText.empty() ? (int)GetPreferredSize(false).y-40 : 0;
	m_nTextEnd = m_bEnableProgress && !cText.empty() ?  (int)GetPreferredSize(false).y-21 : 0;
	Flush();
	Sync();
	Invalidate(true);
}

void SplashView::SetTextColor(const Color32& sColor)
{
	m_sTextColor = sColor;
	Flush();
	Sync();
	Invalidate();
}

void SplashView::SetEnableProgressBar(bool bEnable)
{
	if (m_bEnableProgress == bEnable && bEnable == true)
		return;
	else
	{
		m_bEnableProgress = bEnable;
		ResizeTo(GetPreferredSize(false));

		if (bEnable)
		{
			m_pcProgressBar = new os::ProgressBar(os::Rect(4, GetPreferredSize(false).y-20,GetBounds().Width()-4,GetPreferredSize(false).y-4),"progress");
			m_pcProgressBar->SetProgress(m_vProgress);
			AddChild(m_pcProgressBar);
		}
		else
		{
			RemoveChild(m_pcProgressBar);
		}
	}
}

void SplashView::SetProgress(float vProgress)
{
	m_vProgress = vProgress;
	m_pcProgressBar->SetProgress(vProgress);
}

void SplashView::SetEnable(bool bEnable)
{
	m_bEnabled = bEnable;

	Flush();
	Sync();
	Invalidate();
}

void SplashView::SetFont(os::Font* pcFont)
{
	View::SetFont(pcFont);
	Flush();
	Sync();
	Invalidate();
}

os::Point SplashView::GetPreferredSize(bool bLargest) const
{
	return m_pcImage->GetSize();
}


Splash::Splash(os::BitmapImage* pcImage, const os::String& cText, bool bEnableProgress, float vProgress) : Window(Rect(0,0,1,1),"title","title",WND_NOT_RESIZABLE | WND_NO_BORDER)
{
	m = new Private;
	m->m_pcView = new SplashView(pcImage,cText,bEnableProgress,vProgress);
	AddChild(m->m_pcView);

	ResizeTo(m->m_pcView->GetPreferredSize(false).x,m->m_pcView->GetPreferredSize(false).y);
}


bool Splash::OkToQuit()
{
	PostMessage(M_QUIT,this);
	return true;
}

void Splash::SetText(const os::String& cText)
{
	m->m_pcView->SetText(cText);
}

void Splash::SetImage(os::BitmapImage* pcImage)
{
	m->m_pcView->SetImage(pcImage);
}

void Splash::SetTextColor(const os::Color32_s& sColor)
{
	m->m_pcView->SetTextColor(sColor);
}

void Splash::SetFont(os::Font* pcFont)
{
	m->m_pcView->SetFont(pcFont);

	m->m_pcView->Flush();
	m->m_pcView->Sync();
	m->m_pcView->Invalidate();
}

void Splash::SetBgColor(const os::Color32_s& sColor)
{
	m->m_pcView->SetBgColor(sColor);

	m->m_pcView->Flush();
	m->m_pcView->Sync();
	m->m_pcView->Invalidate();
}

void Splash::SetEnableProgressBar(bool bEnable)
{
	m->m_pcView->SetEnableProgressBar(bEnable);
	ResizeTo(m->m_pcView->GetPreferredSize(false));
}

void Splash::SetProgress(float vProgress)
{
	m->m_pcView->SetProgress(vProgress);
}

void Splash::SetEnable(bool bEnable)
{
	m->m_pcView->SetEnable(bEnable);
}

os::Splash* Splash::Go(os::BitmapImage* pcImage, const os::String& cText)
{
	os::Splash* splash = new os::Splash(pcImage,cText,true,0);
	splash->CenterInScreen();
	splash->Show();
	splash->MakeFocus();
	return splash;
}

void Splash::Quit(os::Window* pcWindow)
{
	Hide();
	pcWindow->Show();
	pcWindow->MakeFocus();
	OkToQuit();
}
