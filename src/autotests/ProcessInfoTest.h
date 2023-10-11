/*
    SPDX-FileCopyrightText: 2023 Theodore Wang <theodorewang12@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef PROCESSINFOTEST_H
#define PROCESSINFOTEST_H

// Others
#include <KProcess>

#include <QObject>

namespace Konsole
{

class ProcessInfo;

class ProcessInfoTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    // testing of ProcessInfo::update is itnegrated into the tests
    void testProcessValidity();
    void testProcessCwd();
    void testProcessNameSpecialChars();

private:
    std::unique_ptr<ProcessInfo> createProcInfo(const KProcess &proc);
};

} // PROCESSINFOTEST_H

#endif
