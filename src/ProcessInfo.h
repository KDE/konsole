/*
    SPDX-FileCopyrightText: 2007-2008 Robert Knight <robertknight@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef PROCESSINFO_H
#define PROCESSINFO_H

// Qt
#include <QFile>
#include <QString>
#include <QVector>

namespace Konsole
{
/**
 * Takes a snapshot of the state of a process and provides access to
 * information such as the process name, parent process,
 * the foreground process in the controlling terminal,
 * the arguments with which the process was started.
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
     */
    static ProcessInfo *newInstance(int pid);

    virtual ~ProcessInfo()
    {
    }

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
    int pid(bool *ok) const;
    /**
     * Returns the id of the parent process id was read successfully or false otherwise
     *
     * @param ok Set to true if the parent process id
     */
    int parentPid(bool *ok) const;

    /**
     * Returns the id of the current foreground process
     *
     * NOTE:  Using the foregroundProcessGroup() method of the Pty
     * instance associated with the terminal of interest is preferred
     * over using this method.
     *
     * @param ok Set to true if the foreground process id was read successfully or false otherwise
     */
    int foregroundPid(bool *ok) const;

    /* Returns the user id of the process */
    int userId(bool *ok) const;

    /** Returns the user's name of the process */
    QString userName() const;

    /** Returns the user's home directory of the process */
    QString userHomeDir() const;

    /** Returns the local host */
    static QString localHost();

    /** Returns the name of the current process */
    QString name(bool *ok) const;

    /**
     * Returns the command-line arguments which the process
     * was started with.
     *
     * The first argument is the name used to launch the process.
     *
     * @param ok Set to true if the arguments were read successfully or false otherwise.
     */
    QVector<QString> arguments(bool *ok) const;

    /**
     * Returns the current working directory of the process
     *
     * @param ok Set to true if the current working directory was read successfully or false otherwise
     */
    QString currentDir(bool *ok) const;

    /**
     * Returns the current working directory of the process (or its parent)
     */
    QString validCurrentDir() const;

    /** Forces the user home directory to be calculated */
    void setUserHomeDir();

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
        PermissionsError,
    };

    /**
     * Returns the last error which occurred.
     */
    Error error() const;

    enum Field {
        PROCESS_ID = 1,
        PARENT_PID = 2,
        FOREGROUND_PID = 4,
        ARGUMENTS = 8,
        NAME = 16,
        CURRENT_DIR = 32,
        UID = 64,
    };
    Q_DECLARE_FLAGS(Fields, Field)

    // takes a full directory path and returns a
    // shortened version suitable for display in
    // space-constrained UI elements (eg. tabs)
    QString formatShortDir(const QString &dirPath) const;

    void setUserNameRequired(bool need);

protected:
    /**
     * Constructs a new process instance.  You should not call the constructor
     * of ProcessInfo or its subclasses directly.  Instead use the
     * static ProcessInfo::newInstance() method which will return
     * a suitable ProcessInfo instance for the current platform.
     */
    explicit ProcessInfo(int pid);

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
     */
    virtual void readProcessInfo(int pid) = 0;

    /**
     * Determine the current directory of the process.
     * @param pid process ID to use
     * @return true on success
     */
    virtual bool readCurrentDir(int pid) = 0;

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
    void setUserName(const QString &name);
    /** Sets the name of the process as returned by name() */
    void setName(const QString &name);
    /** Sets the current working directory for the process */
    void setCurrentDir(const QString &dir);

    /** Sets the error */
    void setError(Error error);

    /** Convenience method.  Sets the error based on a QFile error code. */
    void setFileError(QFile::FileError error);

    /**
     * Adds a commandline argument for the process, as returned
     * by arguments()
     */
    void addArgument(const QString &argument);

    /**
     * clear the commandline arguments for the process, as returned
     * by arguments()
     */
    void clearArguments();

    bool userNameRequired() const;

private:
    Fields _fields;

    int _pid;
    int _parentPid;
    int _foregroundPid;
    int _userId;

    Error _lastError;

    QString _name;
    QString _userName;
    QString _userHomeDir;
    QString _currentDir;

    bool _userNameRequired;

    QVector<QString> _arguments;

    static QStringList commonDirNames();
    static QStringList _commonDirNames;
};
Q_DECLARE_OPERATORS_FOR_FLAGS(ProcessInfo::Fields)

}

#endif // PROCESSINFO_H
