/***************************************************************************
                          schemaeditor.h  -  description
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

#ifndef SCHEMAEDITOR_H
#define SCHEMAEDITOR_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <kapplication.h>
#include <qwidget.h>
//Added by qt3to4:
#include <QPixmap>
#include <Q3MemArray>
class QPixmap;
class KSharedPixmap;

#include "schemadialog.h"

/** SchemaEditor is the base class of the porject */
class SchemaEditor : public SchemaDialog
{
  Q_OBJECT 
  public:
    /** constructor */
    SchemaEditor(QWidget* parent=0, const char *name=0);
    /** destructor */
    ~SchemaEditor();

    QString schema();
    void setSchema(QString);
    bool isModified() const { return schMod; }
    void querySave();

  signals:
  	void changed();
	void schemaListChanged(const QStringList &titles, const QStringList &filenames);

  public slots:
  	void slotColorChanged(int);
  	void imageSelect();  	
	void slotTypeChanged(int);
	void readSchema(int);
	void saveCurrent();
	void removeCurrent();
	void previewLoaded(bool l);		
	void getList();
  private slots:
	void show();
	void schemaModified();
	void loadAllSchema(QString currentFile="");
	void updatePreview();
  private:
	bool schMod;
  	Q3MemArray<QColor> color;
	Q3MemArray<int> type; // 0= custom, 1= sysfg, 2=sysbg, 3=rcolor
	Q3MemArray<bool> transparent;
	Q3MemArray<bool> bold;
	QPixmap pix;
	KSharedPixmap *spix;
	QString defaultSchema;	
	bool loaded;
	bool schemaLoaded;
	bool change;
	int oldSchema;
	int oldSlot;
	QString readSchemaTitle(const QString& filename);
	void schemaListChanged();

};

#endif
