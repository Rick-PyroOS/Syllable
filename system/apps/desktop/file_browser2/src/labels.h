#ifndef __LABELS__H_
#define __LABELS__H_

#include <util/settings.h>
#include <gui/gfxtypes.h>
#include <gui/menu.h>
#include <util/string.h>
#include <util/message.h>
#include <gui/point.h>
#include <gui/font.h>

#include <map>

struct Label
{
	char name[1024];
	os::Color32_s color;
};

typedef std::map<os::String,os::Color32_s> LabelType;


class LabelMenuItem : public os::MenuItem
{
public:
	LabelMenuItem( const os::String& cLabel, os::Message* pcMsg, os::Color32_s color );
	~LabelMenuItem();

	virtual void  Draw();
	virtual void  SetHighlighted( bool bHighlighted );
	virtual os::Point GetContentSize() const;

	virtual os::Color32_s GetColor();

	virtual bool Invoked(os::Message *pcMessage);
	virtual bool IsSelectable() const;
private:
	LabelMenuItem( os::Menu* pcMenu, os::Message* pcMsg, os::Color32_s );
	LabelMenuItem& operator=( const LabelMenuItem& );
	LabelMenuItem( const LabelMenuItem& );

	bool m_Highlighted;
	bool m_Enabled;
	bool m_bIsSelectable;
	os::Color32_s m_sColor;
};

class Labels
{
public:
	static Labels* GetInstance();
	
	Labels();
	
	LabelType GetLabels();
	
	void SaveSettings();
	void LoadSettings();
	os::Menu* GetMenu(int message);	
private:	
	class Private;
	Private* m;
private:	
	static Labels* m_theInstance;	
};

#endif














