#ifndef __COMMAND_H__
#define __COMMAND_H__


namespace common {

typedef void (*callback)(char* szData);

class Command
{
public:
	Command( callback pfnCB );
	~Command();
	
	/* Command execution: runs szCommand (NULL-terminated argv), passes its
	   stdout to the callback in NUL-terminated chunks and returns its exit
	   status (see command.cpp) */
	int			Execute( char** szCommand );
	void		SetCallback( callback pfnCB );
	
private:
	callback	m_pfnCallback;
};

}; /* namespace common */

#endif /* __COMMAND_H__ */

