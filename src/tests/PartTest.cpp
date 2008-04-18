/*
    Copyright 2008 by Robert Knight <robertknight@gmail.com>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
    02110-1301  USA.
*/

// Own
#include "PartTest.h"

// Qt
#include <QtGui/QWidget>
#include <QtGui/QLabel>
#include <QtGui/QVBoxLayout>
#include <QtCore/QCoreApplication>

// System
#include <termios.h>
#include <sys/types.h>

// KDE
#include <KPluginLoader>
#include <KPluginFactory>
#include <KParts/Part>
#include <KPtyProcess>
#include <KPtyDevice>
#include <KDialog>
#include <qtest_kde.h>

// Konsole
#include "../Pty.h"
#include "../Session.h"
#include "../KeyboardTranslator.h"

using namespace Konsole;

void PartTest::testFd()
{
	// start a pty process
	KPtyProcess ptyProcess;
	ptyProcess.setProgram("/bin/ping",QStringList() << "localhost");
	ptyProcess.setPtyChannels(KPtyProcess::AllChannels);
	ptyProcess.start();
	QVERIFY(ptyProcess.waitForStarted());

	int fd = ptyProcess.pty()->masterFd();

	// create a Konsole part and attempt to connect to it
	KParts::Part* terminalPart = createPart();
	bool result = QMetaObject::invokeMethod(terminalPart,"openTeletype",
						Qt::DirectConnection,Q_ARG(int,fd));
	QVERIFY(result);

		
	// suspend the KPtyDevice so that the embedded terminal gets a chance to 
	// read from the pty.  Otherwise the KPtyDevice will simply read everything
	// as soon as it becomes available and the terminal will not display any output
	ptyProcess.pty()->setSuspended(true);
	
	KDialog* dialog = new KDialog();
	dialog->setButtons(0);
	QVBoxLayout* layout = new QVBoxLayout(dialog->mainWidget());
	layout->addWidget(new QLabel("Output of 'ping localhost' should appear in a terminal below for 5 seconds"));
	layout->addWidget(terminalPart->widget());
	QTimer::singleShot(5000,dialog,SLOT(close()));
	dialog->exec();

	delete terminalPart;
	delete dialog;
	ptyProcess.kill();
	ptyProcess.waitForFinished(1000);
}
KParts::Part* PartTest::createPart()
{
	KPluginLoader loader("libkonsolepart");
	KPluginFactory* factory = loader.factory();
	Q_ASSERT(factory);

	KParts::Part* terminalPart = factory->create<KParts::Part>(this);
	
	return terminalPart;
}

QTEST_KDEMAIN( PartTest , GUI )

#include "PartTest.moc"

