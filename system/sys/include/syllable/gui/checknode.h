#ifndef __F__GUI_CHECKNODE_H__
#define __F__GUI_CHECKNODE_H__

#include <gui/treeview.h>
#include <util/message.h>
#include <util/messenger.h>
#include <gui/guidefines.h>

namespace os
{
class TreeViewCheckNode : public os::TreeViewStringNode
{
public:
    TreeViewCheckNode(os::Messenger* target=NULL);
    virtual ~TreeViewCheckNode();

	// From ListViewRow
    virtual void 	    Paint( const Rect& cFrame, View* pcView, uint nColumn,bool bSelected, bool bHighlighted, bool bHasFocus );
    virtual bool		HitTest( View* pcView, const Rect& cFrame, int nColumn, Point cPos );
	void Toggle(os::View* pcView,os::TreeView* pcOwner=NULL);
	void SetChecked(bool bChecked);
	bool IsChecked();
private:
	bool m_bIsChecked;
	os::Messenger* m_pcTarget;
	static os::Bitmap* s_pcCheckBitmap;
	os::Rect m_cCheckFrame;
};
}
#endif




