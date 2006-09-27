#ifndef SESSIONMANAGER_H
#define SESSIONMANAGER_H

class KSimpleConfig;

/** 
 * Provides information about a type of 
 * session, including the title of the session
 * type, whether or not the session will run
 * as root and whether or not the binary
 * for the session is available.
 *
 * The availability of the session type is not determined until the 
 * isAvailable() method is called.
 * 
 */ 
class SessionInfo
{
public:
    /** 
     * Construct a new SessionInfo
     * to provide information on a session type.
     *
     * @p path Path to the configuration file
     * for this type of session
     */
    SessionInfo(const QString& path);

    ~SessionInfo();

    /**
     * Constructs a new SessionInfo to provide
     * information on a session type.
     *
     * @p config Specifies the configuration information loaded from a .desktop file to use
     */
//    SessionInfo(const KConfig* config);
    
    /** 
     * Returns the path to the session's
     * config file
     */
    QString path() const;

    /** Returns the title of the session type */
    QString name() const;
    /** 
     * Returns the path of an icon associated
     * with this session type
     */
    QString icon() const;
    /** 
     * Returns the command that will be executed
     * when the session is run
     *
     * @p stripSu For commands of the form
     * "su -flags 'commandname'", specifies whether
     * to return the whole command string or just
     * the 'commandname' part
     * 
     * eg.  If the command string is 
     * "su -c 'screen'", command(true) will
     * just return "screen"
     */
    QString command(bool stripSu) const;
    /** 
     * Returns true if the session will run as
     * root
     */
    bool isRootSession() const;
    /**
     * Searches the user's PATH for the binary
     * specified in the command string.
     *
     * TODO:  isAvailable() assumes and does not yet verify the 
     * existence of additional binaries(usually 'su' or 'sudo') required 
     * to run the command as root.
     */
    bool isAvailable() const;

    QString terminal() const;
    QString keyboardSetup() const;
    QString colorScheme() const;
    QFont defaultFont() const;
    QString defaultWorkingDirectory() const;

private:
    KConfig* _config;
}

/** 
 * Creates new terminal sessions using information in configuration files.  
 * Information about the available session kinds can be obtained using 
 * availableSessionTypes().  Call createSession() to create a new session.  
 * The session will automatically notify the SessionManager when it finishes running.  
 */ 
class SessionManager 
{
public:
    ~SessionManager();
   
    /**
     * Returns a list of session information
     * objects which describe the kinds
     * of session which are available
     */
    QList<SessionInfo> availableSessionTypes();
    
    /** 
     * Creates a new session of the specified type.
     * The new session has no views associated with it.  A new TEWidget view
     * must be created in order to display the output from the terminal session.
     *
     * @param type Specifies the type of session to create.  Passing an empty
     *             string will create a session using the default configuration.
     * @param initialDir Specifies the initial working directory for the new 
     *        session.  This may be an empty string, in which case the default
     *        directory 
     */
    TESession* createSession(QString configPath = QString() , const QString& initialDir = QString());
    
    /**
     * Returns a list of active sessions.
     */
    QList<TESession*> sessions();

protected Q_SLOTS:

    /**
     * Called to inform the manager that a session has finished executing 
     */
    void sessionTerminated( TESession* session );

private:
    QList<SessionInfo> _types; 
    QList<TESession*> _sessions;
};

#endif //SESSIONMANAGER_H
