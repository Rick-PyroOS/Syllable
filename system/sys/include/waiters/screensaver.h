#ifndef	__F_WAITERS_SCREENSAVER_H__
#define	__F_WAITERS_SCREENSAVER_H__

/** \file waiter/screensaver.h
 * \par Description:
 *	Screensaver system
 * \author Dee Sharpe (demetrioussharpe@netscape.net)
 *****************************************************************************/
#include <os/types.h>
#include <os/kernel.h>

#include <gui/view.h>

#include <storage/path.h>

#include <util/message.h>
#include <util/string.h>


#ifdef __cplusplus
namespace os
{
#endif


class ScreenSaver
{
public:
	ScreenSaver( Message *pcState, int32 nHandle );
	virtual ~ScreenSaver();
	
	virtual status_t	InitCheck( void );
	virtual status_t	StartSaver( View *pcView, bool bPreview );
	virtual void		StopSaver( void );
	virtual void		Draw( View *pcView, int32 nFrame );
	virtual void		StartConfig( View *pcConfigView );
	virtual void		StopConfig( void );
	
	virtual status_t	SaveState( Message *pcState) const;
	virtual void		SupplyInfo( Message *pcInfo ) const;
	virtual void		ModulesChanged( const Message *pcInfo );
	
	void 				SetLoop( int32 nOnCount, int32 nOffCount );
	int32				LoopOnCount( void ) const;
	int32				LoopOffCount( void ) const;
	void				SetTickSize( bigtime_t nSize );
	bigtime_t			TickSize( void ) const;
	
	String				GetPath( void ) const;
	void				SetPath( String& cPath );
	
private:
	friend class Engine;
	int32				GetLibHandle( void )				{	CALLED();	return( m_nLibHandle );		}
	void				SetLibHandle( int32 nLibHandle )	{	CALLED();	m_nLibHandle = nLibHandle;	}
	
	Path				*m_pcPath;
	int32				m_nLibHandle;
	int32				m_nOnCount;
	int32				m_nOffCount;
	bigtime_t			m_nTickSize;
};

typedef ScreenSaver* instantiate_screensaver();


#ifdef __cplusplus
} // End of namespace
#endif

#endif	// __F_WAITERS_SCREENSAVER_H__

