/*
    SPDX-FileCopyrightText: 2007-2008 Robert Knight <robertknight@gmail.countm>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

// Config
#include "config-konsole.h"

// Own
#include "NullProcessInfo.h"
#include "ProcessInfo.h"
#include "UnixProcessInfo.h"
#include "konsoledebug.h"

// Unix
#ifndef Q_OS_WIN
#include <arpa/inet.h>
#include <cerrno>
#include <netinet/in.h>
#include <pwd.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

// Qt
#include <QDir>
#include <QFileInfo>
#include <QGuiApplication>
#include <QHostInfo>
#include <QStringList>
#include <QTextStream>
#include <QtGlobal>

// KDE
#include <KConfigGroup>
#include <KSandbox>
#include <KSharedConfig>
#include <KUser>

#if defined(Q_OS_LINUX) || defined(Q_OS_SOLARIS)
#include <memory>
#endif

#if defined(Q_OS_FREEBSD) || defined(Q_OS_OPENBSD) || defined(Q_OS_MACOS)
#include <QSharedPointer>
#include <sys/sysctl.h>
#endif

#if defined(Q_OS_MACOS)
#include <libproc.h>
#include <qplatformdefs.h>
#endif

#if defined(Q_OS_FREEBSD) || defined(Q_OS_OPENBSD)
#include <sys/types.h>

#include <sys/syslimits.h>
#include <sys/user.h>
#if defined(Q_OS_FREEBSD)
#include <libutil.h>
#include <sys/param.h>
#include <sys/queue.h>
#endif
#endif

using namespace Konsole;

ProcessInfo::ProcessInfo(int pid)
    : _fields(ARGUMENTS) // arguments
    // are currently always valid,
    // they just return an empty
    // vector / map respectively
    // if no arguments
    // have been explicitly set
    , _pid(pid)
    , _parentPid(0)
    , _foregroundPid(0)
    , _userId(0)
    , _lastError(NoError)
    , _name(QString())
    , _userName(QString())
    , _userHomeDir(QString())
    , _currentDir(QString())
    , _userNameRequired(true)
    , _arguments(QVector<QString>())
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
    readProcessName(_pid);
}

void ProcessInfo::refreshArguments()
{
    clearArguments();
    readArguments(_pid);
}

QString ProcessInfo::validCurrentDir() const
{
    bool ok = false;

    // read current dir, if an error occurs try the parent as the next
    // best option
    int currentPid = parentPid(&ok);
    QString dir = currentDir(&ok);
    while (!ok && currentPid != 0) {
        ProcessInfo *current = ProcessInfo::newInstance(currentPid);
        current->update();
        currentPid = current->parentPid(&ok);
        dir = current->currentDir(&ok);
        delete current;
    }

    return dir;
}

QStringList ProcessInfo::_commonDirNames;

QStringList ProcessInfo::commonDirNames()
{
    static bool forTheFirstTime = true;

    if (forTheFirstTime) {
        const KSharedConfigPtr &config = KSharedConfig::openConfig();
        const KConfigGroup &configGroup = config->group(QStringLiteral("ProcessInfo"));
        // Need to make a local copy so the begin() and end() point to the same QList
        _commonDirNames = configGroup.readEntry("CommonDirNames", QStringList());
        _commonDirNames.removeDuplicates();

        forTheFirstTime = false;
    }

    return _commonDirNames;
}

QString ProcessInfo::formatShortDir(const QString &input) const
{
    if (input == QLatin1Char('/')) {
        return QStringLiteral("/");
    }

    QString result;

    const QStringList parts = input.split(QDir::separator());

    QStringList dirNamesToShorten = commonDirNames();

    // go backwards through the list of the path's parts
    // adding abbreviations of common directory names
    // and stopping when we reach a dir name which is not
    // in the commonDirNames set
    for (auto it = parts.crbegin(), endIt = parts.crend(); it != endIt; ++it) {
        const QString &part = *it;
        if (dirNamesToShorten.contains(part)) {
            result.prepend(QDir::separator() + static_cast<QString>(part[0]));
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

#if defined(Q_OS_LINUX)
#include <QByteArray>
#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDBusMetaType>
#include <QFile>
#include <QMap>
#include <QString>
#include <QThread>
#include <QVariant>

#include "KonsoleSettings.h"

typedef QPair<QString, QDBusVariant> VariantPair;
typedef QList<VariantPair> VariantList;
typedef QList<QPair<QString, VariantList>> EmptyArray;

Q_DECLARE_METATYPE(VariantPair);
Q_DECLARE_METATYPE(VariantList);
Q_DECLARE_METATYPE(EmptyArray);

// EmptyArray is a custom type used for the last argument of org.freedesktop.systemd1.Manager.StartTransientUnit

QDBusArgument &operator<<(QDBusArgument &argument, const VariantPair pair)
{
    argument.beginStructure();
    argument << pair.first;
    argument << pair.second;
    argument.endStructure();

    return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument, VariantPair &pair)
{
    argument.beginStructure();
    argument >> pair.first;
    QVariant value;
    argument >> value;
    pair.second = qvariant_cast<QDBusVariant>(value);
    argument.endStructure();

    return argument;
}

QDBusArgument &operator<<(QDBusArgument &argument, const QPair<QString, VariantList> pair)
{
    argument.beginStructure();
    argument << pair.first;
    argument << pair.second;
    argument.endStructure();

    return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument, QPair<QString, VariantList> &pair)
{
    argument.beginStructure();
    argument >> pair.first;
    VariantList variantList;
    argument >> variantList;
    pair.second = variantList;
    argument.endStructure();

    return argument;
}

class LinuxProcessInfo : public UnixProcessInfo
{
public:
    explicit LinuxProcessInfo(int pid, int sessionPid)
        : UnixProcessInfo(pid)
    {
        if (pid == 0 || _cGroupCreationFailed) {
            return;
        }

        if (_createdAppCGroupPath.isEmpty() && !_cGroupCreationFailed) {
            _cGroupCreationFailed = !initCGroupHierachy(pid);
            if (KonsoleSettings::enableMemoryMonitoring() && !_cGroupCreationFailed) {
                setUnitMemLimit(KonsoleSettings::memoryLimitValue());
            }
            return;
        }

        const bool isForegroundProcess = sessionPid != -1;

        if (!isForegroundProcess) {
            _cGroupCreationFailed = !createCGroup(QStringLiteral("tab(%1).scope").arg(pid), pid);
        } else {
            _cGroupCreationFailed = !moveToCGroup(getProcCGroup(sessionPid), pid);
        }
    }

    static bool setUnitMemLimit(const int newMemHigh)
    {
        QFile memHighFile(_createdAppCGroupPath + QDir::separator() + QStringLiteral("memory.high"));

        if (_cGroupCreationFailed || !memHighFile.open(QIODevice::WriteOnly)) {
            return false;
        }

        memHighFile.write(QStringLiteral("%1M").arg(newMemHigh).toLocal8Bit());

        return true;
    }

protected:
    bool readCurrentDir(int pid) override
    {
        char path_buffer[MAXPATHLEN + 1];
        path_buffer[MAXPATHLEN] = 0;
        QByteArray procCwd = QFile::encodeName(QStringLiteral("/proc/%1/cwd").arg(pid));
        const auto length = static_cast<int>(readlink(procCwd.constData(), path_buffer, MAXPATHLEN));
        if (length == -1) {
            setError(UnknownError);
            return false;
        }

        path_buffer[length] = '\0';
        QString path = QFile::decodeName(path_buffer);

        setCurrentDir(path);
        return true;
    }

    bool readProcessName(int pid) override
    {
        Q_UNUSED(pid);

        if (!_infoFile || _infoFile->error() != 0 || !_infoFile->reset()) {
            return false;
        }

        const QString data = QString::fromUtf8(_infoFile->readLine());

        if (data.isEmpty()) {
            setName(data);
            return false;
        }

        const int nameStartIdx = data.indexOf(QLatin1Char('(')) + 1;
        const int nameLength = data.lastIndexOf(QLatin1Char(')')) - nameStartIdx;

        setName(data.mid(nameStartIdx, nameLength));
        return true;
    }

private:
    bool readProcInfo(int pid) override
    {
        // indices of various fields within the process status file which
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

            uidStrings << uidLine.split(QLatin1Char('\t'), Qt::SkipEmptyParts);
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
                setUserNameRequired(false);
            }
        } else {
            setFileError(statusInfo.error());
            return false;
        }

        // read process status file ( /proc/<pid/stat )
        //
        // the expected file format is a list of fields separated by spaces, using
        // parentheses to escape fields such as the process name which may itself contain
        // spaces:
        //
        // FIELD FIELD (FIELD WITH SPACES) FIELD FIELD
        //
        _infoFile = std::make_unique<QFile>(new QFile());
        _infoFile->setFileName(QStringLiteral("/proc/%1/stat").arg(pid));
        if (_infoFile->open(QIODevice::ReadOnly)) {
            QTextStream stream(_infoFile.get());
            const QString &data = stream.readAll();

            int field = 0;
            int pos = 0;

            while (pos < data.length()) {
                QChar c = data[pos];

                if (c == QLatin1Char(' ')) {
                    field++;
                } else {
                    switch (field) {
                    case PARENT_PID_FIELD:
                        parentPidString.append(c);
                        break;
                    case PROCESS_NAME_FIELD: {
                        pos++;
                        const int NAME_MAX_LEN = 16;
                        const int nameEndIdx = data.lastIndexOf(QStringLiteral(")"), pos + NAME_MAX_LEN);
                        processNameString = data.mid(pos, nameEndIdx - pos);
                        pos = nameEndIdx;
                        break;
                    }
                    case GROUP_PROCESS_FIELD:
                        foregroundPidString.append(c);
                        break;
                    }
                }

                pos++;
            }
        } else {
            setFileError(_infoFile->error());
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

    QDBusMessage callSmdDBus(const QString &objectPath,
                             const QString &interfaceName,
                             const QString &methodName,
                             const QList<QVariant> &args,
                             const QString &interfacePrefix = QString{})
    {
        const QString service(QStringLiteral("org.freedesktop.systemd1"));
        QString interface;
        if (interfacePrefix.isEmpty()) {
            interface = service + u"." + interfaceName;
        } else {
            interface = interfacePrefix + u"." + interfaceName;
        }

        auto methodCall = QDBusMessage::createMethodCall(service, objectPath, interface, methodName);
        methodCall.setArguments(args);

        return QDBusConnection::sessionBus().call(methodCall);
    }

    bool initCGroupHierachy(int pid)
    {
        qDBusRegisterMetaType<VariantPair>();
        qDBusRegisterMetaType<VariantList>();
        qDBusRegisterMetaType<QPair<QString, VariantList>>();
        qDBusRegisterMetaType<EmptyArray>();
        const pid_t selfPid = getpid();

        const QString managerObjPath(QStringLiteral("/org/freedesktop/systemd1"));

        // check if systemd dbus services exist
        if (!QDBusConnection::sessionBus().interface()->isServiceRegistered(QStringLiteral("org.freedesktop.systemd1"))) {
            return false;
        }

        // get current application cgroup path
        const QString oldAppCGroupPath(getProcCGroup(selfPid));

        auto appUnitName = QStringLiteral("app-%1-%2.scope").arg(QGuiApplication::desktopFileName()).arg(selfPid);
        qCDebug(KonsoleDebug) << "Creating scope" << appUnitName;

        // create application unit
        VariantList properties;
        const QList<uint> mainPid({static_cast<quint32>(selfPid)});

        properties.append(VariantPair({QStringLiteral("Delegate"), QDBusVariant(QVariant::fromValue(true))}));
        properties.append(VariantPair({QStringLiteral("ManagedOOMMemoryPressure"), QDBusVariant(QVariant::fromValue(QStringLiteral("kill")))}));
        properties.append(VariantPair({QStringLiteral("PIDs"), QDBusVariant(QVariant::fromValue(mainPid))}));

        if (!createSystemdUnit(appUnitName, properties)) {
            return false;
        }

        // get created app cgroup path
        QString newAppCGroupPath = getProcCGroup(selfPid);
        if (newAppCGroupPath == oldAppCGroupPath) {
            return false;
        }

        _createdAppCGroupPath = newAppCGroupPath;

        // create sub cgroups
        if (!createCGroup(QStringLiteral("main.scope"), selfPid) || !createCGroup(QStringLiteral("tab(%1).scope").arg(pid), pid)) {
            return false;
        }

        // enable all controllers
        QFile appCGroupContsFile(_createdAppCGroupPath + QStringLiteral("/cgroup.controllers"));
        QFile appCGroupSTContsFile(_createdAppCGroupPath + QStringLiteral("/cgroup.subtree_control"));

        if (!appCGroupContsFile.open(QIODevice::ReadOnly) || !appCGroupSTContsFile.open(QIODevice::WriteOnly)) {
            setFileError(static_cast<QFile::FileError>(appCGroupContsFile.error() | appCGroupSTContsFile.error()));
            return false;
        }

        const QStringList conts(QString::fromUtf8(appCGroupContsFile.readAll()).split(QLatin1Char(' ')));
        QString contsToEnable;

        for (auto cont : conts) {
            contsToEnable += QLatin1Char('+') + cont.simplified();
            if (!conts.endsWith(cont)) {
                contsToEnable += QLatin1Char(' ');
            }
        }

        appCGroupSTContsFile.write(contsToEnable.toLocal8Bit());

        return true;
    }

    QString getProcCGroup(const int pid)
    {
        const QString cGroupFilePath(QStringLiteral("/proc/%1/cgroup").arg(pid));
        QFile cGroupFile(cGroupFilePath);

        if (!cGroupFile.open(QIODevice::ReadOnly)) {
            setFileError(cGroupFile.error());
            return QString();
        }

        const QString data = QString::fromUtf8(cGroupFile.readAll());
        const qsizetype lastColonPos = data.lastIndexOf(QLatin1Char(':'));
        if (lastColonPos < 0) {
            return QString();
        }

        QStringView cGroupPath(QStringView(data).mid(lastColonPos + 1).trimmed());
        return QString(QStringLiteral("/sys/fs/cgroup") + cGroupPath);
    }

    bool createSystemdUnit(const QString &name, const VariantList &propList)
    {
        using namespace Qt::StringLiterals;

        const QList<QVariant> args({name, u"fail"_s, QVariant::fromValue(propList), QVariant::fromValue(EmptyArray())});

        auto message = callSmdDBus(u"/org/freedesktop/systemd1"_s, u"Manager"_s, u"StartTransientUnit"_s, args);
        if (message.type() == QDBusMessage::ErrorMessage) {
            return false;
        }

        auto reply = QDBusReply<QDBusObjectPath>(message);
        if (!reply.isValid()) {
            return false;
        }

        auto path = reply.value();

        // Wait for the job to finish
        // We really should be listening for the "JobRemoved" signal of systemd's Manager object,
        // but that is hard as it makes things async, so instead just wait for the job object to
        // disappear and if it does, assume the job finished successfully.
        int tries = 10;
        bool jobExists = true;
        while (tries > 0) {
            const auto jobArgs = QList<QVariant>{u"org.freedesktop.systemd1.Job"_s, u"State"_s};
            auto jobReply = callSmdDBus(path.path(), u"Properties"_s, u"Get"_s, jobArgs, u"org.freedesktop.DBus"_s);
            if (jobReply.type() == QDBusMessage::ErrorMessage) {
                jobExists = false;
                break;
            }

            QThread::sleep(std::chrono::milliseconds(10));
            tries--;
        }

        return !jobExists;
    }

    bool createCGroup(const QString &name, int initialPid)
    {
        const QString newCGroupPath(_createdAppCGroupPath + QLatin1Char('/') + name);

        QDir::root().mkpath(newCGroupPath);

        return moveToCGroup(newCGroupPath, initialPid);
    }

    bool moveToCGroup(const QString &cGroupPath, int pid)
    {
        QFile cGroupProcs(cGroupPath + QStringLiteral("/cgroup.procs"));

        if (!cGroupProcs.open(QIODevice::WriteOnly)) {
            setFileError(cGroupProcs.error());
            return false;
        }

        cGroupProcs.write(QStringLiteral("%1").arg(pid).toLocal8Bit());

        return true;
    }

    static QString _createdAppCGroupPath;
    static bool _cGroupCreationFailed;
    std::unique_ptr<QFile> _infoFile;
};

QString LinuxProcessInfo::_createdAppCGroupPath = QString();
bool LinuxProcessInfo::_cGroupCreationFailed = false;

class FlatpakProcessInfo : public ProcessInfo
{
public:
    FlatpakProcessInfo(int pid, QByteArrayView tty)
        : ProcessInfo(pid)
    {
        QProcess proc;
        proc.setProgram(QStringLiteral("ps"));
        proc.setArguments({
            QStringLiteral("-o"),
            QStringLiteral("%U\t%p\t%c"),
            QStringLiteral("--tty"),
            QString::fromLatin1(tty.data()),
            QStringLiteral("--no-headers"),
        });

        KSandbox::startHostProcess(proc, QProcess::ReadOnly);

        proc.waitForStarted();
        proc.waitForFinished();

        const QString out = QString::fromUtf8(proc.readAllStandardOutput());
        for (QStringView line : QStringTokenizer(out, u'\n', Qt::SkipEmptyParts)) {
            using Container = QVarLengthArray<QStringView, 4>;
            const Container tokens = QStringTokenizer(line, u'\t', Qt::SkipEmptyParts).toContainer<Container>();
            if (tokens[1].toInt() == pid) {
                setUserName(tokens[0].trimmed().toString());
                setPid(tokens[1].trimmed().toInt());
                setName(tokens[2].trimmed().toString());
            }
        }
    }

    void readProcessInfo(int) override
    {
        /* done in ctor */
    }

    bool readProcessName(int) override
    {
        return true;
    }

    bool readCurrentDir(int) override
    {
        bool ok = false;
        QString pidString = QString::number(pid(&ok));
        if (!ok) {
            return false;
        }

        QProcess proc;
        proc.setProgram(QStringLiteral("pwdx"));
        proc.setArguments({pidString});
        KSandbox::startHostProcess(proc, QProcess::ReadOnly);
        if (proc.waitForStarted() && proc.waitForFinished()) {
            proc.setReadChannel(QProcess::StandardOutput);
            char buffer[4096];
            int readCount = proc.readLine(buffer, sizeof(buffer));
            auto line = readCount > 0 ? QByteArrayView(buffer, readCount).trimmed() : QByteArrayView();
            if (int colon = line.indexOf(": "); colon != -1) {
                setCurrentDir(QString::fromUtf8(line.mid(colon + 2)));
                return true;
            }
        }
        return false;
    }

    bool readArguments(int) override
    {
        return false;
    }

    void readUserName(void) override
    {
        /* done in ctor */
    }
};

#elif defined(Q_OS_FREEBSD)
class FreeBSDProcessInfo : public UnixProcessInfo
{
public:
    explicit FreeBSDProcessInfo(int pid)
        : UnixProcessInfo(pid)
    {
    }

protected:
    bool readCurrentDir(int pid) override
    {
#if HAVE_OS_DRAGONFLYBSD
        int managementInfoBase[4];
        char buf[PATH_MAX];
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
        struct kinfo_file *info = nullptr;

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

    bool readProcessName(int pid) override
    {
        int managementInfoBase[4];

        managementInfoBase[0] = CTL_KERN;
        managementInfoBase[1] = KERN_PROC;
        managementInfoBase[2] = KERN_PROC_PID;
        managementInfoBase[3] = pid;

        auto kInfoProc = getProcInfoStruct(managementInfoBase, 4);

        if (kInfoProc == nullptr) {
            return false;
        }

#if HAVE_OS_DRAGONFLYBSD
        setName(QString::fromUtf8(kInfoProc->kp_comm));
#else
        setName(QString::fromUtf8(kInfoProc->ki_comm));
#endif

        return true;
    }

private:
    bool readProcInfo(int pid) override
    {
        int managementInfoBase[4];

        managementInfoBase[0] = CTL_KERN;
        managementInfoBase[1] = KERN_PROC;
        managementInfoBase[2] = KERN_PROC_PID;
        managementInfoBase[3] = pid;

        auto kInfoProc = getProcInfoStruct(managementInfoBase, 4);

        if (kInfoProc == nullptr) {
            return false;
        }

#if HAVE_OS_DRAGONFLYBSD
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

        return true;
    }

    bool readArguments(int pid) override
    {
        int managementInfoBase[4];
        char args[ARG_MAX];
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
        const QStringList argurments = QString::fromLocal8Bit(args, len).split(QLatin1Char('\u0000'));
        for (const QString &value : argurments) {
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
    explicit OpenBSDProcessInfo(int pid)
        : UnixProcessInfo(pid)
    {
    }

protected:
    bool readCurrentDir(int pid) override
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

    bool readProcessName(int pid) override
    {
        int managementInfoBase[6];

        managementInfoBase[0] = CTL_KERN;
        managementInfoBase[1] = KERN_PROC;
        managementInfoBase[2] = KERN_PROC_PID;
        managementInfoBase[3] = pid;
        managementInfoBase[4] = sizeof(struct kinfo_proc);
        managementInfoBase[5] = 1;

        auto kInfoProc = getProcInfoStruct(managementInfoBase, 6);

        if (kInfoProc == nullptr) {
            return false;
        }

        setName(QString::fromUtf8(kInfoProc->p_comm));

        return true;
    }

private:
    bool readProcInfo(int pid) override
    {
        int managementInfoBase[6];

        managementInfoBase[0] = CTL_KERN;
        managementInfoBase[1] = KERN_PROC;
        managementInfoBase[2] = KERN_PROC_PID;
        managementInfoBase[3] = pid;
        managementInfoBase[4] = sizeof(struct kinfo_proc);
        managementInfoBase[5] = 1;

        auto kInfoProc = getProcInfoStruct(managementInfoBase, 6);

        if (kInfoProc == nullptr) {
            return false;
        }

        setName(QString::fromUtf8(kInfoProc->p_comm));
        setPid(kInfoProc->p_pid);
        setParentPid(kInfoProc->p_ppid);
        setForegroundPid(kInfoProc->p_tpgid);
        setUserId(kInfoProc->p_uid);
        setUserName(QString::fromUtf8(kInfoProc->p_login));

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

    bool readArguments(int pid) override
    {
        char **argv;

        argv = readProcArgs(pid, KERN_PROC_ARGV);
        if (argv == NULL) {
            return false;
        }

        for (char **p = argv; *p != NULL; p++) {
            addArgument(QString::fromUtf8(*p));
        }
        free(argv);
        return true;
    }
};

#elif defined(Q_OS_MACOS)
class MacProcessInfo : public UnixProcessInfo
{
public:
    explicit MacProcessInfo(int pid)
        : UnixProcessInfo(pid)
    {
    }

protected:
    bool readCurrentDir(int pid) override
    {
        struct proc_vnodepathinfo vpi;
        const int nb = proc_pidinfo(pid, PROC_PIDVNODEPATHINFO, 0, &vpi, sizeof(vpi));
        if (nb == sizeof(vpi)) {
            setCurrentDir(QString::fromUtf8(vpi.pvi_cdir.vip_path));
            return true;
        }
        return false;
    }

    bool readProcessName(int pid) override
    {
        int managementInfoBase[4];

        managementInfoBase[0] = CTL_KERN;
        managementInfoBase[1] = KERN_PROC;
        managementInfoBase[2] = KERN_PROC_PID;
        managementInfoBase[3] = pid;

        auto kInfoProc = getProcInfoStruct(managementInfoBase, 4);

        if (kInfoProc != nullptr) {
            setName(QString::fromUtf8(kInfoProc->kp_proc.p_comm));
            return true;
        }

        return false;
    }

private:
    bool readProcInfo(int pid) override
    {
        QT_STATBUF statInfo;
        int managementInfoBase[4];

        managementInfoBase[0] = CTL_KERN;
        managementInfoBase[1] = KERN_PROC;
        managementInfoBase[2] = KERN_PROC_PID;
        managementInfoBase[3] = pid;

        // Find the tty device of 'pid' (Example: /dev/ttys001)
        auto kInfoProc = getProcInfoStruct(managementInfoBase, 4);

        if (kInfoProc == nullptr) {
            return false;
        }

        const QString deviceNumber = QString::fromUtf8(devname(((&kInfoProc->kp_eproc)->e_tdev), S_IFCHR));
        const QString fullDeviceName = QStringLiteral("/dev/") + deviceNumber.rightJustified(3, QLatin1Char('0'));

        setParentPid(kInfoProc->kp_eproc.e_ppid);
        setForegroundPid(kInfoProc->kp_eproc.e_pgid);

        const QByteArray deviceName = fullDeviceName.toLatin1();
        const char *ttyName = deviceName.data();

        if (QT_STAT(ttyName, &statInfo) != 0) {
            return false;
        }

        managementInfoBase[2] = KERN_PROC_TTY;
        managementInfoBase[3] = statInfo.st_rdev;

        // Find all processes attached to ttyName
        kInfoProc = getProcInfoStruct(managementInfoBase, 4);

        if (kInfoProc == nullptr) {
            return false;
        }

        // The foreground program is the first one
        setName(QString::fromUtf8(kInfoProc->kp_proc.p_comm));

        setPid(pid);

        // Get user id - this will allow using username
        struct proc_bsdshortinfo bsdinfo;
        int ret;
        bool ok = false;
        const int fpid = foregroundPid(&ok);
        if (ok) {
            ret = proc_pidinfo(fpid, PROC_PIDT_SHORTBSDINFO, 0, &bsdinfo, sizeof(bsdinfo));
            if (ret == sizeof(bsdinfo)) {
                setUserId(bsdinfo.pbsi_uid);
            }
            // This will cause constant opening of /etc/passwd
            if (userNameRequired()) {
                readUserName();
                setUserNameRequired(false);
            }
        }

        return true;
    }

    bool readArguments(int pid) override
    {
        int managementInfoBase[3];
        size_t size;
        std::string procargs;

        managementInfoBase[0] = CTL_KERN;
        managementInfoBase[1] = KERN_PROCARGS2;
        managementInfoBase[2] = pid;

        // It is not clear on why this fails for some commands
        if (sysctl(managementInfoBase, 3, nullptr, &size, nullptr, 0) == -1) {
            qWarning() << "OS_MACOS: unable to obtain argument size for " << pid;
            return false;
        }

        // Some macosx versions need extra space
        const size_t argmax = size + 32;
        procargs.resize(argmax);

        if (sysctl(managementInfoBase, 3, &procargs[0], &size, nullptr, 0) == -1) {
            qWarning() << "OS_MACOS: unable to obtain arguments for " << pid;
            return false;
        } else {
            auto args = QString::fromStdString(procargs);
            auto parts = args.split(QLatin1Char('\u0000'), Qt::SkipEmptyParts);
            // Do a lot of data checks
            if (parts.isEmpty()) {
                return false;
            }
            /*  0: argc as \u####
                1: full command path
                2: command (no path)
                3: first argument
                4+: next arguments
                argc+: a lot of other data including ENV
            */
            auto argcs = parts[0]; // first string argc
            if (argcs.isEmpty()) {
                return false;
            }
            auto argc = (int)QChar(argcs[0]).unicode();
            if (argc < 1) { // trash
                return false;
            } else if (argc == 1) { // just command, no args
                addArgument(parts[1]); // this is the full path + command
                return true;
            }

            // Check argc + 2(argc/full cmd) is not larger then parts size
            argc = qMin(argc + 2, parts.size());

            /* The output is not obvious for some commands:
               For example: 'man 3 sysctl' shows arg=4
               0: \u0004; 1: /bin/bash; 2: /bin/sh; 3: /usr/bin/man; 4: 3; 5: sysctl
             */
            for (int i = 2; i < argc; i++) {
                addArgument(parts[i]);
            }
            return true;
        }
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
    explicit SolarisProcessInfo(int pid)
        : UnixProcessInfo(pid)
    {
    }

protected:
    // FIXME: This will have the same issues as BKO 251351; the Linux
    // version uses readlink.
    bool readCurrentDir(int pid) override
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

    bool readProcessName(int /*pid*/) override
    {
        if (execNameFile->isOpen()) {
            const QString data(execNameFile->readAll());
            setName(execNameFile->readAll().constData());
            return true;
        }
        return false;
    }

private:
    bool readProcInfo(int pid) override
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

        _execNameFile = std::make_unique(new QFile());
        _execNameFile->setFileName(QString("/proc/%1/execname").arg(pid));
        _execNameFile->open(QIODevice::ReadOnly);

        return true;
    }

    std::unique_ptr<QFile> _execNameFile;
};
#endif

ProcessInfo *ProcessInfo::newInstance(int pid, int sessionPid, QByteArrayView tty)
{
    ProcessInfo *info;
#if defined(Q_OS_LINUX)
    if (KSandbox::isFlatpak()) {
        if (tty.isEmpty()) {
            info = new NullProcessInfo(pid);
        } else {
            info = new FlatpakProcessInfo(pid, tty);
        }
    } else {
        info = new LinuxProcessInfo(pid, sessionPid);
    }
#elif defined(Q_OS_SOLARIS)
    info = new SolarisProcessInfo(pid);
#elif defined(Q_OS_MACOS)
    info = new MacProcessInfo(pid);
#elif defined(Q_OS_FREEBSD)
    info = new FreeBSDProcessInfo(pid);
#elif defined(Q_OS_OPENBSD)
    info = new OpenBSDProcessInfo(pid);
#else
    info = new NullProcessInfo(pid);
#endif
    info->readProcessInfo(pid);
    return info;
}
