/*
 *  This file is part of the KDE libraries
 *  Copyright (c) 2002 Michael Goffioul <kdeprint@swing.be>
 *  Copyright (c) 2003 Waldo Bastian <bastian@kde.org>
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License version 2 as published by the Free Software Foundation.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 **/

#ifndef PRINTSETTINGS_H
#define PRINTSETTINGS_H

#include <kdeprint/kprintdialogpage.h>

class QCheckBox;

class PrintSettings : public KPrintDialogPage
{
	Q_OBJECT
public:
	PrintSettings(QWidget *parent = 0, const char *name = 0);
	~PrintSettings();

	void getOptions(QMap<QString,QString>& opts, bool incldef = false);
	void setOptions(const QMap<QString,QString>& opts);

private:
	QCheckBox	*m_printfriendly;
	QCheckBox	*m_printexact;
	QCheckBox	*m_printheader;
};

#endif
