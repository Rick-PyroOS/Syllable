#ifndef	__F_WAITERS_ENGINE_H__
#define	__F_WAITERS_ENGINE_H__

/** \file waiter/engine.h
 * \par Description:
 *	Screensaver system
 * \author Dee Sharpe (demetrioussharpe@netscape.net)
 *****************************************************************************/
#include <os/types.h>
#include <os/kernel.h>

#include <gui/view.h>

#include <util/application.h>
#include <util/message.h>

#include <waiters/screensaver.h>


#ifdef __cplusplus
namespace os
{
#endif


class Engine : public Application
{
public:
	Engine();
	virtual ~Engine();
	
	status_t	LoadSaver( String &cDevice );
	status_t	UnloadSaver( void );
	
	void		Start( void );
	
	void		SetView( View *pcView );
	status_t	StartSaver( bool bPreview );
	void		StopSaver( void );
	void		Draw( int32 nFrame );
	void		StartConfig( View *pcConfigView );
	void		StopConfig( void );
	
	status_t	SaveState( Message *pcState ) const;
	void		SupplyInfo( Message *pcInfo ) const;
	void		ModulesChanged( const Message *pcInfo );
	
private:
	status_t			StartSaverThread( void *pData );
	void				SaverThread( void );
	static bool			m_bSaveScreen;
	static bool			m_bQuit;
	ScreenSaver			*m_pcSaver;
	View				*m_pcView;
	View				*m_pcPreview;
	thread_id			m_hSaverThread;
};

#ifdef __cplusplus
} // End of namespace
#endif

#endif	// __F_WAITERS_ENGINE_H__


