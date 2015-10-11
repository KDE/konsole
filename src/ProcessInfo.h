/*
    Copyright 2007-2008 by Robert Knight <robertknight@gmail.com>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
    02110-1301  USA.
*/

#ifndef PROCESSINFO_H
#define PROCESSINFO_H

// Qt
#include <QtCore/QFile>
#include <QtCore/QMap>
#include <QtCore/QString>
#include <QtCore/QVector>

namespace Konsole
{
/**
 * Takes a snapshot of the state of a process and provides access to
 * information such as the process name, parent process,
 * the foreground process in the controlling terminal,
 * the arguments with which the process was started and the
 * environment.
 *
 * To create a new snapshot, construct a new ProcessInfo instance,
 * using ProcessInfo::newInstance(),
 * passing the process identifier of the process you are interested in.
 *
 * After creating a new instance, call the update() method to take a
 * snapshot of the current state of the process.
 *
 * Before calling any additional methods, check that the process state
 * was read successfully using the isValid() method.
 *
 * Each accessor method which provides information about the process state ( such as pid(),
 * currentDir(), name() ) takes a pointer to a boolean as an argument.  If the information
 * requested was read successfully then the boolean is set to true, otherwise it is set
 * to false, in which case the return value from the function should be ignored.
 * If this boolean is set to false, it may indicate an error reading the process information,
 * or it may indicate that the information is not available on the current platform.
 *
 * eg.
 *
 * @code
 *   ProcessInfo* info = ProcessInfo::newInstance(pid);
 *   info->update();
 *
 *   if ( info->isValid() )
 *   {
 *      bool ok;
 *
 *      QString name = info->name(&ok);
 *      if ( ok ) qDebug() << "process name - " << name;
 *      int parentPid = info->parentPid(&ok);
 *      if ( ok ) qDebug() << "parent process - " << parentPid;
 *      int foregroundPid = info->foregroundPid(&ok);
 *      if ( ok ) qDebug() << "foreground process - " << foregroundPid;
 *   }
 * @endcode
 */
class ProcessInfo
{
public:
    /**
     * Constructs a new instance of a suitable ProcessInfo sub-class for
     * the current platform which provides information about a given process.
     *
     * @param pid The pid of the process to examine
     * @param readEnvironment Specifies whether environment bindings should
     * be read.  If this is false, then environment() calls will
     * always fail.  This is an optimization to avoid the overhead
     * of reading the (potentially large) environment data when it
     * is not required.
     */
    static ProcessInfo* newInstance(int pid, bool readEnvironment = false);

    virtual ~ProcessInfo() {}

    /**
     * Updates the information about the process.  This must
     * be called before attempting to use any of the accessor methods.
     */
    void update();

    /** Returns true if the process state was read successfully. */
    bool isValid() const;
    /**
     * Returns the process id.
     *
     * @param ok Set to true if the process id was read successfully or false otherwise
     */
    int pid(bool* ok) const;
    /**
     * Returns the id of the parent process id was read successfully or false otherwise
     *
     * @param ok Set to true if the parent process id
     */
    int parentPid(bool* ok) const;

    /**
     * Returns the id of the current foreground process
     *
     * NOTE:  Using the foregroundProcessGroup() method of the Pty
     * instance associated with the terminal of interest is preferred
     * over using this method.
     *
     * @param ok Set to true if the foreground process id was read successfully or false otherwise
     */
    int foregroundPid(bool* ok) const;

    /* Returns the user id of the process */
    int userId(bool* ok) const;

    /** Returns the user's name of the process */
    QString userName() const;

    /** Returns the user's home directory of the process */
    QString userHomeDir() const;

    /** Returns the local host */
    static QString localHost();

    /** Returns the name of the current process */
    QString name(bool* ok) const;

    /**
     * Returns the command-line arguments which the process
     * was started with.
     *
     * The first argument is the name used to launch the process.
     *
     * @param ok Set to true if the arguments were read successfully or false otherwise.
     */
    QVector<QString> arguments(bool* ok) const;
    /**
     * Returns the environment bindings which the process
     * was started with.
     * In the returned map, the key is the name of the environment variable,
     * and the value is the corresponding value.
     *
     * @param ok Set to true if the environment bindings were read successfully or false otherwise
     */
    QMap<QString, QString> environment(bool* ok) const;

    /**
     * Returns the current working directory of the process
     *
     * @param ok Set to true if the current working directory was read successfully or false otherwise
     */
    QString currentDir(bool* ok) const;

    /**
     * Returns the current working directory of the process (or its parent)
     */
    QString validCurrentDir() const;

    /** Forces the user home directory to be calculated */
    void setUserHomeDir();

    /**
     * Parses an input string, looking for markers beginning with a '%'
     * character and returns a string with the markers replaced
     * with information from this process description.
     * <br>
     * The markers recognized are:
     * <ul>
     * <li> %u - Name of the user which owns the process. </li>
     * <li> %n - Replaced with the name of the process.   </li>
     * <li> %d - Replaced with the last part of the path name of the
     *      process' current working directory.
     *
     *      (eg. if the current directory is '/home/bob' then
     *      'bob' would be returned)
     * </li>
     * <li> %D - Replaced with the current working directory of the process. </li>
     * </ul>
     */
    QString format(const QString& text) const;

    /**
     * This enum describes the errors which can occur when trying to read
     * a process's information.
     */
    enum Error {
        /** No error occurred. */
        NoError,
        /** The nature of the error is unknown. */
        UnknownError,
        /** Konsole does not have permission to obtain the process information. */
        PermissionsError
    };

    /**
     * Returns the last error which occurred.
     */
    Error error() const;

    enum Field {
        PROCESS_ID          = 1,
        PARENT_PID          = 2,
        FOREGROUND_PID      = 4,
        ARGUMENTS           = 8,
        ENVIRONMENT         = 16,
        NAME                = 32,
        CURRENT_DIR         = 64,
        UID                 = 128
    };
    Q_DECLARE_FLAGS(Fields, Field)

protected:
    /**
     * Constructs a new process instance.  You should not call the constructor
     * of ProcessInfo or its subclasses directly.  Instead use the
     * static ProcessInfo::newInstance() method which will return
     * a suitable ProcessInfo instance for the current platform.
     */
    explicit ProcessInfo(int pid , bool readEnvironment = false);

    /**
     * This is called on construction to read the process state
     * Subclasses should reimplement this function to provide
     * platform-specific process state reading functionality.
     *
     * When called, readProcessInfo() should attempt to read all
     * of the necessary state information.  If the attempt is successful,
     * it should set the process id using setPid(), and update
     * the other relevant information using setParentPid(), setName(),
     * setArguments() etc.
     *
     * Calls to isValid() will return true only if the process id
     * has been set using setPid()
     *
     * @param pid The process id of the process to read
     * @param readEnvironment Specifies whether the environment bindings
     *                        for the process should be read
     */
    virtual bool readProcessInfo(int pid , bool readEnvironment) = 0;

    /* Read the user name */
    virtual void readUserName(void) = 0;

    /** Sets the process id associated with this ProcessInfo instance */
    void setPid(int pid);
    /** Sets the parent process id as returned by parentPid() */
    void setParentPid(int pid);
    /** Sets the foreground process id as returned by foregroundPid() */
    void setForegroundPid(int pid);
    /** Sets the user id associated with this ProcessInfo instance */
    void setUserId(int uid);
    /** Sets the user name of the process as set by readUserName() */
    void setUserName(const QString& name);
    /** Sets the name of the process as returned by name() */
    void setName(const QString& name);
    /** Sets the current working directory for the process */
    void setCurrentDir(const QString& dir);

    /** Sets the error */
    void setError(Error error);

    /** Convenience method.  Sets the error based on a QFile error code. */
    void setFileError(QFile::FileError error);

    /**
     * Adds a commandline argument for the process, as returned
     * by arguments()
     */
    void addArgument(const QString& argument);

    /**
     * clear the commandline arguments for the process, as returned
     * by arguments()
     */
    void clearArguments();

    /**
     * Adds an environment binding for the process, as returned by
     * environment()
     *
     * @param name The name of the environment variable, eg. "PATH"
     * @param value The value of the environment variable, eg. "/bin"
     */
    void addEnvironmentBinding(const QString& name , const QString& value);

private:
    // takes a full directory path and returns a
    // shortened version suitable for display in
    // space-constrained UI elements (eg. tabs)
    QString formatShortDir(const QString& dirPath) const;

    Fields _fields;

    bool _enableEnvironmentRead; // specifies whether to read the environment
    // bindings when update() is called
    int _pid;
    int _parentPid;
    int _foregroundPid;
    int _userId;

    Error _lastError;

    QString _name;
    QString _userName;
    QString _userHomeDir;
    QString _currentDir;

    QVector<QString> _arguments;
    QMap<QString, QString> _environment;

    static QSet<QString> commonDirNames();
    static QSet<QString> _commonDirNames;
};
Q_DECLARE_OPERATORS_FOR_FLAGS(ProcessInfo::Fields)

/**
 * Implementation of ProcessInfo which does nothing.
 * Used on platforms where a suitable ProcessInfo subclass is not
 * available.
 *
 * isValid() will always return false for instances of NullProcessInfo
 */
class NullProcessInfo : public ProcessInfo
{
public:
    /**
     * Constructs a new NullProcessInfo instance.
     * See ProcessInfo::newInstance()
     */
    explicit NullProcessInfo(int pid, bool readEnvironment = false);
protected:
    virtual bool readProcessInfo(int pid, bool readEnvironment);
    virtual void readUserName(void);
};

#if !defined(Q_OS_WIN)
/**
 * Implementation of ProcessInfo for Unix platforms which uses
 * the /proc filesystem
 */
class UnixProcessInfo : public ProcessInfo
{
public:
    /**
     * Constructs a new instance of UnixProcessInfo.
     * See ProcessInfo::newInstance()
     */
    explicit UnixProcessInfo(int pid, bool readEnvironment = false);

protected:
    /**
     * Implementation of ProcessInfo::readProcessInfo(); calls the
     * four private methods below in turn.
     */
    virtual bool readProcessInfo(int pid , bool readEnvironment);

    virtual void readUserName(void);

private:
    /**
     * Read the standard process information -- PID, parent PID, foreground PID.
     * @param pid process ID to use
     * @return true on success
     */
    virtual bool readProcInfo(int pid) = 0;

    /**
     * Read the environment of the process. Sets _environment.
     * @param pid process ID to use
     * @return true on success
     */
    virtual bool readEnvironment(int pid) = 0;

    /**
     * Determine what arguments were passed to the process. Sets _arguments.
     * @param pid process ID to use
     * @return true on success
     */
    virtual bool readArguments(int pid) = 0;

    /**
     * Determine the current directory of the process.
     * @param pid process ID to use
     * @return true on success
     */
    virtual bool readCurrentDir(int pid) = 0;
};
#endif

/**
 * Lightweight class which provides additional information about SSH processes.
 */
class SSHProcessInfo
{
public:
    /**
     * Constructs a new SSHProcessInfo instance which provides additional
     * information about the specified SSH process.
     *
     * @param process A ProcessInfo instance for a SSH process.
     */
    explicit SSHProcessInfo(const ProcessInfo& process);

    /**
     * Returns the user name which the user initially logged into on
     * the remote computer.
     */
    QString userName() const;

    /**
     * Returns the host which the user has connected to.
     */
    QString host() const;

    /**
     * Returns the port on host which the user has connected to.
     */
    QString port() const;

    /**
     * Returns the command which the user specified to execute on the
     * remote computer when starting the SSH process.
     */
    QString command() const;

    /**
     * Operates in the same way as ProcessInfo::format(), except
     * that the set of markers understood is different:
     *
     * %u - Replaced with user name which the user initially logged
     *      into on the remote computer.
     * %h - Replaced with the first part of the host name which
     *      is connected to.
     * %H - Replaced with the full host name of the computer which
     *      is connected to.
     * %c - Replaced with the command which the user specified
     *      to execute when starting the SSH process.
     */
    QString format(const QString& input) const;

private:
    const ProcessInfo& _process;
    QString _user;
    QString _host;
    QString _port;
    QString _command;
};
}
#endif //PROCESSINFO_H
