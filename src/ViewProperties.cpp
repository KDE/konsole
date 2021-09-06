/*
    SPDX-FileCopyrightText: 2007-2008 Robert Knight <robertknight@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

// Own
#include "ViewProperties.h"

using Konsole::ViewProperties;

QHash<int, ViewProperties *> ViewProperties::_viewProperties;

ViewProperties::ViewProperties(QObject *parent)
    : QObject(parent)
    , _icon(QIcon())
    , _title(QString())
    , _identifier(0)
{
}

ViewProperties::~ViewProperties()
{
    _viewProperties.remove(_identifier);
}

ViewProperties *ViewProperties::propertiesById(int id)
{
    return _viewProperties[id];
}

QUrl ViewProperties::url() const
{
    return QUrl();
}

QString ViewProperties::currentDir() const
{
    return QString();
}

void ViewProperties::fireActivity()
{
    Q_EMIT activity(this);
}

void ViewProperties::rename()
{
}

void ViewProperties::setTitle(const QString &title)
{
    if (title != _title) {
        _title = title;
        Q_EMIT titleChanged(this);
    }
}

void ViewProperties::setIcon(const QIcon &icon)
{
    // the icon's cache key is used to determine whether this icon is the same
    // as the old one.  if so no signal is emitted.

    if (icon.cacheKey() != _icon.cacheKey()) {
        _icon = icon;
        Q_EMIT iconChanged(this);
    }
}

void ViewProperties::setColor(const QColor &color)
{
    if (color != _color) {
        _color = color;
        Q_EMIT colorChanged(this);
    }
}

void ViewProperties::setIdentifier(int id)
{
    if (_viewProperties.contains(_identifier)) {
        _viewProperties.remove(_identifier);
    }

    _identifier = id;

    _viewProperties.insert(id, this);
}

QString ViewProperties::title() const
{
    return _title;
}

QIcon ViewProperties::icon() const
{
    return _icon;
}

int ViewProperties::identifier() const
{
    return _identifier;
}

QColor ViewProperties::color() const
{
    return _color;
}
