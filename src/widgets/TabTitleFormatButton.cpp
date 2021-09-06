/*
    SPDX-FileCopyrightText: 2007-2008 Robert Knight <robertknight@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

// Own
#include "widgets/TabTitleFormatButton.h"

// Qt
#include <QList>
#include <QMenu>

// KDE
#include <KLocalizedString>

using namespace Konsole;

const TabTitleFormatButton::Element TabTitleFormatButton::_localElements[] = {
    {QStringLiteral("%n"), I18N_NOOP("Program Name: %n")},
    {QStringLiteral("%d"), I18N_NOOP("Current Directory (Short): %d")},
    {QStringLiteral("%D"), I18N_NOOP("Current Directory (Long): %D")},
    {QStringLiteral("%w"), I18N_NOOP("Window Title Set by Shell: %w")},
    {QStringLiteral("%#"), I18N_NOOP("Session Number: %#")},
    {QStringLiteral("%u"), I18N_NOOP("User Name: %u")},
    {QStringLiteral("%h"), I18N_NOOP("Local Host: %h")},
    {
        QStringLiteral("%B"),
        I18N_NOOP("User's Bourne prompt sigil: %B"),
    } //  ($, or # for superuser)
};
const int TabTitleFormatButton::_localElementCount = sizeof(_localElements) / sizeof(TabTitleFormatButton::Element);

const TabTitleFormatButton::Element TabTitleFormatButton::_remoteElements[] = {
    {QStringLiteral("%u"), I18N_NOOP("User Name: %u")},
    {QStringLiteral("%U"), I18N_NOOP("User Name@ (if given): %U")},
    {QStringLiteral("%h"), I18N_NOOP("Remote Host (Short): %h")},
    {QStringLiteral("%H"), I18N_NOOP("Remote Host (Long): %H")},
    {QStringLiteral("%c"), I18N_NOOP("Command and arguments: %c")},
    {QStringLiteral("%w"), I18N_NOOP("Window Title Set by Shell: %w")},
    {QStringLiteral("%#"), I18N_NOOP("Session Number: %#")},
};
const int TabTitleFormatButton::_remoteElementCount = sizeof(_remoteElements) / sizeof(TabTitleFormatButton::Element);

TabTitleFormatButton::TabTitleFormatButton(QWidget *parent)
    : QPushButton(parent)
    , _context(Session::LocalTabTitle)
{
    setText(i18n("Insert"));
    setMenu(new QMenu());
    connect(menu(), &QMenu::triggered, this, &Konsole::TabTitleFormatButton::fireElementSelected);
}

TabTitleFormatButton::~TabTitleFormatButton()
{
    menu()->deleteLater();
}

void TabTitleFormatButton::fireElementSelected(QAction *action)
{
    Q_EMIT dynamicElementSelected(action->data().toString());
}

void TabTitleFormatButton::setContext(Session::TabTitleContext titleContext)
{
    _context = titleContext;

    menu()->clear();

    int count = 0;
    const Element *array = nullptr;

    if (titleContext == Session::LocalTabTitle) {
        setToolTip(i18nc("@info:tooltip", "Insert title format"));
        array = _localElements;
        count = _localElementCount;
    } else if (titleContext == Session::RemoteTabTitle) {
        setToolTip(i18nc("@info:tooltip", "Insert remote title format"));
        array = _remoteElements;
        count = _remoteElementCount;
    }

    QList<QAction *> menuActions;
    menuActions.reserve(count);

    for (int i = 0; i < count; i++) {
        QAction *action = new QAction(i18n(array[i].description), this);
        action->setData(array[i].element);
        menuActions << action;
    }

    menu()->addActions(menuActions);
}

Session::TabTitleContext TabTitleFormatButton::context() const
{
    return _context;
}
