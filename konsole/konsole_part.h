/*
    This file is part of the KDE system
    Copyright (C)  1999,2000 Boloni Laszlo

    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
    Boston, MA 02111-1307, USA.

    gofai.cpp - Good Old Fashioned Artificial Intelligence With a Modern Twist
 */

#ifndef __KONSOLE_PART_H__
#define __KONSOLE_PART_H__

#include <kparts/browserextension.h>
#include <kparts/factory.h>
#include <kaction.h>
#include <konsole.h>

class KInstance;
class konsoleBrowserExtension;
class QLabel;
class TESession;

class konsoleFactory : public KParts::Factory
{
    Q_OBJECT
public:
    konsoleFactory();
    virtual ~konsoleFactory();

    virtual KParts::Part* createPart(QWidget *parentWidget = 0, const char *widgetName = 0,
                                     QObject* parent = 0, const char* name = 0,
                                     const char* classname = "KParts::Part",
                                     const QStringList &args = QStringList());

    static KInstance *instance();

 private:
    static KInstance *s_instance;
    static KAboutData *s_aboutData;
};

class konsolePart: public KParts::ReadOnlyPart
{
    Q_OBJECT
	public:
    konsolePart(QWidget *parentWidget, const char *widgetName, QObject * parent, const char *name);
    virtual ~konsolePart();

 protected:
    virtual bool openURL( const KURL & url );
    virtual bool openFile() {return false;} // never used
    virtual bool closeURL();

 protected slots:
      void slotNew();
      void slotSaveFile();
      void slotLoadFile();

      void doneSession(TESession*,int);
      void sessionDestroyed();
      void configureRequest(TEWidget*,int,int x,int y);

 private:
    QLabel *widget;
    //    Konsole *kons;
    TEWidget *te;
    TESession *initial;
    konsoleBrowserExtension *m_extension;
    /*
    KAction *m_NewAction;
    KAction *m_SaveAction;
    KAction *m_LoadAction;
    KAction *m_TestDialogAction;
    KAction *m_ZoomInAction;
    KAction *m_ZoomOutAction;
    */
};

class konsoleBrowserExtension : public KParts::BrowserExtension
{
    Q_OBJECT
	friend class konsolePart;
 public:
    konsoleBrowserExtension(konsolePart *parent);
    virtual ~konsoleBrowserExtension();
};

#endif
