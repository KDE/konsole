/*
 SPDX-FileCopyrightText: 2017 Kurt Hindenburg <kurt.hindenburg@gmail.com>

 SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#ifndef DEMO_KONSOLEPART_H
#define DEMO_KONSOLEPART_H

#include <KMainWindow>
#include <KParts/Part>
#include <KParts/ReadOnlyPart>
#include <kde_terminal_interface.h>

class demo_konsolepart : public KMainWindow
{
    Q_OBJECT
public:
    demo_konsolepart();

    ~demo_konsolepart() override;

    void manageProfiles();

public Q_SLOTS:
    void quit();

private:
    Q_DISABLE_COPY(demo_konsolepart)

    KParts::ReadOnlyPart *createPart();

    KMainWindow *_mainWindow;
    KParts::ReadOnlyPart *_terminalPart;
    TerminalInterface *_terminal;
};

#endif
