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
#include <klibloader.h>
#include <kaction.h>
#include <konsole.h>

class KInstance;
class konsoleBrowserExtension;
class QLabel;

class konsoleFactory : public KLibFactory
{
    Q_OBJECT
	public:
    konsoleFactory();
    virtual ~konsoleFactory();
    
    virtual QObject* create(QObject* parent = 0, const char* name = 0,
			    const char* classname = "QObject",
			    const QStringList &args = QStringList());
    
    static KInstance *instance();
    
 private:
    static KInstance *s_instance;
};

class konsolePart: public KParts::ReadOnlyPart
{
    Q_OBJECT
	public:
    konsolePart(QWidget *parent, const char *name);
    virtual ~konsolePart();
    
 protected:
    virtual bool openFile();
    
 protected slots:
      void slotNew();
      void slotSaveFile();
      void slotLoadFile();

 private:
    QLabel *widget;
    //    Konsole *kons;
    TEWidget *te;
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
