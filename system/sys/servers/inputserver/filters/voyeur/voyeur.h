

#ifndef _VOYEUR_H
#define _VOYEUR_H

#include <os/types.h>

#include <servers/inputserver/inputserverfilter.h>
#include <servers/inputserver/protocol.h>

#include "voyeurwindow.h"


class Voyeur : public InputServerFilter
{
public:
	Voyeur( void );
	~Voyeur();
	
	MessageFilterResult Filter( Message* pcMessage, MessageList tOutList );
	status_t InitCheck( void );
	
private:
	VoyeurWindow*	m_pcWindow;
};


#endif	// _VOYEUR_H

