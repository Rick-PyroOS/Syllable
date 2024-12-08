#ifndef __F_GUI__RADIOMENU_H__
#define __F_GUI__RADIOMENU_H__

#include <gui/menu.h>
#include <gui/font.h>
#include <gui/view.h>
#include <util/message.h>

namespace os
{
/** RadioMenuItem
 * \ingroup gui
 * \par Description:
 * A RadioMenuItem is a menu item with a radio button attribute.  
 *
 * \sa os::RadiMenu, os::Menu, os::MenuItem
 * \author	Rick Caudill <rick@syllable.org>
 *****************************************************************************/	
class RadioMenuItem : public os::MenuItem
{
public:
	RadioMenuItem(const os::String& cLabel,os::Message* msg);
	virtual void  Draw();
	virtual Point GetContentSize() const;
	void SetHighlighted( bool bHighlighted );	
public:
	
	bool GetRadio();
	void SetRadio(bool b);
	
private:
	static Bitmap* s_pcBitmap;
	bool m_bRadio;
	bool m_Highlighted;
	bool m_Enabled;
};

/** RadioMenu
 * \ingroup gui
 * \par Description:
 * A RadioMenu is a menu that consits of RadioMenuItems.  You can only add RadioMenuItems
 * The reason for only adding RadioMenuItems is so that RadioMenu can tell the menu what RadioMenuItem
 * is to be selected.  
 *
 * \sa os::RadiMenuItem, os::Menu, os::MenuItem
 * \author	Rick Caudill <rick@syllable.org>
 *****************************************************************************/	
class RadioMenu : public os::Menu
{
public:
	RadioMenu( Rect cFrame, const String& cName, MenuLayout_e eLayout,uint32 nResizeMask = CF_FOLLOW_LEFT | CF_FOLLOW_RIGHT | CF_FOLLOW_TOP,uint32 nFlags  = WID_WILL_DRAW | WID_CLEAR_BACKGROUND | WID_FULL_UPDATE_ON_RESIZE );
	
	bool AddItem(os::RadioMenuItem*);
	bool AddItem(os::RadioMenuItem*,int nIndex);
protected:
	void MouseUp( const Point & cPosition, uint32 nButtons, Message * pcData );	

private:
	bool				AddItem( const String& cLabel, Message* pcMessage, const String& cShortcut = "", Image* pcImage = NULL );
    bool				AddItem( MenuItem* pcItem );
    bool				AddItem( MenuItem* pcItem, int nIndex );
    bool				AddItem( Menu* pcItem );
    bool				AddItem( Menu* pcItem, int nIndex );
};
}
#endif



