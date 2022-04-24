/*************************************************************************************
 * SPDX-FileCopyrightText: 2016 Zhigalin Alexander <alexander@zhigalin.tk>
 * SPDX-FileCopyrightText: 2021 DI DIO Maximilien <maximilien.didio@gmail.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
 *************************************************************************************/

#ifndef KONSOLE_COLOR_SCHEME_CHOOSER_H
#define KONSOLE_COLOR_SCHEME_CHOOSER_H

#include <QAction>
#include <QObject>

#include <KColorSchemeManager>
#include <kconfigwidgets_version.h>

// Konsole
#include "konsoleprivate_export.h"

class KActionCollection;

/**
 * Provides a menu that will offer to change the color scheme
 *
 * Furthermore, it will save the selection in the user configuration.
 */
// TODO: Once the minimum KF version is changed to >= 5.93, remove this whole
// class and move the couple of lines of code from the constructor to MainWindow
class KONSOLEPRIVATE_EXPORT AppColorSchemeChooser : public QAction
{
public:
    explicit AppColorSchemeChooser(QObject *parent);

    // Not needed with KF >= 5.93
#if KCONFIGWIDGETS_VERSION < QT_VERSION_CHECK(5, 93, 0)
    QString currentSchemeName() const;
private Q_SLOTS:
    void slotSchemeChanged(QAction *triggeredAction);
#endif
};

#endif
