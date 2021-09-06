/*
    SPDX-FileCopyrightText: 2007-2008 Robert Knight <robertknight@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

// Own
#include "SSHProcessInfo.h"

// Unix
#include <arpa/inet.h>
#include <cerrno>
#include <netinet/in.h>
#include <pwd.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <unistd.h>

// Qt
#include <QDebug>

using namespace Konsole;

SSHProcessInfo::SSHProcessInfo(const ProcessInfo &process)
    : _process(process)
    , _user(QString())
    , _host(QString())
    , _port(QString())
    , _command(QString())
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
    static const QString singleArgumentOptions(QStringLiteral("bcDeFIiJLlmOopRSWw"));

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
        output.remove(QLatin1String("%U"));
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
