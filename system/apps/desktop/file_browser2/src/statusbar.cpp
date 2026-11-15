#include "statusbar.h"


Status::Status() : os::StatusBar(os::Rect(),"")
{
	AddPanel( "message", "" );	
}


Status::~Status()
{
}

void Status::SendMessage(Status::eStatus status,os::String message)
{
}



