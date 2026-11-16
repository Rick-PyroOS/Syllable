#ifndef __TESTWIN_H__
#define __TESTWIN_H__

#include <gui/window.h>
#include <gui/imageview.h>
#include <util/message.h>

#define NUM_BOXES	100

using namespace os;

class BouncingBox
{
	public:
	BouncingBox();
	~BouncingBox();
	void Animate( Rect& cBounds, View* pcView );
	void Initialize( const Point& cPosition );
	private:
	Point			m_cDelta;
	Point			m_cPosition;
	Color32			m_cColor;
	BitmapImage*	m_pcBoxImage;
};

class Win : public os::Window
{
public:
    Win( const os::Rect& cRect );
    ~Win();

	void TimerTick( int nID );
	void RenderFrame( View* pcView );

	bool OkToQuit();
private:
	BitmapImage*	m_pcImage;
	ImageView*		m_pcImgView;
	BouncingBox		m_caBoxes[ NUM_BOXES ];
};

#endif // __TESTTWIN_H__






