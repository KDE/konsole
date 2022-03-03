/*
    SPDX-FileCopyrightText: 2008 Robert Knight <robertknight@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef PARTMANUALTEST_H
#define PARTMANUALTEST_H

#include <QEventLoop>

#include <KParts/Part>
#include <kde_terminal_interface.h>

class QKeyEvent;

namespace Konsole
{
class PartManualTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void testShortcutOverride();

    // marked as protected so they are not treated as test cases
protected Q_SLOTS:
    void overrideShortcut(QKeyEvent *event, bool &override);
    void shortcutTriggered();

private:
    KParts::Part *createPart();

    // variables for testShortcutOverride()
    bool _shortcutTriggered = false;
    bool _overrideCalled = false;
    bool _override = false;
    QEventLoop *_shortcutEventLoop = nullptr;
};

}

#endif // PARTMANUALTEST_H
