#ifndef __MENU__PROVIDER__H
#define __MENU__PROVIDER__H

#include "../provider.h"

class MenuProvider : public Provider
{
public:
	MenuProvider();

	virtual ~MenuProvider();
	std::vector<Result> Process( const os::String & );

	status_t Launch( const Result &result );

private:
	os::RegistrarManager *m_pcManager;
};

#endif
