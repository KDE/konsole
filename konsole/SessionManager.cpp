
SessionInfo::SessionInfo(const QString& path)
{
    KSimpleConfig* _config = new KSimpleConfig(KStandardDirs::locate("appdata",path) , true );
}
SessionInfo::~SessionInfo()
{
    delete _config;
}

QString SessionInfo::name()
{
    return _config->readEntry("Name");
}

QString SessionInfo::icon()
{
    return _config->readEntry("Icon","konsole");
}


bool SessionInfo::isRootSession()

    const QString& cmd = config->readEntry("Exec");

    return ( cmd.startsWith("su") )
}

QString SessionInfo::command()
{
    const QString& fullCommand = config->readEntry("Exec");
    
    if ( !isRootSession() )
        return fullCommand;
    else
    {
        //command is of the form "su -flags 'commandname'"
        //we need to strip out and return just the command name part.
        
        return fullCommand.section('\'',1,1);
    } 
}

bool SessionInfo::isAvailable()
{
    //TODO:  Is it necessary to cache the result of the search? 
    
    QString binary = KRun::binaryName( command(true) , false );
    binary = KShell::tildeExpand(exec);

    QString fullBinaryPath = KGlobal::dirs()->findExe(binary);

    if ( fullBinaryPath.isEmpty() )
        return false;        
    else
        return true;
}


QString SessionInfo::terminal()
{
    return _config->readEntry("Term","xterm");
}
QString SessionInfo::keyboardSetup()
{
    return _config->readEntry("KeyTab",QString());
}
QString SessionInfo::colorScheme()
{
    //TODO Pick a default color scheme
    return _config->readEntry("Schema");
}
QString SessionInfo::defaultFont()
{
    return _config->readEntry("defaultfont").value<QFont>();
}
QString SessionInfo::defaultWorkingDirectory()
{
    return _config->readPathEntry("Cwd");
}

SessionManager::SessionManager()
{
    //locate default session
   KConfig* appConfig = KGlobal::config();
   appConfig->setDesktopGroup();

   _defaultSessionPath = appConfig->readEntry("DefaultSession","shell.desktop");

    //locate config files and extract the most important properties of them from
    //the config files.
    //
    //the sessions are only parsed completely when a session of this type 
    //is actually created
    QList<QString> files = KGlobal::dirs()->findAllResources("appdata", "*.desktop", false, true);

    QListIterator<QString> fileIter(files);
   
    while (fileIter.hasNext())
    { 
        QString configFile = fileIter.next();
        
        if (configFile != _defaultSessionPath)
            _types << SessionInfo(configFile);
    }
}

TESession* SessionManager::createSession(QString configPath , const QString& initialDir)
{
    //search for SessionInfo object built from this config path
    QListIterator<SessionInfo> iter(_types);
    
    while (iter.hasNext())
    {
        SessionInfo& info = iter.next();

        if ( info.path() == configPath() )
        {
            //configuration information found, create a new session based on this
            TESession* session = new TESession();

                
            _sessions << session;          
        }
    }
    
    delete sessionConfig;
}
