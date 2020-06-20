/*
 Copyright 2017 by Kurt Hindenburg <kurt.hindenburg@gmail.com>

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License as
 published by the Free Software Foundation; either version 2 of
 the License or (at your option) version 3 or any later version
 accepted by the membership of KDE e.V. (or its successor approved
 by the membership of KDE e.V.), which shall act as a proxy
 defined in Section 14 of version 3 of the license.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

// application header
#include "demo_konsolepart.h"

// KF headers
#include <KAboutData>
#include <KLocalizedString>

// Qt headers
#include <QApplication>
#include <QCommandLineParser>

int main(int argc, char **argv)
{
    auto app = new QApplication(argc, argv);

    KLocalizedString::setApplicationDomain("demo_konsolepart");

    KAboutData about(QStringLiteral("demo_konsolepart"),
                     i18nc("@title", "Demo KonsolePart"),
                     QStringLiteral("1.0"),
                     i18nc("@title", "Terminal emulator"),
                     KAboutLicense::GPL_V2,
                     i18nc("@info:credit", "(c) 2017, The Konsole Developers"),
                     QString(),
                     QStringLiteral("https://konsole.kde.org/"));

    KAboutData::setApplicationData(about);

    QSharedPointer<QCommandLineParser> parser(new QCommandLineParser);
    parser->setApplicationDescription(about.shortDescription());
    parser->addHelpOption();
    parser->addVersionOption();
    about.setupCommandLine(parser.data());

    QStringList args = app->arguments();

    parser->process(args);
    about.processCommandLine(parser.data());

    auto demo = new demo_konsolepart();
    demo->show();

    int ret = app->exec();
    delete app;
    return ret;
}
