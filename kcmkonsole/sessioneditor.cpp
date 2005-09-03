/***************************************************************************
                          sessioneditor.cpp  -  description
                             -------------------
    begin                : oct 28 2001
    copyright            : (C) 2001 by Stephan Binner
    email                : binner@kde.org
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "sessioneditor.h"
#include "sessioneditor.moc"

#include <qlineedit.h>
#include <qcombobox.h>
#include <kdebug.h>
#include <kstandarddirs.h>

#include <klocale.h>
#include <kfiledialog.h>
#include <kinputdialog.h>
#include <kicondialog.h>
#include <kmessagebox.h>
#include <kurlrequester.h>
#include <klineedit.h>
#include <kiconloader.h>
#include <krun.h>
#include <kshell.h>

// SessionListBoxText is a list box text item with session filename
class SessionListBoxText : public Q3ListBoxText
{
  public:
    SessionListBoxText(const QString &title, const QString &filename): Q3ListBoxText(title)
    {
      m_filename = filename;
    };

    const QString filename() { return m_filename; };

  private:
    QString m_filename;
};

SessionEditor::SessionEditor(QWidget * parent, const char *name)
:SessionDialog(parent, name)
{
  sesMod=false;
  oldSession=-1;
  loaded=false;

  KGlobal::locale()->insertCatalogue("konsole"); // For schema and keytab translations
  KGlobal::iconLoader()->addAppDir( "konsole" );

  directoryLine->setMode(KFile::Directory);
  connect(sessionList, SIGNAL(highlighted(int)), this, SLOT(readSession(int)));
  connect(saveButton, SIGNAL(clicked()), this, SLOT(saveCurrent()));
  connect(removeButton, SIGNAL(clicked()), this, SLOT(removeCurrent()));

  connect(nameLine, SIGNAL(textChanged(const QString&)), this, SLOT(sessionModified()));
  connect(directoryLine, SIGNAL(textChanged(const QString&)), this, SLOT(sessionModified()));
  connect(executeLine, SIGNAL(textChanged(const QString&)), this, SLOT(sessionModified()));
  connect(termLine, SIGNAL(textChanged(const QString&)), this, SLOT(sessionModified()));

  connect(previewIcon, SIGNAL(iconChanged(QString)), this, SLOT(sessionModified()));

  connect(fontCombo, SIGNAL(activated(int)), this, SLOT(sessionModified()));
  connect(keytabCombo, SIGNAL(activated(int)), this, SLOT(sessionModified()));
  connect(schemaCombo, SIGNAL(activated(int)), this, SLOT(sessionModified()));
}

SessionEditor::~SessionEditor()
{
    keytabFilename.setAutoDelete(true);
    schemaFilename.setAutoDelete(true);
}

void SessionEditor::show()
{
  removeButton->setEnabled(sessionList->count()>1);
  if (! loaded) {
    loadAllKeytab();
    loadAllSession();
    readSession(0);
    sessionList->setCurrentItem(0);
    loaded = true;
  }
  SessionDialog::show();
}

void SessionEditor::loadAllKeytab()
{
  QStringList lst = KGlobal::dirs()->findAllResources("data", "konsole/*.keytab");
  keytabCombo->clear();
  keytabFilename.clear();

  keytabCombo->insertItem(i18n("XTerm (XFree 4.x.x)"),0);
  keytabFilename.append(new QString(""));

  int i=1;
  for(QStringList::Iterator it = lst.begin(); it != lst.end(); ++it )
  {
    QString name = (*it);
    QString title = readKeymapTitle(name);

    name = name.section('/',-1);
    name = name.section('.',0);
    keytabFilename.append(new QString(name));

    if (title.isNull() || title.isEmpty())
      title=i18n("untitled");

    keytabCombo->insertItem(title,i);

    i++;
  }
}

QString SessionEditor::readKeymapTitle(const QString & file)
{
  QString fPath = locate("data", "konsole/" + file);

  if (fPath.isNull())
    fPath = locate("data", file);
  removeButton->setEnabled( QFileInfo (fPath).isWritable () );

  if (fPath.isNull())
    return 0;

  FILE *sysin = fopen(QFile::encodeName(fPath), "r");
  if (!sysin)
    return 0;

  char line[100];
  int len;
  while (fscanf(sysin, "%80[^\n]\n", line) > 0)
    if ((len = strlen(line)) > 8)
      if (!strncmp(line, "keyboard", 8)) {
	fclose(sysin);
        if(line[len-1] == '"')
          line[len-1] = '\000';
        QString temp;
        if(line[9] == '"')
          temp=i18n(line+10);
        else
          temp=i18n(line+9);
	return temp;
      }

  return 0;
}

void SessionEditor::loadAllSession(QString currentFile)
{
  QStringList list = KGlobal::dirs()->findAllResources("data", "konsole/*.desktop", false, true);
  sessionList->clear();

  Q3ListBoxItem* currentItem = 0;
  for (QStringList::ConstIterator it = list.begin(); it != list.end(); ++it) {

    QString name = (*it);

    KSimpleConfig* co = new KSimpleConfig(name,true);
    co->setDesktopGroup();
    QString sesname = co->readEntry("Name",i18n("Unnamed"));
    delete co;

    sessionList->insertItem(new SessionListBoxText(sesname, name));

    if (currentFile==name.section('/',-1))
      currentItem = sessionList->item( sessionList->count()-1 );
  }
  sessionList->sort();
  sessionList->setCurrentItem(0);  // select the first added item correctly too
  sessionList->setCurrentItem(currentItem);
  emit getList();
}

void SessionEditor::readSession(int num)
{
    int i,counter;
    QString str;
    KSimpleConfig* co;

    if(sesMod) {
        disconnect(sessionList, SIGNAL(highlighted(int)), this, SLOT(readSession(int)));

        sessionList->setCurrentItem(oldSession);
        querySave();
        sessionList->setCurrentItem(num);
        connect(sessionList, SIGNAL(highlighted(int)), this, SLOT(readSession(int)));
        sesMod=false;
    }
    if( sessionList->item(num) )
    {
        removeButton->setEnabled( QFileInfo ( ((SessionListBoxText *)sessionList->item(num))->filename() ).isWritable () );
        co = new KSimpleConfig( ((SessionListBoxText *)sessionList->item(num))->filename(),true);

        co->setDesktopGroup();
        str = co->readEntry("Name");
        nameLine->setText(str);

        str = co->readPathEntry("Cwd");
        directoryLine->lineEdit()->setText(str);

        str = co->readPathEntry("Exec");
        executeLine->setText(str);

        str = co->readEntry("Icon","konsole");
        previewIcon->setIcon(str);

        i = co->readUnsignedNumEntry("Font",(unsigned int)-1);
        fontCombo->setCurrentItem(i+1);

        str = co->readEntry("Term","xterm");
        termLine->setText(str);

        str = co->readEntry("KeyTab","");
        i=0;
        counter=0;
        for (QString *it = keytabFilename.first(); it != 0; it = keytabFilename.next()) {
            if (str == (*it))
                i = counter;
            counter++;
        }
        keytabCombo->setCurrentItem(i);

        str = co->readEntry("Schema","");
        i=0;
        counter=0;
        for (QString *it = schemaFilename.first(); it != 0; it = schemaFilename.next()) {
            if (str == (*it))
                i = counter;
            counter++;
        }
        schemaCombo->setCurrentItem(i);
        delete co;
    }
    sesMod=false;
    oldSession=num;
}

void SessionEditor::querySave()
{
    int result = KMessageBox::questionYesNo(this,
                         i18n("The session has been modified.\n"
			"Do you want to save the changes?"),
			i18n("Session Modified"),
			KStdGuiItem::save(),
			KStdGuiItem::discard());
    if (result == KMessageBox::Yes)
    {
        saveCurrent();
    }
}

void SessionEditor::schemaListChanged(const QStringList &titles, const QStringList &filenames)
{
  const QString text = schemaCombo->currentText();

  schemaCombo->clear();
  schemaFilename.clear();

  schemaCombo->insertItem(i18n("Konsole Default"),0);
  schemaFilename.append(new QString(""));

  schemaCombo->insertStringList(titles, 1);
  for (QStringList::const_iterator it = filenames.begin(); it != filenames.end(); ++it)
      schemaFilename.append(new QString(*it));

  // Restore current item
  int item = 0;
  for (int i = 0; i < schemaCombo->count(); i++)
      if (schemaCombo->text(i) == text) {
          item = i;
          break;
      }
  schemaCombo->setCurrentItem(item);
}

void SessionEditor::saveCurrent()
{
  // Verify Execute entry is valid; otherwise Konsole will ignore it.
  // This code is take from konsole.cpp; if you change one, change both.
  QString exec = executeLine->text();
  if ( exec.startsWith( "su -c \'" ) ) {
     exec = exec.mid( 7, exec.length() - 8 );
  }
  exec = KRun::binaryName( exec, false );
  exec = KShell::tildeExpand( exec );
  QString pexec = KGlobal::dirs()->findExe( exec );

  if ( pexec.isEmpty() )
  {
    int result = KMessageBox::warningContinueCancel( this,
            i18n( "The Execute entry is not a valid command.\n"
			"You can still save this session, but it will not show up in Konsole's Session list." ),
			i18n( "Invalid Execute Entry" ),
			KStdGuiItem::save() );
    if ( result != KMessageBox::Continue )
    {
        return;
    }

  }

  QString fullpath;
  if (sessionList->currentText() == nameLine->text()) {
    fullpath = ( ((SessionListBoxText *)sessionList->item( sessionList->currentItem() ))->filename() ).section('/',-1);
  }
  else {
    // Only ask for a name for changed nameLine, considered a "save as"
    fullpath = nameLine->text().stripWhiteSpace().simplifyWhiteSpace()+".desktop";

    bool ok;
    fullpath = KInputDialog::getText( i18n( "Save Session" ),
        i18n( "File name:" ), fullpath, &ok, this );
    if (!ok) return;
  }

  if (fullpath[0] != '/')
    fullpath = KGlobal::dirs()->saveLocation("data", "konsole/") + fullpath;

  KSimpleConfig* co = new KSimpleConfig(fullpath);
  co->setDesktopGroup();
  co->writeEntry("Type","KonsoleApplication");
  co->writeEntry("Name",nameLine->text());
  co->writePathEntry("Cwd",directoryLine->lineEdit()->text());
  co->writePathEntry("Exec",executeLine->text());
  co->writeEntry("Icon",previewIcon->icon());
  if (fontCombo->currentItem()==0)
    co->writeEntry("Font","");
  else
    co->writeEntry("Font",fontCombo->currentItem()-1);
  co->writeEntry("Term",termLine->text());
  co->writeEntry("KeyTab",*keytabFilename.at(keytabCombo->currentItem()));
  co->writeEntry("Schema",*schemaFilename.at(schemaCombo->currentItem()));
  co->sync();
  delete co;
  sesMod=false;
  loadAllSession(fullpath.section('/',-1));
  removeButton->setEnabled(sessionList->count()>1);
}

void SessionEditor::removeCurrent()
{
  QString base = ((SessionListBoxText *)sessionList->item( sessionList->currentItem() ))->filename();

  // Query if system sessions should be removed
  if (locateLocal("data", "konsole/" + base.section('/', -1)) != base) {
    int code = KMessageBox::warningContinueCancel(this,
      i18n("You are trying to remove a system session. Are you sure?"),
      i18n("Removing System Session"),
      KGuiItem(i18n("&Delete"),"editdelete"));
    if (code != KMessageBox::Continue)
      return;
  }

  if (!QFile::remove(base)) {
    KMessageBox::error(this,
      i18n("Cannot remove the session.\nMaybe it is a system session.\n"),
      i18n("Error Removing Session"));
    return;
  }
  removeButton->setEnabled(sessionList->count()>1);
  loadAllSession();
  readSession(0);
  sessionList->setCurrentItem(0);
}

void SessionEditor::sessionModified()
{
  saveButton->setEnabled(nameLine->text().length() != 0);
  sesMod=true;
  emit changed();
}
