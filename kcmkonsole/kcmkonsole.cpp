/***************************************************************************
                          kcmkonsole.cpp - control module for konsole
                             -------------------
    begin                : mar apr 17 16:44:59 CEST 2001
    copyright            : (C) 2001 by Andrea Rizzi
    email                : rizzi@kde.org
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include <qcheckbox.h>
#include <qlayout.h>
#include <qstringlist.h>
#include <qtabwidget.h>
//Added by qt3to4:
#include <QVBoxLayout>

#include <dcopclient.h>

#include <kaboutdata.h>
#include <kconfig.h>
#include <kdebug.h>
#include <kfontdialog.h>
#include <kgenericfactory.h>
#include <kmessagebox.h>

#include "schemaeditor.h"
#include "sessioneditor.h"
#include "kcmkonsole.h"

typedef KGenericFactory<KCMKonsole, QWidget> ModuleFactory;
K_EXPORT_COMPONENT_FACTORY( kcm_konsole, ModuleFactory("kcmkonsole") )

KCMKonsole::KCMKonsole(QWidget * parent, const char *name, const QStringList&)
:KCModule(ModuleFactory::instance(), parent)
{
    
    setQuickHelp( i18n("<h1>Konsole</h1> With this module you can configure Konsole, the KDE terminal"
		" application. You can configure the generic Konsole options (which can also be "
		"configured using the RMB) and you can edit the schemas and sessions "
		"available to Konsole."));

    QVBoxLayout *topLayout = new QVBoxLayout(this);
    dialog = new KCMKonsoleDialog(this);
    dialog->line_spacingSB->setRange(0, 8, 1, false);
    dialog->line_spacingSB->setSpecialValueText(i18n("normal line spacing", "Normal"));
    dialog->show();
    topLayout->add(dialog);
    load();

    KAboutData *ab=new KAboutData( "kcmkonsole", I18N_NOOP("KCM Konsole"),
       "0.2",I18N_NOOP("KControl module for Konsole configuration"), KAboutData::License_GPL,
       "(c) 2001, Andrea Rizzi", 0, 0, "rizzi@kde.org");

    ab->addAuthor("Andrea Rizzi",0, "rizzi@kde.org");
    setAboutData( ab );

    connect(dialog->terminalSizeHintCB,SIGNAL(toggled(bool)), SLOT( changed() ));
    connect(dialog->warnCB,SIGNAL(toggled(bool)), SLOT( changed() ));
    connect(dialog->ctrldragCB,SIGNAL(toggled(bool)), SLOT( changed() ));
    connect(dialog->cutToBeginningOfLineCB,SIGNAL(toggled(bool)), SLOT( changed() ));
    connect(dialog->allowResizeCB,SIGNAL(toggled(bool)), SLOT( changed() ));
    connect(dialog->bidiCB,SIGNAL(toggled(bool)), SLOT( changed() ));
    connect(dialog->xonXoffCB,SIGNAL(toggled(bool)), SLOT( changed() ));
    connect(dialog->blinkingCB,SIGNAL(toggled(bool)), SLOT( changed() ));
    connect(dialog->frameCB,SIGNAL(toggled(bool)), SLOT( changed() ));
    connect(dialog->line_spacingSB,SIGNAL(valueChanged(int)), SLOT( changed() ));
    connect(dialog->matchTabWinTitleCB,SIGNAL(toggled(bool)), SLOT( changed() ));
    connect(dialog->silence_secondsSB,SIGNAL(valueChanged(int)), SLOT( changed() ));
    connect(dialog->word_connectorLE,SIGNAL(textChanged(const QString &)), SLOT( changed() ));
    connect(dialog->SchemaEditor1, SIGNAL(changed()), SLOT( changed() ));
    connect(dialog->SessionEditor1, SIGNAL(changed()), SLOT( changed() ));
    connect(dialog->SchemaEditor1, SIGNAL(schemaListChanged(const QStringList &,const QStringList &)),
            dialog->SessionEditor1, SLOT(schemaListChanged(const QStringList &,const QStringList &)));
    connect(dialog->SessionEditor1, SIGNAL(getList()), dialog->SchemaEditor1, SLOT(getList()));
}

void KCMKonsole::load()
{
    load(false);
}

void KCMKonsole::load(bool useDefaults)
{
    KConfig config("konsolerc", true);
    config.setDesktopGroup();
    config.setReadDefaults(useDefaults);

    dialog->terminalSizeHintCB->setChecked(config.readBoolEntry("TerminalSizeHint",false));
    bidiOrig = config.readBoolEntry("EnableBidi",false);
    dialog->bidiCB->setChecked(bidiOrig);
    dialog->matchTabWinTitleCB->setChecked(config.readBoolEntry("MatchTabWinTitle",false));
    dialog->warnCB->setChecked(config.readBoolEntry("WarnQuit",true));
    dialog->ctrldragCB->setChecked(config.readBoolEntry("CtrlDrag",true));
    dialog->cutToBeginningOfLineCB->setChecked(config.readBoolEntry("CutToBeginningOfLine",false));
    dialog->allowResizeCB->setChecked(config.readBoolEntry("AllowResize",false));
    xonXoffOrig = config.readBoolEntry("XonXoff",false);
    dialog->xonXoffCB->setChecked(xonXoffOrig);
    dialog->blinkingCB->setChecked(config.readBoolEntry("BlinkingCursor",false));
    dialog->frameCB->setChecked(config.readBoolEntry("has frame",true));
    dialog->line_spacingSB->setValue(config.readUnsignedNumEntry( "LineSpacing", 0 ));
    dialog->silence_secondsSB->setValue(config.readUnsignedNumEntry( "SilenceSeconds", 10 ));
    dialog->word_connectorLE->setText(config.readEntry("wordseps",":@-./_~"));

    dialog->SchemaEditor1->setSchema(config.readEntry("schema"));

    emit changed(useDefaults);
}

void KCMKonsole::save()
{
    if (dialog->SchemaEditor1->isModified())
    {
       dialog->TabWidget2->showPage(dialog->tab_2);
       dialog->SchemaEditor1->querySave();
    }

    if (dialog->SessionEditor1->isModified())
    {
       dialog->TabWidget2->showPage(dialog->tab_3);
       dialog->SessionEditor1->querySave();
    }

    KConfig config("konsolerc");
    config.setDesktopGroup();

    config.writeEntry("TerminalSizeHint", dialog->terminalSizeHintCB->isChecked());
    bool bidiNew = dialog->bidiCB->isChecked();
    config.writeEntry("EnableBidi", bidiNew);
    config.writeEntry("MatchTabWinTitle", dialog->matchTabWinTitleCB->isChecked());
    config.writeEntry("WarnQuit", dialog->warnCB->isChecked());
    config.writeEntry("CtrlDrag", dialog->ctrldragCB->isChecked());
    config.writeEntry("CutToBeginningOfLine", dialog->cutToBeginningOfLineCB->isChecked());
    config.writeEntry("AllowResize", dialog->allowResizeCB->isChecked());
    bool xonXoffNew = dialog->xonXoffCB->isChecked();
    config.writeEntry("XonXoff", xonXoffNew);
    config.writeEntry("BlinkingCursor", dialog->blinkingCB->isChecked());
    config.writeEntry("has frame", dialog->frameCB->isChecked());
    config.writeEntry("LineSpacing" , dialog->line_spacingSB->value());
    config.writeEntry("SilenceSeconds" , dialog->silence_secondsSB->value());
    config.writeEntry("wordseps", dialog->word_connectorLE->text());

    config.writeEntry("schema", dialog->SchemaEditor1->schema());

    config.sync();

    emit changed(false);

    DCOPClient *dcc = kapp->dcopClient();
    dcc->send("konsole-*", "konsole", "reparseConfiguration()", QByteArray());
    dcc->send("kdesktop", "default", "configure()", QByteArray());
    dcc->send("klauncher", "klauncher", "reparseConfiguration()", QByteArray());

    if (xonXoffOrig != xonXoffNew)
    {
       xonXoffOrig = xonXoffNew;
       KMessageBox::information(this, i18n("The Ctrl+S/Ctrl+Q flow control setting will only affect "
                                           "newly started Konsole sessions.\n"
                                           "The 'stty' command can be used to change the flow control "
                                           "settings of existing Konsole sessions."));
    }

    if (bidiNew && !bidiOrig)
    {
       KMessageBox::information(this, i18n("You have chosen to enable "
                                           "bidirectional text rendering by "
                                           "default.\n"
                                           "Note that bidirectional text may "
                                           "not always be shown correctly, "
                                           "especially when selecting parts of "
                                           "text written right-to-left. This "
                                           "is a known issue which cannot be "
                                           "resolved at the moment due to the "
                                           "nature of text handling in "
                                           "console-based applications."));
    }
    bidiOrig = bidiNew;

}

void KCMKonsole::defaults()
{
    load(true);
}

#include "kcmkonsole.moc"
