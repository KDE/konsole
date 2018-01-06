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
#include <errno.h>

// Qt
#include <QDir>
#include <QFileInfo>
#include <QFlags>
#include <QTextStream>
#include <QStringList>
#include <QHostInfo>

// KDE
#include <KConfigGroup>
#include <KSharedConfig>
#include <KUser>

#if defined(Q_OS_FREEBSD) || defined(Q_OS_OPENBSD) || defined(Q_OS_MACOS)
#include <sys/sysctl.h>
#endif

#if defined(Q_OS_MACOS)
#include <libproc.h>
#include <qplatformdefs.h>
#endif

#if defined(Q_OS_FREEBSD) || defined(Q_OS_OPENBSD)
#include <sys/types.h>
#include <sys/user.h>
#include <sys/syslimits.h>
#   if defined(Q_OS_FREEBSD)
#   include <libutil.h>
#   include <sys/param.h>
#   include <sys/queue.h>
#   endif
#endif

using namespace Konsole;

ProcessInfo::ProcessInfo(int pid) :
    _fields(ARGUMENTS)     // arguments
    // are currently always valid,
    // they just return an empty
    // vector / map respectively
    // if no arguments
    // have been explicitly set
    ,
    _pid(pid),
    _parentPid(0),
    _foregroundPid(0),
    _userId(0),
    _lastError(NoError),
    _name(QString()),
    _userName(QString()),
    _userHomeDir(QString()),
    _currentDir(QString()),
    _userNameRequired(true),
    _arguments(QVector<QString>())
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
    readCurrentDir(_pid);
}

QString ProcessInfo::validCurrentDir() const
{
    bool ok = false;

    // read current dir, if an error occurs try the parent as the next
    // best option
    int currentPid = parentPid(&ok);
    QString dir = currentDir(&ok);
    while (!ok && currentPid != 0) {
        ProcessInfo *current = ProcessInfo::newInstance(currentPid, QString());
        current->update();
        currentPid = current->parentPid(&ok);
        dir = current->currentDir(&ok);
        delete current;
    }

    return dir;
}

QSet<QString> ProcessInfo::_commonDirNames;

QSet<QString> ProcessInfo::commonDirNames()
{
    static bool forTheFirstTime = true;

    if (forTheFirstTime) {
        const KSharedConfigPtr &config = KSharedConfig::openConfig();
        const KConfigGroup &configGroup = config->group("ProcessInfo");
        _commonDirNames = QSet<QString>::fromList(configGroup.readEntry("CommonDirNames", QStringList()));

        forTheFirstTime = false;
    }

    return _commonDirNames;
}

QString ProcessInfo::formatShortDir(const QString &input) const
{
    QString result;

    const QStringList &parts = input.split(QDir::separator());

    QSet<QString> dirNamesToShorten = commonDirNames();

    QListIterator<QString> iter(parts);
    iter.toBack();

    // go backwards through the list of the path's parts
    // adding abbreviations of common directory names
    // and stopping when we reach a dir name which is not
    // in the commonDirNames set
    while (iter.hasPrevious()) {
        const QString &part = iter.previous();

        if (dirNamesToShorten.contains(part)) {
            result.prepend(QDir::separator() + part[0]);
        } else {
            result.prepend(part);
            break;
        }
    }

    return result;
}

QVector<QString> ProcessInfo::arguments(bool *ok) const
{
    *ok = _fields.testFlag(ARGUMENTS);

    return _arguments;
}

bool ProcessInfo::isValid() const
{
    return _fields.testFlag(PROCESS_ID);
}

int ProcessInfo::pid(bool *ok) const
{
    *ok = _fields.testFlag(PROCESS_ID);

    return _pid;
}

int ProcessInfo::parentPid(bool *ok) const
{
    *ok = _fields.testFlag(PARENT_PID);

    return _parentPid;
}

int ProcessInfo::foregroundPid(bool *ok) const
{
    *ok = _fields.testFlag(FOREGROUND_PID);

    return _foregroundPid;
}

QString ProcessInfo::name(bool *ok) const
{
    *ok = _fields.testFlag(NAME);

    return _name;
}

int ProcessInfo::userId(bool *ok) const
{
    *ok = _fields.testFlag(UID);

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

void ProcessInfo::setPid(int pid)
{
    _pid = pid;
    _fields |= PROCESS_ID;
}

void ProcessInfo::setUserId(int uid)
{
    _userId = uid;
    _fields |= UID;
}

void ProcessInfo::setUserName(const QString &name)
{
    _userName = name;
    setUserHomeDir();
}

void ProcessInfo::setUserHomeDir()
{
    const QString &usersName = userName();
    if (!usersName.isEmpty()) {
        _userHomeDir = KUser(usersName).homeDir();
    } else {
        _userHomeDir = QDir::homePath();
    }
}

void ProcessInfo::setParentPid(int pid)
{
    _parentPid = pid;
    _fields |= PARENT_PID;
}

void ProcessInfo::setForegroundPid(int pid)
{
    _foregroundPid = pid;
    _fields |= FOREGROUND_PID;
}

void ProcessInfo::setUserNameRequired(bool need)
{
    _userNameRequired = need;
}

bool ProcessInfo::userNameRequired() const
{
    return _userNameRequired;
}

QString ProcessInfo::currentDir(bool *ok) const
{
    if (ok != nullptr) {
        *ok = (_fields & CURRENT_DIR) != 0;
    }

    return _currentDir;
}

void ProcessInfo::setCurrentDir(const QString &dir)
{
    _fields |= CURRENT_DIR;
    _currentDir = dir;
}

void ProcessInfo::setName(const QString &name)
{
    _name = name;
    _fields |= NAME;
}

void ProcessInfo::addArgument(const QString &argument)
{
    _arguments << argument;
}

void ProcessInfo::clearArguments()
{
    _arguments.clear();
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

NullProcessInfo::NullProcessInfo(int pid, const QString & /*titleFormat*/) :
    ProcessInfo(pid)
{
}

void NullProcessInfo::readProcessInfo(int /*pid*/)
{
}

bool NullProcessInfo::readCurrentDir(int /*pid*/)
{
    return false;
}

void NullProcessInfo::readUserName()
{
}

#if !defined(Q_OS_WIN)
UnixProcessInfo::UnixProcessInfo(int pid, const QString &titleFormat) :
    ProcessInfo(pid)
{
    setUserNameRequired(titleFormat.contains(QLatin1String("%u")));
}

void UnixProcessInfo::readProcessInfo(int pid)
{
    // prevent _arguments from growing longer and longer each time this
    // method is called.
    clearArguments();

    if (readProcInfo(pid)) {
        readArguments(pid);
        readCurrentDir(pid);
    }
}

void UnixProcessInfo::readUserName()
{
    bool ok = false;
    const int uid = userId(&ok);
    if (!ok) {
        return;
    }

    struct passwd passwdStruct;
    struct passwd *getpwResult;
    char *getpwBuffer;
    long getpwBufferSize;
    int getpwStatus;

    getpwBufferSize = sysconf(_SC_GETPW_R_SIZE_MAX);
    if (getpwBufferSize == -1) {
        getpwBufferSize = 16384;
    }

    getpwBuffer = new char[getpwBufferSize];
    if (getpwBuffer == nullptr) {
        return;
    }
    getpwStatus = getpwuid_r(uid, &passwdStruct, getpwBuffer, getpwBufferSize, &getpwResult);
    if ((getpwStatus == 0) && (getpwResult != nullptr)) {
        setUserName(QLatin1String(passwdStruct.pw_name));
    } else {
        setUserName(QString());
        qWarning() << "getpwuid_r returned error : " << getpwStatus;
    }
    delete [] getpwBuffer;
}

#endif

#if defined(Q_OS_LINUX)
class LinuxProcessInfo : public UnixProcessInfo
{
public:
    LinuxProcessInfo(int pid, const QString &titleFormat) :
        UnixProcessInfo(pid, titleFormat)
    {
    }

protected:
    bool readCurrentDir(int pid) Q_DECL_OVERRIDE
    {
        char path_buffer[MAXPATHLEN + 1];
        path_buffer[MAXPATHLEN] = 0;
        QByteArray procCwd = QFile::encodeName(QStringLiteral("/proc/%1/cwd").arg(pid));
        const int length = static_cast<int>(readlink(procCwd.constData(), path_buffer, MAXPATHLEN));
        if (length == -1) {
            setError(UnknownError);
            return false;
        }

        path_buffer[length] = '\0';
        QString path = QFile::decodeName(path_buffer);

        setCurrentDir(path);
        return true;
    }

private:
    bool readProcInfo(int pid) Q_DECL_OVERRIDE
    {
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
        QFile statusInfo(QStringLiteral("/proc/%1/status").arg(pid));
        if (statusInfo.open(QIODevice::ReadOnly)) {
            QTextStream stream(&statusInfo);
            QString statusLine;
            do {
                statusLine = stream.readLine();
                if (statusLine.startsWith(QLatin1String("Uid:"))) {
                    uidLine = statusLine;
                }
            } while (!statusLine.isNull() && uidLine.isNull());

            uidStrings << uidLine.split(QLatin1Char('\t'), QString::SkipEmptyParts);
            // Must be 5 entries: 'Uid: %d %d %d %d' and
            // uid string must be less than 5 chars (uint)
            if (uidStrings.size() == 5) {
                uidString = uidStrings[1];
            }
            if (uidString.size() > 5) {
                uidString.clear();
            }

            bool ok = false;
            const int uid = uidString.toInt(&ok);
            if (ok) {
                setUserId(uid);
            }
            // This will cause constant opening of /etc/passwd
            if (userNameRequired()) {
                readUserName();
            }
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
        QFile processInfo(QStringLiteral("/proc/%1/stat").arg(pid));
        if (processInfo.open(QIODevice::ReadOnly)) {
            QTextStream stream(&processInfo);
            const QString &data = stream.readAll();

            int stack = 0;
            int field = 0;
            int pos = 0;

            while (pos < data.count()) {
                QChar c = data[pos];

                if (c == QLatin1Char('(')) {
                    stack++;
                } else if (c == QLatin1Char(')')) {
                    stack--;
                } else if (stack == 0 && c == QLatin1Char(' ')) {
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
        if (ok) {
            setForegroundPid(foregroundPid);
        }

        const int parentPid = parentPidString.toInt(&ok);
        if (ok) {
            setParentPid(parentPid);
        }

        if (!processNameString.isEmpty()) {
            setName(processNameString);
        }

        // update object state
        setPid(pid);

        return ok;
    }

    bool readArguments(int pid) Q_DECL_OVERRIDE
    {
        // read command-line arguments file found at /proc/<pid>/cmdline
        // the expected format is a list of strings delimited by null characters,
        // and ending in a double null character pair.

        QFile argumentsFile(QStringLiteral("/proc/%1/cmdline").arg(pid));
        if (argumentsFile.open(QIODevice::ReadOnly)) {
            QTextStream stream(&argumentsFile);
            const QString &data = stream.readAll();

            const QStringList &argList = data.split(QLatin1Char('\0'));

            foreach (const QString &entry, argList) {
                if (!entry.isEmpty()) {
                    addArgument(entry);
                }
            }
        } else {
            setFileError(argumentsFile.error());
        }

        return true;
    }
};

#elif defined(Q_OS_FREEBSD)
class FreeBSDProcessInfo : public UnixProcessInfo
{
public:
    FreeBSDProcessInfo(int pid, const QString &titleFormat) :
        UnixProcessInfo(pid, titleFormat)
    {
    }

protected:
    bool readCurrentDir(int pid) Q_DECL_OVERRIDE
    {
#if defined(HAVE_OS_DRAGONFLYBSD)
        char buf[PATH_MAX];
        int managementInfoBase[4];
        size_t len;

        managementInfoBase[0] = CTL_KERN;
        managementInfoBase[1] = KERN_PROC;
        managementInfoBase[2] = KERN_PROC_CWD;
        managementInfoBase[3] = pid;

        len = sizeof(buf);
        if (sysctl(managementInfoBase, 4, buf, &len, NULL, 0) == -1) {
            return false;
        }

        setCurrentDir(QString::fromUtf8(buf));

        return true;
#else
        int numrecords;
        struct kinfo_file *info = 0;

        info = kinfo_getfile(pid, &numrecords);

        if (!info) {
            return false;
        }

        for (int i = 0; i < numrecords; ++i) {
            if (info[i].kf_fd == KF_FD_TYPE_CWD) {
                setCurrentDir(QString::fromUtf8(info[i].kf_path));

                free(info);
                return true;
            }
        }

        free(info);
        return false;
#endif
    }

private:
    bool readProcInfo(int pid) Q_DECL_OVERRIDE
    {
        int managementInfoBase[4];
        size_t mibLength;
        struct kinfo_proc *kInfoProc;

        managementInfoBase[0] = CTL_KERN;
        managementInfoBase[1] = KERN_PROC;
        managementInfoBase[2] = KERN_PROC_PID;
        managementInfoBase[3] = pid;

        if (sysctl(managementInfoBase, 4, NULL, &mibLength, NULL, 0) == -1) {
            return false;
        }

        kInfoProc = new struct kinfo_proc [mibLength];

        if (sysctl(managementInfoBase, 4, kInfoProc, &mibLength, NULL, 0) == -1) {
            delete [] kInfoProc;
            return false;
        }

#if defined(HAVE_OS_DRAGONFLYBSD)
        setName(QString::fromUtf8(kInfoProc->kp_comm));
        setPid(kInfoProc->kp_pid);
        setParentPid(kInfoProc->kp_ppid);
        setForegroundPid(kInfoProc->kp_pgid);
        setUserId(kInfoProc->kp_uid);
#else
        setName(QString::fromUtf8(kInfoProc->ki_comm));
        setPid(kInfoProc->ki_pid);
        setParentPid(kInfoProc->ki_ppid);
        setForegroundPid(kInfoProc->ki_pgid);
        setUserId(kInfoProc->ki_uid);
#endif

        readUserName();

        delete [] kInfoProc;
        return true;
    }

    bool readArguments(int pid) Q_DECL_OVERRIDE
    {
        char args[ARG_MAX];
        int managementInfoBase[4];
        size_t len;

        managementInfoBase[0] = CTL_KERN;
        managementInfoBase[1] = KERN_PROC;
        managementInfoBase[2] = KERN_PROC_ARGS;
        managementInfoBase[3] = pid;

        len = sizeof(args);
        if (sysctl(managementInfoBase, 4, args, &len, NULL, 0) == -1) {
            return false;
        }

        // len holds the length of the string
        QString qargs = QString::fromLocal8Bit(args, len);
        foreach (const QString &value, qargs.split(QLatin1Char('\u0000'))) {
            if (!value.isEmpty()) {
                addArgument(value);
            }
        }

        return true;
    }
};

#elif defined(Q_OS_OPENBSD)
class OpenBSDProcessInfo : public UnixProcessInfo
{
public:
    OpenBSDProcessInfo(int pid, const QString &titleFormat) :
        UnixProcessInfo(pid, titleFormat)
    {
    }

protected:
    bool readCurrentDir(int pid) Q_DECL_OVERRIDE
    {
        char buf[PATH_MAX];
        int managementInfoBase[3];
        size_t len;

        managementInfoBase[0] = CTL_KERN;
        managementInfoBase[1] = KERN_PROC_CWD;
        managementInfoBase[2] = pid;

        len = sizeof(buf);
        if (::sysctl(managementInfoBase, 3, buf, &len, NULL, 0) == -1) {
            qWarning() << "sysctl() call failed with code" << errno;
            return false;
        }

        setCurrentDir(QString::fromUtf8(buf));
        return true;
    }

private:
    bool readProcInfo(int pid) Q_DECL_OVERRIDE
    {
        int managementInfoBase[6];
        size_t mibLength;
        struct kinfo_proc *kInfoProc;

        managementInfoBase[0] = CTL_KERN;
        managementInfoBase[1] = KERN_PROC;
        managementInfoBase[2] = KERN_PROC_PID;
        managementInfoBase[3] = pid;
        managementInfoBase[4] = sizeof(struct kinfo_proc);
        managementInfoBase[5] = 1;

        if (::sysctl(managementInfoBase, 6, NULL, &mibLength, NULL, 0) == -1) {
            qWarning() << "first sysctl() call failed with code" << errno;
            return false;
        }

        kInfoProc = (struct kinfo_proc *)malloc(mibLength);

        if (::sysctl(managementInfoBase, 6, kInfoProc, &mibLength, NULL, 0) == -1) {
            qWarning() << "second sysctl() call failed with code" << errno;
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

    char **readProcArgs(int pid, int whatMib)
    {
        void *buf = NULL;
        void *nbuf;
        size_t len = 4096;
        int rc = -1;
        int managementInfoBase[4];

        managementInfoBase[0] = CTL_KERN;
        managementInfoBase[1] = KERN_PROC_ARGS;
        managementInfoBase[2] = pid;
        managementInfoBase[3] = whatMib;

        do {
            len *= 2;
            nbuf = realloc(buf, len);
            if (nbuf == NULL) {
                break;
            }

            buf = nbuf;
            rc = ::sysctl(managementInfoBase, 4, buf, &len, NULL, 0);
            qWarning() << "sysctl() call failed with code" << errno;
        } while (rc == -1 && errno == ENOMEM);

        if (nbuf == NULL || rc == -1) {
            free(buf);
            return NULL;
        }

        return (char **)buf;
    }

    bool readArguments(int pid) Q_DECL_OVERRIDE
    {
        char **argv;

        argv = readProcArgs(pid, KERN_PROC_ARGV);
        if (argv == NULL) {
            return false;
        }

        for (char **p = argv; *p != NULL; p++) {
            addArgument(QString(*p));
        }
        free(argv);
        return true;
    }
};

#elif defined(Q_OS_MACOS)
class MacProcessInfo : public UnixProcessInfo
{
public:
    MacProcessInfo(int pid, const QString &titleFormat) :
        UnixProcessInfo(pid, titleFormat)
    {
    }

protected:
    bool readCurrentDir(int pid) Q_DECL_OVERRIDE
    {
        struct proc_vnodepathinfo vpi;
        const int nb = proc_pidinfo(pid, PROC_PIDVNODEPATHINFO, 0, &vpi, sizeof(vpi));
        if (nb == sizeof(vpi)) {
            setCurrentDir(QString::fromUtf8(vpi.pvi_cdir.vip_path));
            return true;
        }
        return false;
    }

private:
    bool readProcInfo(int pid) Q_DECL_OVERRIDE
    {
        int managementInfoBase[4];
        size_t mibLength;
        struct kinfo_proc *kInfoProc;
        QT_STATBUF statInfo;

        // Find the tty device of 'pid' (Example: /dev/ttys001)
        managementInfoBase[0] = CTL_KERN;
        managementInfoBase[1] = KERN_PROC;
        managementInfoBase[2] = KERN_PROC_PID;
        managementInfoBase[3] = pid;

        if (sysctl(managementInfoBase, 4, NULL, &mibLength, NULL, 0) == -1) {
            return false;
        } else {
            kInfoProc = new struct kinfo_proc [mibLength];
            if (sysctl(managementInfoBase, 4, kInfoProc, &mibLength, NULL, 0) == -1) {
                delete [] kInfoProc;
                return false;
            } else {
                const QString deviceNumber = QString::fromUtf8(devname(((&kInfoProc->kp_eproc)->e_tdev), S_IFCHR));
                const QString fullDeviceName = QStringLiteral("/dev/")
                                               + deviceNumber.rightJustified(3, QLatin1Char('0'));
                delete [] kInfoProc;

                const QByteArray deviceName = fullDeviceName.toLatin1();
                const char *ttyName = deviceName.data();

                if (QT_STAT(ttyName, &statInfo) != 0) {
                    return false;
                }

                // Find all processes attached to ttyName
                managementInfoBase[0] = CTL_KERN;
                managementInfoBase[1] = KERN_PROC;
                managementInfoBase[2] = KERN_PROC_TTY;
                managementInfoBase[3] = statInfo.st_rdev;

                mibLength = 0;
                if (sysctl(managementInfoBase, sizeof(managementInfoBase) / sizeof(int), NULL,
                           &mibLength, NULL, 0) == -1) {
                    return false;
                }

                kInfoProc = new struct kinfo_proc [mibLength];
                if (sysctl(managementInfoBase, sizeof(managementInfoBase) / sizeof(int), kInfoProc,
                           &mibLength, NULL, 0) == -1) {
                    return false;
                }

                // The foreground program is the first one
                setName(QString::fromUtf8(kInfoProc->kp_proc.p_comm));

                delete [] kInfoProc;
            }
            setPid(pid);
        }
        return true;
    }

    bool readArguments(int pid) Q_DECL_OVERRIDE
    {
        Q_UNUSED(pid);
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
#if defined(_FILE_OFFSET_BITS) && (_FILE_OFFSET_BITS == 64)
#undef _FILE_OFFSET_BITS
#endif
#include <procfs.h>

class SolarisProcessInfo : public UnixProcessInfo
{
public:
    SolarisProcessInfo(int pid, const QString &titleFormat) :
        UnixProcessInfo(pid, titleFormat)
    {
    }

protected:
    // FIXME: This will have the same issues as BKO 251351; the Linux
    // version uses readlink.
    bool readCurrentDir(int pid) Q_DECL_OVERRIDE
    {
        QFileInfo info(QString("/proc/%1/path/cwd").arg(pid));
        const bool readable = info.isReadable();

        if (readable && info.isSymLink()) {
            setCurrentDir(info.symLinkTarget());
            return true;
        } else {
            if (!readable) {
                setError(PermissionsError);
            } else {
                setError(UnknownError);
            }

            return false;
        }
    }

private:
    bool readProcInfo(int pid) Q_DECL_OVERRIDE
    {
        QFile psinfo(QString("/proc/%1/psinfo").arg(pid));
        if (psinfo.open(QIODevice::ReadOnly)) {
            struct psinfo info;
            if (psinfo.read((char *)&info, sizeof(info)) != sizeof(info)) {
                return false;
            }

            setParentPid(info.pr_ppid);
            setForegroundPid(info.pr_pgid);
            setName(info.pr_fname);
            setPid(pid);

            // Bogus, because we're treating the arguments as one single string
            info.pr_psargs[PRARGSZ - 1] = 0;
            addArgument(info.pr_psargs);
        }
        return true;
    }

    bool readArguments(int /*pid*/) Q_DECL_OVERRIDE
    {
        // Handled in readProcInfo()
        return false;
    }
};
#endif

SSHProcessInfo::SSHProcessInfo(const ProcessInfo &process) :
    _process(process),
    _user(QString()),
    _host(QString()),
    _port(QString()),
    _command(QString())
{
    bool ok = false;

    // check that this is a SSH process
    const QString &name = _process.name(&ok);

    if (!ok || name != QLatin1String("ssh")) {
        if (!ok) {
            qWarning() << "Could not read process info";
        } else {
            qWarning() << "Process is not a SSH process";
        }

        return;
    }

    // read arguments
    const QVector<QString> &args = _process.arguments(&ok);

    // SSH options
    // these are taken from the SSH manual ( accessed via 'man ssh' )

    // options which take no arguments
    static const QString noArgumentOptions(QStringLiteral("1246AaCfgKkMNnqsTtVvXxYy"));
    // options which take one argument
    static const QString singleArgumentOptions(QStringLiteral("bcDeFIiLlmOopRSWw"));

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
        for (int i = 1; i < args.count(); i++) {
            // If this one is an option ...
            // Most options together with its argument will be skipped.
            if (args[i].startsWith(QLatin1Char('-'))) {
                const QChar optionChar = (args[i].length() > 1) ? args[i][1] : QLatin1Char('\0');
                // for example: -p2222 or -p 2222 ?
                const bool optionArgumentCombined = args[i].length() > 2;

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
                    if (optionChar == QLatin1Char('l')) {
                        _user = argument;
                    }
                    // support using `-p port` to specify port.
                    else if (optionChar == QLatin1Char('p')) {
                        _port = argument;
                    }

                    continue;
                }
            }

            // check whether the host has been found yet
            // if not, this must be the username/host argument
            if (_host.isEmpty()) {
                // check to see if only a hostname is specified, or whether
                // both a username and host are specified ( in which case they
                // are separated by an '@' character:  username@host )

                const int separatorPosition = args[i].indexOf(QLatin1Char('@'));
                if (separatorPosition != -1) {
                    // username and host specified
                    _user = args[i].left(separatorPosition);
                    _host = args[i].mid(separatorPosition + 1);
                } else {
                    // just the host specified
                    _host = args[i];
                }
            } else {
                // host has already been found, this must be part of the
                // command arguments.
                // Note this is not 100% correct.  If any of the above
                // noArgumentOptions or singleArgumentOptions are found, this
                // will not be correct (example ssh server top -i 50)
                // Suggest putting ssh command in quotes
                if (_command.isEmpty()) {
                    _command = args[i];
                } else {
                    _command = _command + QLatin1Char(' ') + args[i];
                }
            }
        }
    } else {
        qWarning() << "Could not read arguments";

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

QString SSHProcessInfo::format(const QString &input) const
{
    QString output(input);

    // search for and replace known markers
    output.replace(QLatin1String("%u"), _user);

    // provide 'user@' if user is defined -- this makes nicer
    // remote tabs possible: "%U%h %c" => User@Host Command
    //                                 => Host Command
    // Depending on whether -l was passed to ssh (which is mostly not the
    // case due to ~/.ssh/config).
    if (_user.isEmpty()) {
        output.replace(QLatin1String("%U"), QString());
    } else {
        output.replace(QLatin1String("%U"), _user + QLatin1Char('@'));
    }

    // test whether host is an ip address
    // in which case 'short host' and 'full host'
    // markers in the input string are replaced with
    // the full address
    struct in_addr address;
    const bool isIpAddress = inet_aton(_host.toLocal8Bit().constData(), &address) != 0;
    if (isIpAddress) {
        output.replace(QLatin1String("%h"), _host);
    } else {
        output.replace(QLatin1String("%h"), _host.left(_host.indexOf(QLatin1Char('.'))));
    }

    output.replace(QLatin1String("%H"), _host);
    output.replace(QLatin1String("%c"), _command);

    return output;
}

ProcessInfo *ProcessInfo::newInstance(int pid, const QString &titleFormat)
{
    ProcessInfo *info;
#if defined(Q_OS_LINUX)
    info = new LinuxProcessInfo(pid, titleFormat);
#elif defined(Q_OS_SOLARIS)
    info = new SolarisProcessInfo(pid, titleFormat);
#elif defined(Q_OS_MACOS)
    info = new MacProcessInfo(pid, titleFormat);
#elif defined(Q_OS_FREEBSD)
    info = new FreeBSDProcessInfo(pid, titleFormat);
#elif defined(Q_OS_OPENBSD)
    info = new OpenBSDProcessInfo(pid, titleFormat);
#else
    info = new NullProcessInfo(pid, titleFormat);
#endif
    info->readProcessInfo(pid);
    return info;
}
