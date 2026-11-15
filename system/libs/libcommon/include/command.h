#ifndef __COMMAND_H__
#define __COMMAND_H__


namespace common {

typedef void (*callback)(char* szData);

class Command
{
public:
	Command( callback pfnCB );
	~Command();
	
	/* Command execution */
	void		Execute( char** szCommand );
	void		SetCallback( callback pfnCB );
	
private:
	callback	m_pfnCallback;
};

}; /* namespace common */

#endif /* __COMMAND_H__ */

