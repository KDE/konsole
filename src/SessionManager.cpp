/*
    This source file is part of Konsole, a terminal emulator.

    Copyright 2006-2008 by Robert Knight <robertknight@gmail.com>

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

// Own
#include "SessionManager.h"

// Qt
#include <QtCore/QStringList>
#include <QtCore/QSignalMapper>
#include <QtCore/QTextCodec>

// KDE
#include <KConfig>
#include <KConfigGroup>
#include <KGlobal>
#include <KDebug>

// Konsole
#include "Session.h"
#include "ProfileManager.h"
#include "History.h"
#include "Enumeration.h"

using namespace Konsole;

SessionManager::SessionManager()
{
    //map finished() signals from sessions
    _sessionMapper = new QSignalMapper(this);
    connect(_sessionMapper , SIGNAL(mapped(QObject*)) , this ,
            SLOT(sessionTerminated(QObject*)));

    ProfileManager* profileMananger = ProfileManager::instance();
    connect(profileMananger , SIGNAL(profileChanged(Profile::Ptr)) ,
            this , SLOT(profileChanged(Profile::Ptr)));
}

SessionManager::~SessionManager()
{
    if (_sessions.count() > 0) {
        kWarning() << "Konsole SessionManager destroyed with sessions still alive";
        // ensure that the Session doesn't later try to call back and do things to the
        // SessionManager
        foreach(Session* session, _sessions) {
            disconnect(session , 0 , this , 0);
        }
    }
}

K_GLOBAL_STATIC(SessionManager , theSessionManager)
SessionManager* SessionManager::instance()
{
    return theSessionManager;
}

void SessionManager::closeAllSessions()
{
    // close remaining sessions
    foreach(Session* session , _sessions) {
        session->close();
    }
    _sessions.clear();
}

const QList<Session*> SessionManager::sessions() const
{
    return _sessions;
}

Session* SessionManager::createSession(Profile::Ptr profile)
{
    if (!profile)
        profile = ProfileManager::instance()->defaultProfile();

    // TODO: check whether this is really needed
    if (!ProfileManager::instance()->loadedProfiles().contains(profile))
        ProfileManager::instance()->addProfile(profile);

    //configuration information found, create a new session based on this
    Session* session = new Session();
    Q_ASSERT(session);
    applyProfile(session, profile, false);

    connect(session , SIGNAL(profileChangeCommandReceived(QString)) , this ,
            SLOT(sessionProfileCommandReceived(QString)));

    //ask for notification when session dies
    _sessionMapper->setMapping(session, session);
    connect(session , SIGNAL(finished()) , _sessionMapper ,
            SLOT(map()));

    //add session to active list
    _sessions << session;
    _sessionProfiles.insert(session, profile);

    return session;
}
void SessionManager::profileChanged(Profile::Ptr profile)
{
    applyProfile(profile, true);
}

void SessionManager::sessionTerminated(QObject* sessionObject)
{
    Session* session = qobject_cast<Session*>(sessionObject);

    Q_ASSERT(session);

    _sessions.removeAll(session);
    _sessionProfiles.remove(session);
    _sessionRuntimeProfiles.remove(session);

    session->deleteLater();
}

void SessionManager::applyProfile(Profile::Ptr profile , bool modifiedPropertiesOnly)
{
    foreach(Session* session, _sessions) {
        if (_sessionProfiles[session] == profile)
            applyProfile(session, profile, modifiedPropertiesOnly);
    }
}
Profile::Ptr SessionManager::sessionProfile(Session* session) const
{
    return _sessionProfiles[session];
}
void SessionManager::setSessionProfile(Session* session, Profile::Ptr profile)
{
    if (!profile)
        profile = ProfileManager::instance()->defaultProfile();

    Q_ASSERT(profile);

    _sessionProfiles[session] = profile;

    applyProfile(session, profile, false);

    emit sessionUpdated(session);
}
void SessionManager::applyProfile(Session* session, const Profile::Ptr profile , bool modifiedPropertiesOnly)
{
    Q_ASSERT(profile);

    _sessionProfiles[session] = profile;

    ShouldApplyProperty apply(profile, modifiedPropertiesOnly);

    // Basic session settings
    if (apply.shouldApply(Profile::Name))
        session->setTitle(Session::NameRole, profile->name());

    if (apply.shouldApply(Profile::Command))
        session->setProgram(profile->command());

    if (apply.shouldApply(Profile::Arguments))
        session->setArguments(profile->arguments());

    if (apply.shouldApply(Profile::Directory))
        session->setInitialWorkingDirectory(profile->defaultWorkingDirectory());

    if (apply.shouldApply(Profile::Environment)) {
        // add environment variable containing home directory of current profile
        // (if specified)
        QStringList environment = profile->environment();
        environment << QString("PROFILEHOME=%1").arg(profile->defaultWorkingDirectory());
        environment << QString("KONSOLE_PROFILE_NAME=%1").arg(profile->name());

        session->setEnvironment(environment);
    }

    if ( apply.shouldApply(Profile::TerminalColumns) ||
            apply.shouldApply(Profile::TerminalRows) ) {
        const int columns = profile->property<int>(Profile::TerminalColumns);
        const int rows = profile->property<int>(Profile::TerminalRows);
        session->setPreferredSize(QSize(columns, rows));
    }

    if (apply.shouldApply(Profile::Icon))
        session->setIconName(profile->icon());

    // Key bindings
    if (apply.shouldApply(Profile::KeyBindings))
        session->setKeyBindings(profile->keyBindings());

    // Tab formats
    if (apply.shouldApply(Profile::LocalTabTitleFormat))
        session->setTabTitleFormat(Session::LocalTabTitle ,
                                   profile->localTabTitleFormat());
    if (apply.shouldApply(Profile::RemoteTabTitleFormat))
        session->setTabTitleFormat(Session::RemoteTabTitle ,
                                   profile->remoteTabTitleFormat());

    // History
    if (apply.shouldApply(Profile::HistoryMode) || apply.shouldApply(Profile::HistorySize)) {
        const int mode = profile->property<int>(Profile::HistoryMode);
        switch (mode) {
        case Enum::NoHistory:
            session->setHistoryType(HistoryTypeNone());
            break;

        case Enum::FixedSizeHistory: {
            int lines = profile->historySize();
            session->setHistoryType(CompactHistoryType(lines));
        }
        break;

        case Enum::UnlimitedHistory:
            session->setHistoryType(HistoryTypeFile());
            break;
        }
    }

    // Terminal features
    if (apply.shouldApply(Profile::FlowControlEnabled))
        session->setFlowControlEnabled(profile->flowControlEnabled());

    // Encoding
    if (apply.shouldApply(Profile::DefaultEncoding)) {
        QByteArray name = profile->defaultEncoding().toUtf8();
        session->setCodec(QTextCodec::codecForName(name));
    }

    // Monitor Silence
    if (apply.shouldApply(Profile::SilenceSeconds))
        session->setMonitorSilenceSeconds(profile->silenceSeconds());
}

void SessionManager::sessionProfileCommandReceived(const QString& text)
{
    Session* session = qobject_cast<Session*>(sender());
    Q_ASSERT(session);

    ProfileCommandParser parser;
    QHash<Profile::Property, QVariant> changes = parser.parse(text);

    Profile::Ptr newProfile;
    if (!_sessionRuntimeProfiles.contains(session)) {
        newProfile = new Profile(_sessionProfiles[session]);
        _sessionRuntimeProfiles.insert(session, newProfile);
    } else {
        newProfile = _sessionRuntimeProfiles[session];
    }

    QHashIterator<Profile::Property, QVariant> iter(changes);
    while (iter.hasNext()) {
        iter.next();
        newProfile->setProperty(iter.key(), iter.value());
    }

    _sessionProfiles[session] = newProfile;
    applyProfile(newProfile, true);
    emit sessionUpdated(session);
}

void SessionManager::saveSessions(KConfig* config)
{
    // The session IDs can't be restored.
    // So we need to map the old ID to the future new ID.
    int n = 1;
    _restoreMapping.clear();

    foreach(Session * session, _sessions) {
        QString name = QLatin1String("Session") + QString::number(n);
        KConfigGroup group(config, name);

        group.writePathEntry("Profile",
                             _sessionProfiles.value(session)->path());
        session->saveSession(group);
        _restoreMapping.insert(session, n);
        n++;
    }

    KConfigGroup group(config, "Number");
    group.writeEntry("NumberOfSessions", _sessions.count());
}

int SessionManager::getRestoreId(Session* session)
{
    return _restoreMapping.value(session);
}

void SessionManager::restoreSessions(KConfig* config)
{
    KConfigGroup group(config, "Number");
    int sessions;

    // Any sessions saved?
    if ((sessions = group.readEntry("NumberOfSessions", 0)) > 0) {
        for (int n = 1; n <= sessions; n++) {
            QString name = QLatin1String("Session") + QString::number(n);
            KConfigGroup sessionGroup(config, name);

            QString profile = sessionGroup.readPathEntry("Profile", QString());
            Profile::Ptr ptr = ProfileManager::instance()->defaultProfile();
            if (!profile.isEmpty()) ptr = ProfileManager::instance()->loadProfile(profile);

            Session* session = createSession(ptr);
            session->restoreSession(sessionGroup);
        }
    }
}

Session* SessionManager::idToSession(int id)
{
    Q_ASSERT(id);
    foreach(Session * session, _sessions) {
        if (session->sessionId() == id)
            return session;
    }
    // this should not happen
    Q_ASSERT(0);
    return 0;
}


#include "SessionManager.moc"

