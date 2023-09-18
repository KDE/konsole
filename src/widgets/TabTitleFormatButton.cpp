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

    {QStringLiteral("%n"), kli18n("Program Name: %n")},
    {QStringLiteral("%d"), kli18n("Current Directory (Short): %d")},
    {QStringLiteral("%D"), kli18n("Current Directory (Long): %D")},
    {QStringLiteral("%w"), kli18n("Window Title Set by Shell: %w")},
    {QStringLiteral("%#"), kli18n("Session Number: %#")},
    {QStringLiteral("%u"), kli18n("User Name: %u")},
    {QStringLiteral("%h"), kli18n("Local Host: %h")},
    {QStringLiteral("%B"), kli18n("User's Bourne prompt sigil: %B")}, //  ($, or # for superuser)
};

const int TabTitleFormatButton::_localElementCount = sizeof(_localElements) / sizeof(TabTitleFormatButton::Element);

const TabTitleFormatButton::Element TabTitleFormatButton::_remoteElements[] = {
    {QStringLiteral("%u"), kli18n("User Name: %u")},
    {QStringLiteral("%U"), kli18n("User Name@ (if given): %U")},
    {QStringLiteral("%h"), kli18n("Remote Host (Short): %h")},
    {QStringLiteral("%H"), kli18n("Remote Host (Long): %H")},
    {QStringLiteral("%c"), kli18n("Command and arguments: %c")},
    {QStringLiteral("%w"), kli18n("Window Title Set by Shell: %w")},
    {QStringLiteral("%#"), kli18n("Session Number: %#")},
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
        auto *action = new QAction(array[i].description.toString(), this);
        action->setData(array[i].element);
        menuActions << action;
    }

    menu()->addActions(menuActions);
}

Session::TabTitleContext TabTitleFormatButton::context() const
{
    return _context;
}

#include "moc_TabTitleFormatButton.cpp"
