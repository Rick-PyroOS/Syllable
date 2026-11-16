#ifndef __STATUSBAR__H__
#define __STATUSBAR__H__

#include <gui/statusbar.h>

class Status : public os::StatusBar
{
public:
	enum eStatus
	{
		STATUS_WARNING,
		STATUS_ERROR,
		STATUS_INFO,
	};
	
public:
	Status();
	~Status();
	
	void SendMessage(eStatus,os::String cMessage);
	//void AttachedToWindow();
	
private:
	class _Private;
	_Private* my;	
};

#endif



