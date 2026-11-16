os::String GetAppName()
{
	os::Application *pApp = os::Application::GetInstance();
	if( pApp )
	{
		const char *pzAppSig = pApp->GetName().c_str();
		const char *pzAppName = strrchr( pzAppSig, '-' );

		// First check for a '-'
		if( pzAppName != NULL )
			pzAppName++;

		// If not found, check for '/'
		else if( strrchr( pzAppSig, '/' ) != NULL )
		{
			pzAppName = strrchr( pzAppSig, '/' ) + 1;
		}

		// Simply use the name directly
		else
			pzAppName = pzAppSig;

		if( pzAppName && strlen( pzAppName ) > 1 )
		{
			return os::String( pzAppName );
		}
	}
	errno = -EINVAL;
	perror( "Cannot find the application to get the name\n" );
	return os::String();
}

os::Settings GetSettings()
{
	if( getenv( "HOME" ) == NULL )
	{
		errno = -EACCES;
		perror( "Home is not defined" );
		return os::Settings();
	}
	os::String cPath = os::String().Format( "%s/Settings/%s", getenv( "HOME" ), GetAppName().c_str() );
	return os::Settings( new os::File( cPath ) );
}

os::String GetSettingsPath( const os::String &provider )
{
	os::String home = getenv( "HOME" ); 

	if( home.empty() ){
		errno = -EINVAL;
		perror("Home is not set in the environment");
		return os::String();
	}

	if( provider.empty() )
	{
		errno = -EINVAL;
		perror( "Provider cannot be empty" );
		return os::String();
	}

	os::String cPath = os::String().Format( "%s/Settings/%s/%s/Settings", home.c_str(), GetAppName().c_str(), provider.c_str() );
	return cPath;
}

os::String GetCachePath( const os::String &provider )
{
	os::String home = getenv( "HOME" ); 

	if( home.empty() ){
		errno = -EINVAL;
		perror("Home is not set in the environment");
		return os::String();
	}

	if( provider.empty() )
	{
		errno = -EINVAL;
		perror( "Provider cannot be empty" );
		return os::String();
	}
	os::String cPath = os::String().Format( "%s/Settings/%s/%s/Cache", home.c_str(), GetAppName().c_str(), provider.c_str() );
	return cPath;
}
