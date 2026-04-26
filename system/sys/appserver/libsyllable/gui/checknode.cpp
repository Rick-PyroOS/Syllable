#include <gui/checknode.h>
using namespace os;

Bitmap *TreeViewCheckNode::s_pcCheckBitmap = NULL;

#define CHECK_W 10
#define CHECK_H 10

static uint8 nCheckData[] = {
	0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff,
	0x00, 0x00, 0x00, 0x00,
	0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff,
	0x00, 0x00, 0x00, 0x00,
	0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0xff,
	0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0xff, 0xff, 0xff, 0xff,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff,
	0x00, 0x00, 0x00, 0x00,
	0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff
};


TreeViewCheckNode::TreeViewCheckNode(os::Messenger* target)
{
	if( s_pcCheckBitmap == NULL )
	{
		Rect cCheckBitmapRect;

		cCheckBitmapRect.left = 0;
		cCheckBitmapRect.top = 0;
		cCheckBitmapRect.right = CHECK_W;
		cCheckBitmapRect.bottom = CHECK_H;

		s_pcCheckBitmap = new Bitmap( (int)cCheckBitmapRect.Width(), (int)cCheckBitmapRect.Height(  ), CS_RGBA32, Bitmap::SHARE_FRAMEBUFFER );
		memcpy( s_pcCheckBitmap->LockRaster(), nCheckData, ( unsigned int )( cCheckBitmapRect.Width(  ) * cCheckBitmapRect.Height(  ) * 4 ) );
	}
	
	m_pcTarget = target;
	m_bIsChecked = false;	
}

TreeViewCheckNode::~TreeViewCheckNode()
{
}

void TreeViewCheckNode::Paint( const Rect& cFrame, View* pcView, uint nColumn,bool bSelected, bool bHighlighted, bool bHasFocus )
{
	TreeView *pcOwner = GetOwner();
	uint nIndentWidth = 10;
	bool bExp = false;

	if( pcOwner ) 
	{
		nIndentWidth = pcOwner->GetIndentWidth();
		bExp = pcOwner->HasChildren( this );	// FIXME: slow!
	}

	uint nIndent = GetIndent() * nIndentWidth;
	Rect cItemRect( cFrame );
	Point	cTextSize;
	
	if( nColumn <= pcOwner->GetRowCount() ) 
	{
		cTextSize = pcView->GetTextExtent( GetString(nColumn), GetTextFlags() );
	}

	if( nIndent && !nColumn ) 
	{	
		cItemRect.left += nIndent + 15;
		cItemRect.right += nIndent + 15;
	} else 
	{
		nIndent = 0;
	}

	pcView->SetDrawingMode( DM_COPY );
    pcView->SetFgColor( 255, 255, 255 );
    pcView->FillRect( cFrame );

	float vTextPos = cItemRect.left + 3.0f;

	if( GetIcon() && nColumn == 0 ) 
	{
		vTextPos += GetIcon()->GetSize().x + 5;
	}
	
	if( bSelected || bHighlighted )
	{
		Rect cSelectFrame = cFrame;
		cSelectFrame.left = cItemRect.left;
		if( nColumn == 0 ) 
		{
			cSelectFrame.left += 2;
			cSelectFrame.top += 2;
			cSelectFrame.bottom -= 2;
		}
		if( bSelected )
		{
			pcView->SetFgColor( 186, 199, 227 );
		}
		else
		{
			pcView->SetFgColor( 0, 50, 200 );
		}
		
		pcView->FillRect( cSelectFrame );
	
		/* Round edges */
		if( nColumn == 0 )
		{
			pcView->DrawLine( os::Point( cSelectFrame.left + 2, cSelectFrame.top - 2 ), 
								os::Point( cSelectFrame.right, cSelectFrame.top - 2 ) );
			pcView->DrawLine( os::Point( cSelectFrame.left, cSelectFrame.top - 1 ), 
								os::Point( cSelectFrame.right, cSelectFrame.top - 1 ) );
			
			pcView->DrawLine( os::Point( cSelectFrame.left - 2, cSelectFrame.top + 2 ), 
								os::Point( cSelectFrame.left - 2, cSelectFrame.bottom - 2 ) );
			pcView->DrawLine( os::Point( cSelectFrame.left - 1, cSelectFrame.top ), 
								os::Point( cSelectFrame.left - 1, cSelectFrame.bottom ) );
								
			pcView->DrawLine( os::Point( cSelectFrame.left + 2, cSelectFrame.bottom + 2 ), 
								os::Point( cSelectFrame.right, cSelectFrame.bottom + 2 ) );
			pcView->DrawLine( os::Point( cSelectFrame.left, cSelectFrame.bottom + 1 ), 
								os::Point( cSelectFrame.right, cSelectFrame.bottom + 1 ) );
		} 
	}
 
    if ( bHighlighted ) 
    {
		pcView->SetFgColor( 255, 255, 255 );
		pcView->SetBgColor( 0, 50, 200 );
    } 
    else if ( bSelected ) 
    {
		pcView->SetFgColor( 0, 0, 0 );
		pcView->SetBgColor( 186, 199, 227 );
    } 
    else 
    {
		pcView->SetBgColor( 255, 255, 255 );
		pcView->SetFgColor( 0, 0, 0 );
    }
    
		pcView->SetFgColor(0,0,0);
		pcView->SetBgColor(0,0,0);
		m_cCheckFrame = os::Rect(vTextPos,cFrame.top,vTextPos+12,cFrame.top+12);
		pcView->DrawLine( Point( m_cCheckFrame.left, m_cCheckFrame.top ), Point( m_cCheckFrame.right, m_cCheckFrame.top ) );
		pcView->DrawLine( Point( m_cCheckFrame.right, m_cCheckFrame.bottom ) );
		pcView->DrawLine( Point( m_cCheckFrame.left, m_cCheckFrame.bottom ) );
		pcView->DrawLine( Point( m_cCheckFrame.left, m_cCheckFrame.top ) );
		m_cCheckFrame.Resize( 1, 1, -1, -1 );
		
		if( m_bIsChecked )
		{
			pcView->DrawBitmap( s_pcCheckBitmap, s_pcCheckBitmap->GetBounds(), os::Rect(m_cCheckFrame.left+1,m_cCheckFrame.top+1,m_cCheckFrame.left+10,m_cCheckFrame.top+10) );
		}
		vTextPos += 14;	
	
	if( nColumn <= pcOwner->GetRowCount() ) 
	{
		Rect cTextRect( vTextPos, cItemRect.top, cItemRect.right, cItemRect.bottom );
    	pcView->DrawText( cTextRect, GetString(nColumn), GetTextFlags() );
    }

	if( GetIcon() && nColumn == 0 ) 
	{
		pcView->SetDrawingMode( DM_BLEND );
		GetIcon()->Draw( Point( cItemRect.left + 2, cItemRect.top + (cItemRect.Height() / 2 - GetIcon()->GetSize().y / 2) ), pcView );
	}

	/* Draw Trunk (a.k.a connecting lines) */
	if( nColumn == 0 && pcOwner && pcOwner->GetDrawTrunk() ) 
	{
		pcView->SetFgColor( 0, 0, 0 );

		uint32 bits = _GetLinePositions();
		int i = 0;
		for( ; bits ; bits >>= 1, i++ ) {
			if( bits & 1 ) {
				if( !( bits >> 1 ) && IsLastSibling() ) {
					pcView->DrawLine( Point( cFrame.left + ( i + .5 ) * (float)nIndentWidth, cFrame.top ),
							Point( cFrame.left + ( i + .5 ) * (float)nIndentWidth, cFrame.top + cFrame.Height()/2 ) );
				} else {
					pcView->DrawLine( Point( cFrame.left + ( i + .5 ) * (float)nIndentWidth, cFrame.top ),
							Point( cFrame.left + ( i + .5 ) * (float)nIndentWidth, cFrame.bottom ) );
				}
			}
		}
		
		if( i ) 
		{
			if( bExp ) 
			{
				pcView->DrawLine( Point( cFrame.left + ( i - .5 ) * (float)nIndentWidth, cFrame.top + cFrame.Height()/2 ),
						Point( _ExpanderCrossPos( cFrame ).left-1, cFrame.top + cFrame.Height()/2 ) );
			} 
			else 
			{
				pcView->DrawLine( Point( cFrame.left + ( i - .5 ) * (float)nIndentWidth, cFrame.top + cFrame.Height()/2 ),
						Point( cItemRect.left, cFrame.top + cFrame.Height()/2 ) );
			}
		}
	}

	if( nIndent && bExp ) 
	{    
		Rect cExpCr( _ExpanderCrossPos( cFrame ) );

	    _DrawExpanderCross( pcView, cExpCr + Point( cFrame.left, cFrame.top ) );

		if( IsExpanded() && pcOwner && pcOwner->GetDrawTrunk() ) 
		{
	    	pcView->DrawLine( Point( nIndent + cFrame.left + nIndentWidth/2, cExpCr.bottom + cFrame.top ),
					Point( nIndent + cFrame.left + nIndentWidth/2, cFrame.bottom ) );
		}
	}
}

bool TreeViewCheckNode::HitTest( View * pcView, const Rect & cFrame, int nColumn, Point cPos )
{
	os::Point cPoint(cPos);
	cPoint.y = m_cCheckFrame.bottom;
	
	TreeView *pcOwner = GetOwner();
	
	if (m_cCheckFrame.DoIntersect(cPoint))
	{
		Toggle(pcView,pcOwner);
		return false;
	}
	
	else if(nColumn == 0)
	{
		return TreeViewNode::HitTest(pcView,cFrame,nColumn,cPos); 
	}
	
	return ( true );
}

void TreeViewCheckNode::Toggle(os::View* pcView,os::TreeView* pcOwner)
{
	bool bExp = false;
	
	if (pcOwner)
	{
		bExp = pcOwner->HasChildren( this );	// FIXME: slow!
	}
	if (m_bIsChecked)
		m_bIsChecked = false;
	else
		m_bIsChecked = true;
	
	
	if (bExp)
	{
			std::vector<TreeViewNode*> children;
			pcOwner->GetChildren(children, this);
			
			for (int i=0; i<children.size(); i++)
			{
				TreeViewCheckNode* node = (TreeViewCheckNode*)children[i];
				node->SetChecked(!m_bIsChecked);
				node->Toggle(pcView,pcOwner);
			}			
	}		
		
	pcView->Flush();
	pcView->Invalidate();
	
	if (m_pcTarget)
	{
		os::Message cMsg = os::Message(M_CHECKNODE_CHANGED);
		cMsg.AddPointer("source",this);
		cMsg.AddBool("changed",m_bIsChecked);
		m_pcTarget->SendMessage(&cMsg);
	}
		
}

void TreeViewCheckNode::SetChecked(bool bChecked)
{
	m_bIsChecked = bChecked;
}

bool TreeViewCheckNode::IsChecked()
{
	return m_bIsChecked;
}




