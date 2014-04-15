/*
    Copyright 2007-2008 by Robert Knight <robertknight@gmail.countm>

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

// Config
#include <config-konsole.h>

// Own
#include "ProcessInfo.h"

// Unix
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pwd.h>
#include <sys/param.h>

// Qt
#include <QtCore/QDir>
#include <QtCore/QFileInfo>
#include <QtCore/QTextStream>
#include <QtCore/QStringList>
#include <QtNetwork/QHostInfo>

// KDE
#include <KConfigGroup>
#include <KGlobal>
#include <KSharedConfig>
#include <KUser>
#include <KDebug>

#if defined(Q_OS_FREEBSD) || defined(Q_OS_OPENBSD) || defined(Q_OS_MAC)
#include <sys/sysctl.h>
#endif

#if defined(Q_OS_MAC)
#include <libproc.h>
#include <kde_file.h>
#endif

#if defined(Q_OS_FREEBSD) || defined(Q_OS_OPENBSD)
#include <sys/types.h>
#include <sys/user.h>
#include <sys/syslimits.h>
#   if defined(Q_OS_FREEBSD)
#   include <libutil.h>
#   endif
#endif

using namespace Konsole;

ProcessInfo::ProcessInfo(int aPid , bool enableEnvironmentRead)
    : _fields(ARGUMENTS | ENVIRONMENT)   // arguments and environments
    // are currently always valid,
    // they just return an empty
    // vector / map respectively
    // if no arguments
    // or environment bindings
    // have been explicitly set
    , _enableEnvironmentRead(enableEnvironmentRead)
    , _pid(aPid)
    , _parentPid(0)
    , _foregroundPid(0)
    , _userId(0)
    , _lastError(NoError)
    , _userName(QString())
    , _userHomeDir(QString())
{
}

ProcessInfo::Error ProcessInfo::error() const
{
    return _lastError;
}
void ProcessInfo::setError(Error error)
{
    _lastError = error;
}

void ProcessInfo::update()
{
    readProcessInfo(_pid, _enableEnvironmentRead);
}

QString ProcessInfo::validCurrentDir() const
{
    bool ok = false;

    // read current dir, if an error occurs try the parent as the next
    // best option
    int currentPid = parentPid(&ok);
    QString dir = currentDir(&ok);
    while (!ok && currentPid != 0) {
        ProcessInfo* current = ProcessInfo::newInstance(currentPid);
        current->update();
        currentPid = current->parentPid(&ok);
        dir = current->currentDir(&ok);
        delete current;
    }

    return dir;
}

QString ProcessInfo::format(const QString& input) const
{
    bool ok = false;

    QString output(input);

    // search for and replace known marker
    output.replace("%u", userName());
    output.replace("%h", localHost());
    output.replace("%n", name(&ok));

    QString dir = validCurrentDir();
    if (output.contains("%D")) {
        QString homeDir = userHomeDir();
        QString tempDir = dir;
        // Change User's Home Dir w/ ~ only at the beginning
        if (tempDir.startsWith(homeDir)) {
            tempDir.remove(0, homeDir.length());
            tempDir.prepend('~');
        }
        output.replace("%D", tempDir);
    }
    output.replace("%d", formatShortDir(dir));

    return output;
}

QSet<QString> ProcessInfo::_commonDirNames;

QSet<QString> ProcessInfo::commonDirNames()
{
    static bool forTheFirstTime = true;

    if (forTheFirstTime) {
        const KSharedConfigPtr& config = KGlobal::config();
        const KConfigGroup& configGroup = config->group("ProcessInfo");
        _commonDirNames = QSet<QString>::fromList(configGroup.readEntry("CommonDirNames", QStringList()));

        forTheFirstTime = false;
    }

    return _commonDirNames;
}

QString ProcessInfo::formatShortDir(const QString& input) const
{
    QString result;

    const QStringList& parts = input.split(QDir::separator());

    QSet<QString> dirNamesToShorten = commonDirNames();

    QListIterator<QString> iter(parts);
    iter.toBack();

    // go backwards through the list of the path's parts
    // adding abbreviations of common directory names
    // and stopping when we reach a dir name which is not
    // in the commonDirNames set
    while (iter.hasPrevious()) {
        const QString& part = iter.previous();

        if (dirNamesToShorten.contains(part)) {
            result.prepend(QDir::separator() + part[0]);
        } else {
            result.prepend(part);
            break;
        }
    }

    return result;
}

QVector<QString> ProcessInfo::arguments(bool* ok) const
{
    *ok = _fields & ARGUMENTS;

    return _arguments;
}

QMap<QString, QString> ProcessInfo::environment(bool* ok) const
{
    *ok = _fields & ENVIRONMENT;

    return _environment;
}

bool ProcessInfo::isValid() const
{
    return _fields & PROCESS_ID;
}

int ProcessInfo::pid(bool* ok) const
{
    *ok = _fields & PROCESS_ID;

    return _pid;
}

int ProcessInfo::parentPid(bool* ok) const
{
    *ok = _fields & PARENT_PID;

    return _parentPid;
}

int ProcessInfo::foregroundPid(bool* ok) const
{
    *ok = _fields & FOREGROUND_PID;

    return _foregroundPid;
}

QString ProcessInfo::name(bool* ok) const
{
    *ok = _fields & NAME;

    return _name;
}

int ProcessInfo::userId(bool* ok) const
{
    *ok = _fields & UID;

    return _userId;
}

QString ProcessInfo::userName() const
{
    return _userName;
}

QString ProcessInfo::userHomeDir() const
{
    return _userHomeDir;
}

QString ProcessInfo::localHost()
{
    return QHostInfo::localHostName();
}

void ProcessInfo::setPid(int aPid)
{
    _pid = aPid;
    _fields |= PROCESS_ID;
}

void ProcessInfo::setUserId(int uid)
{
    _userId = uid;
    _fields |= UID;
}

void ProcessInfo::setUserName(const QString& name)
{
    _userName = name;
    setUserHomeDir();
}

void ProcessInfo::setUserHomeDir()
{
    const QString& usersName = userName();
    if (!usersName.isEmpty())
        _userHomeDir = KUser(usersName).homeDir();
    else
        _userHomeDir = QDir::homePath();
}

void ProcessInfo::setParentPid(int aPid)
{
    _parentPid = aPid;
    _fields |= PARENT_PID;
}
void ProcessInfo::setForegroundPid(int aPid)
{
    _foregroundPid = aPid;
    _fields |= FOREGROUND_PID;
}

QString ProcessInfo::currentDir(bool* ok) const
{
    if (ok)
        *ok = _fields & CURRENT_DIR;

    return _currentDir;
}
void ProcessInfo::setCurrentDir(const QString& dir)
{
    _fields |= CURRENT_DIR;
    _currentDir = dir;
}

void ProcessInfo::setName(const QString& name)
{
    _name = name;
    _fields |= NAME;
}
void ProcessInfo::addArgument(const QString& argument)
{
    _arguments << argument;
}

void ProcessInfo::clearArguments()
{
    _arguments.clear();
}

void ProcessInfo::addEnvironmentBinding(const QString& name , const QString& value)
{
    _environment.insert(name, value);
}

void ProcessInfo::setFileError(QFile::FileError error)
{
    switch (error) {
    case QFile::PermissionsError:
        setError(ProcessInfo::PermissionsError);
        break;
    case QFile::NoError:
        setError(ProcessInfo::NoError);
        break;
    default:
        setError(ProcessInfo::UnknownError);
    }
}

//
// ProcessInfo::newInstance() is way at the bottom so it can see all of the
// implementations of the UnixProcessInfo abstract class.
//

NullProcessInfo::NullProcessInfo(int aPid, bool enableEnvironmentRead)
    : ProcessInfo(aPid, enableEnvironmentRead)
{
}

bool NullProcessInfo::readProcessInfo(int /*pid*/ , bool /*enableEnvironmentRead*/)
{
    return false;
}

void NullProcessInfo::readUserName()
{
}

#if !defined(Q_OS_WIN)
UnixProcessInfo::UnixProcessInfo(int aPid, bool enableEnvironmentRead)
    : ProcessInfo(aPid, enableEnvironmentRead)
{
}

bool UnixProcessInfo::readProcessInfo(int aPid , bool enableEnvironmentRead)
{
    // prevent _arguments from growing longer and longer each time this
    // method is called.
    clearArguments();

    bool ok = readProcInfo(aPid);
    if (ok) {
        ok |= readArguments(aPid);
        ok |= readCurrentDir(aPid);
        if (enableEnvironmentRead) {
            ok |= readEnvironment(aPid);
        }
    }
    return ok;
}

void UnixProcessInfo::readUserName()
{
    bool ok = false;
    const int uid = userId(&ok);
    if (!ok) return;

    struct passwd passwdStruct;
    struct passwd* getpwResult;
    char* getpwBuffer;
    long getpwBufferSize;
    int getpwStatus;

    getpwBufferSize = sysconf(_SC_GETPW_R_SIZE_MAX);
    if (getpwBufferSize == -1)
        getpwBufferSize = 16384;

    getpwBuffer = new char[getpwBufferSize];
    if (getpwBuffer == NULL)
        return;
    getpwStatus = getpwuid_r(uid, &passwdStruct, getpwBuffer, getpwBufferSize, &getpwResult);
    if ((getpwStatus == 0) && (getpwResult != NULL)) {
        setUserName(QString(passwdStruct.pw_name));
    } else {
        setUserName(QString());
        kWarning() << "getpwuid_r returned error : " << getpwStatus;
    }
    delete [] getpwBuffer;
}
#endif

#if defined(Q_OS_LINUX)
class LinuxProcessInfo : public UnixProcessInfo
{
public:
    LinuxProcessInfo(int aPid, bool env) :
        UnixProcessInfo(aPid, env) {
    }

private:
    virtual bool readProcInfo(int aPid) {
        // indicies of various fields within the process status file which
        // contain various information about the process
        const int PARENT_PID_FIELD = 3;
        const int PROCESS_NAME_FIELD = 1;
        const int GROUP_PROCESS_FIELD = 7;

        QString parentPidString;
        QString processNameString;
        QString foregroundPidString;
        QString uidLine;
        QString uidString;
        QStringList uidStrings;

        // For user id read process status file ( /proc/<pid>/status )
        //  Can not use getuid() due to it does not work for 'su'
        QFile statusInfo(QString("/proc/%1/status").arg(aPid));
        if (statusInfo.open(QIODevice::ReadOnly)) {
            QTextStream stream(&statusInfo);
            QString statusLine;
            do {
                statusLine = stream.readLine(0);
                if (statusLine.startsWith(QLatin1String("Uid:")))
                    uidLine = statusLine;
            } while (!statusLine.isNull() && uidLine.isNull());

            uidStrings << uidLine.split('\t', QString::SkipEmptyParts);
            // Must be 5 entries: 'Uid: %d %d %d %d' and
            // uid string must be less than 5 chars (uint)
            if (uidStrings.size() == 5)
                uidString = uidStrings[1];
            if (uidString.size() > 5)
                uidString.clear();

            bool ok = false;
            const int uid = uidString.toInt(&ok);
            if (ok)
                setUserId(uid);
            readUserName();
        } else {
            setFileError(statusInfo.error());
            return false;
        }

        // read process status file ( /proc/<pid/stat )
        //
        // the expected file format is a list of fields separated by spaces, using
        // parenthesies to escape fields such as the process name which may itself contain
        // spaces:
        //
        // FIELD FIELD (FIELD WITH SPACES) FIELD FIELD
        //
        QFile processInfo(QString("/proc/%1/stat").arg(aPid));
        if (processInfo.open(QIODevice::ReadOnly)) {
            QTextStream stream(&processInfo);
            const QString& data = stream.readAll();

            int stack = 0;
            int field = 0;
            int pos = 0;

            while (pos < data.count()) {
                QChar c = data[pos];

                if (c == '(') {
                    stack++;
                } else if (c == ')') {
                    stack--;
                } else if (stack == 0 && c == ' ') {
                    field++;
                } else {
                    switch (field) {
                    case PARENT_PID_FIELD:
                        parentPidString.append(c);
                        break;
                    case PROCESS_NAME_FIELD:
                        processNameString.append(c);
                        break;
                    case GROUP_PROCESS_FIELD:
                        foregroundPidString.append(c);
                        break;
                    }
                }

                pos++;
            }
        } else {
            setFileError(processInfo.error());
            return false;
        }

        // check that data was read successfully
        bool ok = false;
        const int foregroundPid = foregroundPidString.toInt(&ok);
        if (ok)
            setForegroundPid(foregroundPid);

        const int parentPid = parentPidString.toInt(&ok);
        if (ok)
            setParentPid(parentPid);

        if (!processNameString.isEmpty())
            setName(processNameString);

        // update object state
        setPid(aPid);

        return ok;
    }

    virtual bool readArguments(int aPid) {
        // read command-line arguments file found at /proc/<pid>/cmdline
        // the expected format is a list of strings delimited by null characters,
        // and ending in a double null character pair.

        QFile argumentsFile(QString("/proc/%1/cmdline").arg(aPid));
        if (argumentsFile.open(QIODevice::ReadOnly)) {
            QTextStream stream(&argumentsFile);
            const QString& data = stream.readAll();

            const QStringList& argList = data.split(QChar('\0'));

            foreach(const QString & entry , argList) {
                if (!entry.isEmpty())
                    addArgument(entry);
            }
        } else {
            setFileError(argumentsFile.error());
        }

        return true;
    }

    virtual bool readCurrentDir(int aPid) {
        char path_buffer[MAXPATHLEN + 1];
        path_buffer[MAXPATHLEN] = 0;
        QByteArray procCwd = QFile::encodeName(QString("/proc/%1/cwd").arg(aPid));
        const int length = readlink(procCwd.constData(), path_buffer, MAXPATHLEN);
        if (length == -1) {
            setError(UnknownError);
            return false;
        }

        path_buffer[length] = '\0';
        QString path = QFile::decodeName(path_buffer);

        setCurrentDir(path);
        return true;
    }

    virtual bool readEnvironment(int aPid) {
        // read environment bindings file found at /proc/<pid>/environ
        // the expected format is a list of KEY=VALUE strings delimited by null
        // characters and ending in a double null character pair.

        QFile environmentFile(QString("/proc/%1/environ").arg(aPid));
        if (environmentFile.open(QIODevice::ReadOnly)) {
            QTextStream stream(&environmentFile);
            const QString& data = stream.readAll();

            const QStringList& bindingList = data.split(QChar('\0'));

            foreach(const QString & entry , bindingList) {
                QString name;
                QString value;

                const int splitPos = entry.indexOf('=');

                if (splitPos != -1) {
                    name = entry.mid(0, splitPos);
                    value = entry.mid(splitPos + 1, -1);

                    addEnvironmentBinding(name, value);
                }
            }
        } else {
            setFileError(environmentFile.error());
        }

        return true;
    }
};

#elif defined(Q_OS_FREEBSD)
class FreeBSDProcessInfo : public UnixProcessInfo
{
public:
    FreeBSDProcessInfo(int aPid, bool readEnvironment) :
        UnixProcessInfo(aPid, readEnvironment) {
    }

private:
    virtual bool readProcInfo(int aPid) {
        int managementInfoBase[4];
        size_t mibLength;
        struct kinfo_proc* kInfoProc;

        managementInfoBase[0] = CTL_KERN;
        managementInfoBase[1] = KERN_PROC;
        managementInfoBase[2] = KERN_PROC_PID;
        managementInfoBase[3] = aPid;

        if (sysctl(managementInfoBase, 4, NULL, &mibLength, NULL, 0) == -1)
            return false;

        kInfoProc = new struct kinfo_proc [mibLength];

        if (sysctl(managementInfoBase, 4, kInfoProc, &mibLength, NULL, 0) == -1) {
            delete [] kInfoProc;
            return false;
        }

#if defined(HAVE_OS_DRAGONFLYBSD)
        setName(kInfoProc->kp_comm);
        setPid(kInfoProc->kp_pid);
        setParentPid(kInfoProc->kp_ppid);
        setForegroundPid(kInfoProc->kp_pgid);
        setUserId(kInfoProc->kp_uid);
#else
        setName(kInfoProc->ki_comm);
        setPid(kInfoProc->ki_pid);
        setParentPid(kInfoProc->ki_ppid);
        setForegroundPid(kInfoProc->ki_pgid);
        setUserId(kInfoProc->ki_uid);
#endif

        readUserName();

        delete [] kInfoProc;
        return true;
    }

    virtual bool readArguments(int aPid) {
        char args[ARG_MAX];
        int managementInfoBase[4];
        size_t len;

        managementInfoBase[0] = CTL_KERN;
        managementInfoBase[1] = KERN_PROC;
        managementInfoBase[2] = KERN_PROC_PID;
        managementInfoBase[3] = aPid;

        len = sizeof(args);
        if (sysctl(managementInfoBase, 4, args, &len, NULL, 0) == -1)
            return false;

        const QStringList& argumentList = QString(args).split(QChar('\0'));

        for (QStringList::const_iterator it = argumentList.begin(); it != argumentList.end(); ++it) {
            addArgument(*it);
        }

        return true;
    }

    virtual bool readEnvironment(int aPid) {
        Q_UNUSED(aPid);
        // Not supported in FreeBSD?
        return false;
    }

    virtual bool readCurrentDir(int aPid) {
#if defined(HAVE_OS_DRAGONFLYBSD)
        char buf[PATH_MAX];
        int managementInfoBase[4];
        size_t len;

        managementInfoBase[0] = CTL_KERN;
        managementInfoBase[1] = KERN_PROC;
        managementInfoBase[2] = KERN_PROC_CWD;
        managementInfoBase[3] = aPid;

        len = sizeof(buf);
        if (sysctl(managementInfoBase, 4, buf, &len, NULL, 0) == -1)
            return false;

        setCurrentDir(buf);

        return true;
#else
        int numrecords;
        struct kinfo_file* info = 0;

        info = kinfo_getfile(aPid, &numrecords);

        if (!info)
            return false;

        for (int i = 0; i < numrecords; ++i) {
            if (info[i].kf_fd == KF_FD_TYPE_CWD) {
                setCurrentDir(info[i].kf_path);

                free(info);
                return true;
            }
        }

        free(info);
        return false;
#endif
    }
};

#elif defined(Q_OS_OPENBSD)
class OpenBSDProcessInfo : public UnixProcessInfo
{
public:
    OpenBSDProcessInfo(int aPid, bool readEnvironment) :
        UnixProcessInfo(aPid, readEnvironment) {
    }

private:
    virtual bool readProcInfo(int aPid) {
        int      managementInfoBase[6];
        size_t   mibLength;
        struct kinfo_proc*  kInfoProc;

        managementInfoBase[0] = CTL_KERN;
        managementInfoBase[1] = KERN_PROC;
        managementInfoBase[2] = KERN_PROC_PID;
        managementInfoBase[3] = aPid;
        managementInfoBase[4] = sizeof(struct kinfo_proc);
        managementInfoBase[5] = 1;

        if (::sysctl(managementInfoBase, 6, NULL, &mibLength, NULL, 0) == -1) {
            kWarning() << "first sysctl() call failed with code" << errno;
            return false;
        }

        kInfoProc = (struct kinfo_proc*)malloc(mibLength);

        if (::sysctl(managementInfoBase, 6, kInfoProc, &mibLength, NULL, 0) == -1) {
            kWarning() << "second sysctl() call failed with code" << errno;
            free(kInfoProc);
            return false;
        }

        setName(kInfoProc->p_comm);
        setPid(kInfoProc->p_pid);
        setParentPid(kInfoProc->p_ppid);
        setForegroundPid(kInfoProc->p_tpgid);
        setUserId(kInfoProc->p_uid);
        setUserName(kInfoProc->p_login);

        free(kInfoProc);
        return true;
    }

    char** readProcArgs(int aPid, int whatMib) {
        void*   buf = NULL;
        void*   nbuf;
        size_t  len = 4096;
        int     rc = -1;
        int     managementInfoBase[4];

        managementInfoBase[0] = CTL_KERN;
        managementInfoBase[1] = KERN_PROC_ARGS;
        managementInfoBase[2] = aPid;
        managementInfoBase[3] = whatMib;

        do {
            len *= 2;
            nbuf = realloc(buf, len);
            if (nbuf == NULL) {
                break;
            }

            buf = nbuf;
            rc = ::sysctl(managementInfoBase, 4, buf, &len, NULL, 0);
            kWarning() << "sysctl() call failed with code" << errno;
        } while (rc == -1 && errno == ENOMEM);

        if (nbuf == NULL || rc == -1) {
            free(buf);
            return NULL;
        }

        return (char**)buf;
    }

    virtual bool readArguments(int aPid) {
        char**  argv;

        argv = readProcArgs(aPid, KERN_PROC_ARGV);
        if (argv == NULL) {
            return false;
        }

        for (char** p = argv; *p != NULL; p++) {
            addArgument(QString(*p));
        }
        free(argv);
        return true;
    }

    virtual bool readEnvironment(int aPid) {
        char**  envp;
        char*   eqsign;

        envp = readProcArgs(aPid, KERN_PROC_ENV);
        if (envp == NULL) {
            return false;
        }

        for (char **p = envp; *p != NULL; p++) {
            eqsign = strchr(*p, '=');
            if (eqsign == NULL || eqsign[1] == '\0') {
                continue;
            }
            *eqsign = '\0';
            addEnvironmentBinding(QString((const char *)p),
                QString((const char *)eqsign + 1));
        }
        free(envp);
        return true;
    }

    virtual bool readCurrentDir(int aPid) {
        char    buf[PATH_MAX];
        int     managementInfoBase[3];
        size_t  len;

        managementInfoBase[0] = CTL_KERN;
        managementInfoBase[1] = KERN_PROC_CWD;
        managementInfoBase[2] = aPid;

        len = sizeof(buf);
        if (::sysctl(managementInfoBase, 3, buf, &len, NULL, 0) == -1) {
            kWarning() << "sysctl() call failed with code" << errno;
            return false;
        }

        setCurrentDir(buf);
        return true;
    }
};

#elif defined(Q_OS_MAC)
class MacProcessInfo : public UnixProcessInfo
{
public:
    MacProcessInfo(int aPid, bool env) :
        UnixProcessInfo(aPid, env) {
    }

private:
    virtual bool readProcInfo(int aPid) {
        int managementInfoBase[4];
        size_t mibLength;
        struct kinfo_proc* kInfoProc;
        KDE_struct_stat statInfo;

        // Find the tty device of 'pid' (Example: /dev/ttys001)
        managementInfoBase[0] = CTL_KERN;
        managementInfoBase[1] = KERN_PROC;
        managementInfoBase[2] = KERN_PROC_PID;
        managementInfoBase[3] = aPid;

        if (sysctl(managementInfoBase, 4, NULL, &mibLength, NULL, 0) == -1) {
            return false;
        } else {
            kInfoProc = new struct kinfo_proc [mibLength];
            if (sysctl(managementInfoBase, 4, kInfoProc, &mibLength, NULL, 0) == -1) {
                delete [] kInfoProc;
                return false;
            } else {
                const QString deviceNumber = QString(devname(((&kInfoProc->kp_eproc)->e_tdev), S_IFCHR));
                const QString fullDeviceName =  QString("/dev/") + deviceNumber.rightJustified(3, '0');
                delete [] kInfoProc;

                const QByteArray deviceName = fullDeviceName.toLatin1();
                const char* ttyName = deviceName.data();

                if (KDE::stat(ttyName, &statInfo) != 0)
                    return false;

                // Find all processes attached to ttyName
                managementInfoBase[0] = CTL_KERN;
                managementInfoBase[1] = KERN_PROC;
                managementInfoBase[2] = KERN_PROC_TTY;
                managementInfoBase[3] = statInfo.st_rdev;

                mibLength = 0;
                if (sysctl(managementInfoBase, sizeof(managementInfoBase) / sizeof(int), NULL, &mibLength, NULL, 0) == -1)
                    return false;

                kInfoProc = new struct kinfo_proc [mibLength];
                if (sysctl(managementInfoBase, sizeof(managementInfoBase) / sizeof(int), kInfoProc, &mibLength, NULL, 0) == -1)
                    return false;

                // The foreground program is the first one
                setName(QString(kInfoProc->kp_proc.p_comm));

                delete [] kInfoProc;
            }
            setPid(aPid);
        }
        return true;
    }

    virtual bool readArguments(int aPid) {
        Q_UNUSED(aPid);
        return false;
    }
    virtual bool readCurrentDir(int aPid) {
        struct proc_vnodepathinfo vpi;
        const int nb = proc_pidinfo(aPid, PROC_PIDVNODEPATHINFO, 0, &vpi, sizeof(vpi));
        if (nb == sizeof(vpi)) {
            setCurrentDir(QString(vpi.pvi_cdir.vip_path));
            return true;
        }
        return false;
    }
    virtual bool readEnvironment(int aPid) {
        Q_UNUSED(aPid);
        return false;
    }
};

#elif defined(Q_OS_SOLARIS)
// The procfs structure definition requires off_t to be
// 32 bits, which only applies if FILE_OFFSET_BITS=32.
// Futz around here to get it to compile regardless,
// although some of the structure sizes might be wrong.
// Fortunately, the structures we actually use don't use
// off_t, and we're safe.
#if defined(_FILE_OFFSET_BITS) && (_FILE_OFFSET_BITS==64)
#undef _FILE_OFFSET_BITS
#endif
#include <procfs.h>

class SolarisProcessInfo : public UnixProcessInfo
{
public:
    SolarisProcessInfo(int aPid, bool readEnvironment)
        : UnixProcessInfo(aPid, readEnvironment) {
    }
private:
    virtual bool readProcInfo(int aPid) {
        QFile psinfo(QString("/proc/%1/psinfo").arg(aPid));
        if (psinfo.open(QIODevice::ReadOnly)) {
            struct psinfo info;
            if (psinfo.read((char *)&info, sizeof(info)) != sizeof(info)) {
                return false;
            }

            setParentPid(info.pr_ppid);
            setForegroundPid(info.pr_pgid);
            setName(info.pr_fname);
            setPid(aPid);

            // Bogus, because we're treating the arguments as one single string
            info.pr_psargs[PRARGSZ - 1] = 0;
            addArgument(info.pr_psargs);
        }
        return true;
    }

    virtual bool readArguments(int /*pid*/) {
        // Handled in readProcInfo()
        return false;
    }

    virtual bool readEnvironment(int /*pid*/) {
        // Not supported in Solaris
        return false;
    }

    // FIXME: This will have the same issues as BKO 251351; the Linux
    // version uses readlink.
    virtual bool readCurrentDir(int aPid) {
        QFileInfo info(QString("/proc/%1/path/cwd").arg(aPid));
        const bool readable = info.isReadable();

        if (readable && info.isSymLink()) {
            setCurrentDir(info.symLinkTarget());
            return true;
        } else {
            if (!readable)
                setError(PermissionsError);
            else
                setError(UnknownError);

            return false;
        }
    }
};
#endif

SSHProcessInfo::SSHProcessInfo(const ProcessInfo& process)
    : _process(process)
{
    bool ok = false;

    // check that this is a SSH process
    const QString& name = _process.name(&ok);

    if (!ok || name != "ssh") {
        if (!ok)
            kWarning() << "Could not read process info";
        else
            kWarning() << "Process is not a SSH process";

        return;
    }

    // read arguments
    const QVector<QString>& args = _process.arguments(&ok);

    // SSH options
    // these are taken from the SSH manual ( accessed via 'man ssh' )

    // options which take no arguments
    static const QString noArgumentOptions("1246AaCfgKkMNnqsTtVvXxYy");
    // options which take one argument
    static const QString singleArgumentOptions("bcDeFIiLlmOopRSWw");

    if (ok) {
        // find the username, host and command arguments
        //
        // the username/host is assumed to be the first argument
        // which is not an option
        // ( ie. does not start with a dash '-' character )
        // or an argument to a previous option.
        //
        // the command, if specified, is assumed to be the argument following
        // the username and host
        //
        // note that we skip the argument at index 0 because that is the
        // program name ( expected to be 'ssh' in this case )
        for (int i = 1 ; i < args.count() ; i++) {
            // If this one is an option ...
            // Most options together with its argument will be skipped.
            if (args[i].startsWith('-')) {
                const QChar optionChar = (args[i].length() > 1) ? args[i][1] : '\0';
                // for example: -p2222 or -p 2222 ?
                const bool optionArgumentCombined =  args[i].length() > 2;

                if (noArgumentOptions.contains(optionChar)) {
                    continue;
                } else if (singleArgumentOptions.contains(optionChar)) {
                    QString argument;
                    if (optionArgumentCombined) {
                        argument = args[i].mid(2);
                    } else {
                        // Verify correct # arguments are given
                        if ((i + 1) < args.count()) {
                            argument = args[i + 1];
                        }
                        i++;
                    }

                    // support using `-l user` to specify username.
                    if (optionChar == 'l')
                        _user = argument;
                    // support using `-p port` to specify port.
                    else if (optionChar == 'p')
                        _port = argument;

                    continue;
                }
            }

            // check whether the host has been found yet
            // if not, this must be the username/host argument
            if (_host.isEmpty()) {
                // check to see if only a hostname is specified, or whether
                // both a username and host are specified ( in which case they
                // are separated by an '@' character:  username@host )

                const int separatorPosition = args[i].indexOf('@');
                if (separatorPosition != -1) {
                    // username and host specified
                    _user = args[i].left(separatorPosition);
                    _host = args[i].mid(separatorPosition + 1);
                } else {
                    // just the host specified
                    _host = args[i];
                }
            } else {
                // host has already been found, this must be the command argument
                _command = args[i];
            }
        }
    } else {
        kWarning() << "Could not read arguments";

        return;
    }
}

QString SSHProcessInfo::userName() const
{
    return _user;
}
QString SSHProcessInfo::host() const
{
    return _host;
}
QString SSHProcessInfo::port() const
{
    return _port;
}
QString SSHProcessInfo::command() const
{
    return _command;
}
QString SSHProcessInfo::format(const QString& input) const
{
    QString output(input);

    // test whether host is an ip address
    // in which case 'short host' and 'full host'
    // markers in the input string are replaced with
    // the full address
    bool isIpAddress = false;

    struct in_addr address;
    if (inet_aton(_host.toLocal8Bit().constData(), &address) != 0)
        isIpAddress = true;
    else
        isIpAddress = false;

    // search for and replace known markers
    output.replace("%u", _user);

    if (isIpAddress)
        output.replace("%h", _host);
    else
        output.replace("%h", _host.left(_host.indexOf('.')));

    output.replace("%H", _host);
    output.replace("%c", _command);

    return output;
}

ProcessInfo* ProcessInfo::newInstance(int aPid, bool enableEnvironmentRead)
{
#if defined(Q_OS_LINUX)
    return new LinuxProcessInfo(aPid, enableEnvironmentRead);
#elif defined(Q_OS_SOLARIS)
    return new SolarisProcessInfo(aPid, enableEnvironmentRead);
#elif defined(Q_OS_MAC)
    return new MacProcessInfo(aPid, enableEnvironmentRead);
#elif defined(Q_OS_FREEBSD)
    return new FreeBSDProcessInfo(aPid, enableEnvironmentRead);
#elif defined(Q_OS_OPENBSD)
    return new OpenBSDProcessInfo(aPid, enableEnvironmentRead);
#else
    return new NullProcessInfo(aPid, enableEnvironmentRead);
#endif
}

