/*
 *  This file is part of the KDE libraries
 *  Copyright (c) 2002 Michael Goffioul <kdeprint@swing.be>
 *            (c) 2003 Waldo Bastian <bastian@kde.org>
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

#include "printsettings.h"

#include <klocale.h>
#include <qcheckbox.h>
#include <qlayout.h>

PrintSettings::PrintSettings(QWidget *parent, const char *name)
: KPrintDialogPage(parent, name)
{
	setTitle(i18n("Options"));

	m_printfriendly = new QCheckBox(i18n("Printer &friendly mode (black text, no background)"), this);
	m_printfriendly->setChecked(true);
	m_printexact = new QCheckBox(i18n("&Pixel for pixel"), this);
	m_printexact->setChecked(false);
	m_printheader = new QCheckBox(i18n("Print &header"), this);
	m_printheader->setChecked(true);

        m_printheader->hide(); // Not yet implemented.

	QVBoxLayout	*l0 = new QVBoxLayout(this, 0, 10);
	l0->addWidget(m_printfriendly);
	l0->addWidget(m_printexact);
	l0->addWidget(m_printheader);
	l0->addStretch(1);
}

PrintSettings::~PrintSettings()
{
}

void PrintSettings::getOptions(QMap<QString,QString>& opts, bool /*incldef*/)
{
	opts["app-konsole-printfriendly"] = (m_printfriendly->isChecked() ? "true" : "false");
	opts["app-konsole-printexact"] = (m_printexact->isChecked() ? "true" : "false");
	opts["app-konsole-printheader"] = (m_printheader->isChecked() ? "true" : "false");
}

void PrintSettings::setOptions(const QMap<QString,QString>& opts)
{
	m_printfriendly->setChecked(opts["app-konsole-printfriendly"] != "false");
	m_printexact->setChecked(opts["app-konsole-printexact"] == "true");
	m_printheader->setChecked(opts["app-konsole-printheader"] != "false");
}

#include "printsettings.moc"
