

namespace os
{
	class BrowserChromeClient;

	class BrowserWindow:public Window
	{
	public:
		BrowserWindow( const Rect & cFrame, bool bSaveSize );
		 ~BrowserWindow();
		static os::Point GetNewWinPosition();
		void OpenURL( const WebCore::FrameLoadRequest& cFrameRequest );
		void CreateNewTab();
		WebCore::Frame* CreateNewTab( const WebCore::FrameLoadRequest& cFrameRequest );
		void SwitchTab();
		void DeleteTab();
		void HandleMessage( Message * pcMsg );
		void TimerTick( int );
		void SetStatusBarText( const os::String& );

		// From Window:
		bool OkToQuit();
		

		TextView *GetURLView();
		WebCore::Frame *GetWebCoreFrame();
		TabView* GetTabs() { return( m_pcTabs ); }
	private:
      	static std::vector<BrowserWindow*> s_cWindows;
	
		void GoHistory( int nStep, bool bButtonsOnly, bool bUpdateURL );
		void PopupMenu( os::String& cURL );

		bool m_bFindActive;
		std::string m_cLastSearchStr;
		bool m_bSearchCaseSensitive;
		LayoutView *m_pcFrame;
		ToolBar *m_pcToolBar;
		ImageButton* m_pcStopButton;		
		StringView* m_pcFindView;
		ImageButton* m_pcFindButton;
		StatusBar *m_pcStatusBar;
		TextView *m_pcURLView;

		os::Menu* m_pcMenuBar;
		os::Menu * m_pcPopupMenu;
		BookmarksMenu * m_pcBookmarkMenu;
		os::TabView * m_pcTabs;
		FindDialog *m_pcSearchDialog;
		bool m_bSaveSize;
		BrowserChromeClient* m_pcChromeClient;
		friend class BrowserApp;
		
	};

	class BrowserApp:public Application
	{
	      public:
		BrowserApp( const char *pzName, int argc, char *argv[] );
		 ~BrowserApp();

		os::String GetHome() { return( m_zHome ); }
		os::String GetProxy() { return( m_zProxy ); }
		void SetSize( const os::Rect& cRect ) { m_cSize = cRect; }
		// From Handler:
		bool OkToQuit();
		void HandleMessage( os::Message* pcMsg );
		//
		private:
			os::Rect m_cSize;
			os::String m_zHome;
			os::String m_zProxy;			
			os::RegistrarManager* m_pcManager;
	};

}






















