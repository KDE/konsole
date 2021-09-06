/*
    SPDX-FileCopyrightText: 2007-2008 Robert Knight <robertknight@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef TABTITLEFORMATBUTTON_H
#define TABTITLEFORMATBUTTON_H

// Qt
#include <QAction>
#include <QPushButton>

// Konsole
#include "session/Session.h"

namespace Konsole
{
class TabTitleFormatButton : public QPushButton
{
    Q_OBJECT

public:
    explicit TabTitleFormatButton(QWidget *parent);
    ~TabTitleFormatButton() override;

    void setContext(Session::TabTitleContext titleContext);
    Session::TabTitleContext context() const;

Q_SIGNALS:
    void dynamicElementSelected(const QString &);

private Q_SLOTS:
    void fireElementSelected(QAction *);

private:
    Session::TabTitleContext _context;

    struct Element {
        QString element;
        const char *description;
    };
    static const Element _localElements[];
    static const int _localElementCount;
    static const Element _remoteElements[];
    static const int _remoteElementCount;
};
}

#endif // TABTITLEFORMATBUTTON_H
