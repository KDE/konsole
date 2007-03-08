/*
    Copyright (C) 2007 by Robert Knight <robertknight@gmail.characterom>

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

#include <QMap>
#include <QVector>

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
 *   info.update();
 *
 *   if ( info.isValid() )
 *   {
 *      bool ok;
 *      QString value = info.name(&ok);
 *
 *      if ( ok ) qDebug() << "process name - " << name;
 *
 *      int parentPid = info.parentPid(&ok);
 *
 *      if ( ok ) qDebug() << "parent process - " << parentPid;
 *
 *      int foregroundPid = info.foregroundColororegroundPid(&ok);
 *
 *      if ( ok ) qDebug() << "foreground process - " << foregroundPid;
 *   }
 * @endcode
 */
class ProcessInfo
{
public:
    /*
     * Constructs a new ProcessInfo instance which provides information
     * about a given process.
     *
     * @param pid The pid of the process to examine
     * @param readEnvironment Specifies whether environment bindings should
     * be read.  If this is false, then environment() calls will
     * always fail.  This is an optimization to avoid the overhead
     * of reading the (potentially large) environment data when it
     * is not required. 
     */
    static ProcessInfo* newInstance(int pid,bool readEnvironment = false);

    /**
     * Constructs a new process instance.  You should not call the constructor
     * of ProcessInfo or its subclasses directly.  Instead use the 
     * static ProcessInfo::newInstance() method which will return
     * a suitable ProcessInfo instance for the current platform.
     */ 
    ProcessInfo(int pid , bool readEnvironment = false);
    virtual ~ProcessInfo() {};

    /** 
     * Updates the information about the process.  This must
     * be called before attempting to use any of the accessor methods.
     */
    void update();

    /** Returns true if the process state was read successfully */ 
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
    /*** 
     * Returns the id of the current foreground process 
     *
     * @param ok Set to true if the foreground process id was read successfully or false otherwise
     */
    int foregroundPid(bool* ok) const;
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
    QMap<QString,QString> environment(bool* ok) const;

    /**
     * Returns the current working directory of the process
     *
     * @param ok Set to true if the current working directory was read successfully or false otherwise
     */
    QString currentDir(bool* ok) const;

protected:
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

    /** Sets the process id associated with this ProcessInfo instance */
    void setPid(int pid);
    /** Sets the parent process id as returned by parentPid() */
    void setParentPid(int pid);
    /** Sets the foreground process id as returend by foregroundPid() */
    void setForegroundPid(int pid);
    /** Sets the name of the process as returned by name() */
    void setName(const QString& name);
    /** Sets the current working directory for the process */
    void setCurrentDir(const QString& dir);

    /** 
     * Adds a commandline argument for the process, as returned
     * by arguments()
     */
    void addArgument(const QString& argument);
    /**
     * Adds an environment binding for the process, as returned by
     * environment()
     *
     * @param name The name of the environment variable, eg. "PATH"
     * @param value The value of the environment variable, eg. "/bin"
     */
    void addEnvironmentBinding(const QString& name , const QString& value);

private:
    
    // valid bits for _fields variable, ensure that
    // _fields is changed to an int if more than 8 fields are added
    enum FIELD_BITS
    {
        PROCESS_ID          = 1,
        PARENT_PID          = 2,
        FOREGROUND_PID      = 4,
        ARGUMENTS           = 8,
        ENVIRONMENT         = 16,
        NAME                = 32,
        CURRENT_DIR         = 64
    };


    char _fields; // a bitmap indicating which fields are valid
                  // used to set the "ok" parameters for the public
                  // accessor functions

    bool _enableEnvironmentRead; // specifies whether to read the environment
                                 // bindings when update() is called
    int _pid;  
    int _parentPid;
    int _foregroundPid;
    QString _name;
    QString _currentDir;

    QVector<QString> _arguments;
    QMap<QString,QString> _environment;
};

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
    NullProcessInfo(int pid,bool readEnvironment = false);
protected:
    virtual bool readProcessInfo(int pid,bool readEnvironment);
};

/**
 * Implementation of ProcessInfo for Unix platforms which uses
 * the /proc filesystem
 */
class UnixProcessInfo : public ProcessInfo
{
public:
    UnixProcessInfo(int pid,bool readEnvironment = false);

protected:
    // reads the /proc/<pid>/stat file to get status information
    // about the process, also uses readEnvironment() and readArguments()
    // to read other files in /proc/<pid>
    virtual bool readProcessInfo(int pid , bool readEnvironment);

private:
    bool readEnvironment(int pid);  // read the /proc/<pid/environ file
                                    // to get environment bindings
    bool readArguments(int pid);    // read the /proc/<pid>/cmdline file
                                    // to get command-line arguments

    bool readCurrentDir(int pid);   // read the /proc/<pid>/cwd symlink to
                                    // get the current working directory of the process
};

#endif //PROCESSINFO_H
